#include "ShellProjectPlayerController.h"

#include "Blueprint/UserWidget.h"
#include "Components/WidgetComponent.h"
#include "Engine/GameInstance.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Pawn.h"
#include "InputAction.h"
#include "InputMappingContext.h"

#include "Shell/Input/ShellInputStateCore.h"
#include "Shell/Input/ShellInputStateManager.h"
#include "Shell/Terminal/ShellQuickCommandHotkeys.h"
#include "Shell/Terminal/ShellSettings.h"
#include "Shell/Terminal/ShellSubsystem.h"
#include "Shell/Terminal/ShellTerminalWidget.h"
#include "Shell/WorldScreen/ShellFloatingQuickButton.h"
#include "ShellProjectCharacter.h"
#include "Shell/WorldScreen/ShellWorldScreen.h"

namespace
{
	/** 下一个呈现状态（0 -> 1 -> 0）。 */
	EShellPresentationState NextPresentationState(EShellPresentationState InState)
	{
		return static_cast<EShellPresentationState>((static_cast<uint8>(InState) + 1) % 2);
	}

	/**
	 * 修饰键校验（M5 热键/开关键共用）。
	 *
	 * ⚠️ 主键本身是修饰键时必须特殊处理：开关键默认 LeftAlt（无修饰），
	 * 玩家按下 LeftAlt 的瞬间 IsAltDown()==true，而 Chord.bAlt==false ——
	 * 若按"逐位相等"校验，主键是修饰键的组合键（"alt" / "ctrl" 单键）
	 * 会【永远无法触发】。正确语义：主键对应的那个修饰位视为已按下，
	 * 其余修饰位仍须逐位匹配（按 LeftAlt 时额外按住 Ctrl → 不匹配，拒绝）。
	 */
	bool MatchesModifiers(const FShellHotkeyChord& InChord, const FModifierKeysState& InMods)
	{
		const FKey& Key = InChord.Key;
		const bool bKeyIsCtrl  = (Key == EKeys::LeftControl  || Key == EKeys::RightControl);
		const bool bKeyIsAlt   = (Key == EKeys::LeftAlt      || Key == EKeys::RightAlt);
		const bool bKeyIsShift = (Key == EKeys::LeftShift    || Key == EKeys::RightShift);
		const bool bKeyIsCmd   = (Key == EKeys::LeftCommand || Key == EKeys::RightCommand);

		return InMods.IsControlDown() == (InChord.bCtrl || bKeyIsCtrl)
			&& InMods.IsAltDown()     == (InChord.bAlt || bKeyIsAlt)
			&& InMods.IsShiftDown()   == (InChord.bShift || bKeyIsShift)
			&& InMods.IsCommandDown() == (InChord.bCmd || bKeyIsCmd);
	}
}

void AShellProjectPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);
	if (!EnhancedInput)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ShellProject] InputComponent 不是 EnhancedInputComponent，Tab 终端开关未绑定"));
		return;
	}

	if (UInputAction* LoadedToggle = TerminalToggleAction.LoadSynchronous())
	{
		EnhancedInput->BindAction(LoadedToggle, ETriggerEvent::Started, this, &AShellProjectPlayerController::HandleTerminalToggle);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[ShellProject] IA_TerminalToggle 加载失败，Tab 终端开关未绑定"));
	}

	// ESC 流控（HostOwned 范式）：终端面板不消费 ESC（EShellFlowKeyMode），
	// 由本控制器统一处理。IA/IMC 运行时惰性构建（构造器内 NewObject 会崩溃，
	// 与角色/世界屏的运行时构建模式一致），映射挂到本地玩家子系统（BeginPlay）。
	EscapeUIAction = NewObject<UInputAction>(this, TEXT("ShellEscapeUIAction"));
	EscapeUIAction->ValueType = EInputActionValueType::Boolean;
	EscapeMappingContext = NewObject<UInputMappingContext>(this, TEXT("ShellEscapeMappingContext"));
	EscapeMappingContext->MapKey(EscapeUIAction, EKeys::Escape);
	EnhancedInput->BindAction(EscapeUIAction, ETriggerEvent::Started, this, &AShellProjectPlayerController::HandleEscapeUI);
}

void AShellProjectPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// 订阅终端面板捕获的宿主流控键（HostOwned 范式）：面板持有键盘焦点时
	// Tab/ESC 经此到达（不依赖游戏输入管道，PIE 内外行为一致）。
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UShellSubsystem* Shell = GI->GetSubsystem<UShellSubsystem>())
		{
			Shell->OnHostFlowKey.AddUniqueDynamic(this, &AShellProjectPlayerController::HandleHostFlowKey);

			// M5：快捷指令热键 —— 列表/绑定变化后重建 IMC（仅变更时调用，非每帧）。
			Shell->OnQuickCommandsChanged.AddUniqueDynamic(this, &AShellProjectPlayerController::HandleQuickCommandsChanged);
		}
	}

	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
		{
			if (UInputMappingContext* LoadedContext = ShellMappingContext.LoadSynchronous())
			{
				InputSubsystem->AddMappingContext(LoadedContext, 0);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("[ShellProject] IMC_Shell 加载失败，终端输入映射未挂载"));
			}

			if (EscapeMappingContext)
			{
				// 优先级 0：与 IMC_Shell 并存（不同动作，互不冲突）。
				InputSubsystem->AddMappingContext(EscapeMappingContext, 0);
			}
		}
	}

	// M5：全局菜单开关键 + 首次热键映射。
	BuildMenuToggleBinding();
	RebuildHotkeyMapping();
}

void AShellProjectPlayerController::SetShellUIFocus(bool bUIFocused)
{
	UShellInputStateManager* Mgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UShellInputStateManager>() : nullptr;
	if (!Mgr)
	{
		return;
	}

	if (bUIFocused)
	{
		// 有活跃世界屏打字面 → UiTyping（聚焦终端）；否则 UiBrowse（只显示 UI 不抢焦）。
		const bool bHasTypingSurface = IsActiveWorldScreenTyping();
		Mgr->RequestState(bHasTypingSurface ? Shell::InputState::UiTyping : Shell::InputState::UiBrowse);
	}
	else
	{
		Mgr->RequestState(Shell::InputState::Gameplay);
	}
}

void AShellProjectPlayerController::HandleTerminalToggle()
{
	// 游戏场景（角色）：循环呈现状态。
	if (Cast<AShellProjectCharacter>(GetPawn()))
	{
		CycleShellPresentation();
		return;
	}

	// 菜单（登录）场景：世界面片终端（AShellWorldScreenActor）是唯一登录入口，
	// 不再弹出全屏 HUD 终端（旧登录方式残留，会与世界屏叠加显示）。
	UE_LOG(LogTemp, Verbose, TEXT("[ShellProject] 菜单场景 Tab 已禁用（世界屏为唯一登录入口）"));
}

void AShellProjectPlayerController::HandleHostFlowKey(FName KeyName)
{
	if (KeyName == EKeys::Tab.GetFName())
	{
		HandleTerminalToggle();
	}
	else if (KeyName == EKeys::Escape.GetFName())
	{
		HandleEscapeUI();
	}
}

void AShellProjectPlayerController::HandleEscapeUI()
{
	// HostOwned 流控键范式：ESC = 退出输入接管态。
	// 仅 InputWindow 态有"UI 输入态"可退（回默认 HeldInHand：世界屏隐藏输入接管、
	// 恢复 GameOnly + 光标隐藏，由 ApplyShellPresentation 统一落地）。
	// 菜单（登录）场景走本函数但故意空操作：世界屏即登录 UI 本体，
	// 没有"可退出"的输入态（Shell 常规约定 ESC 也无操作，清行归 Ctrl+C）。
	// 若未来要给登录页 ESC 加语义（如清空当前输入行），在此处按场景分支。
	if (PresentationState == EShellPresentationState::InputWindow)
	{
		SetShellPresentationState(static_cast<int32>(EShellPresentationState::HeldInHand));
	}
}

void AShellProjectPlayerController::CycleShellPresentation()
{
	PresentationState = NextPresentationState(PresentationState);
	ApplyShellPresentation();
}

void AShellProjectPlayerController::SetShellPresentationState(int32 InState)
{
	const int32 Clamped = FMath::Clamp(InState, 0, 1);
	PresentationState = static_cast<EShellPresentationState>(Clamped);
	ApplyShellPresentation();
}

void AShellProjectPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	// 进入游戏场景（手持角色）即应用默认态：HeldInHand（手持面板）。
	if (Cast<AShellProjectCharacter>(InPawn))
	{
		ApplyShellPresentation();
	}
}

UShellWorldScreen* AShellProjectPlayerController::GetWorldScreenOrNull() const
{
	const AShellProjectCharacter* Char = Cast<AShellProjectCharacter>(GetPawn());
	return Char ? Char->GetWorldScreen() : nullptr;
}

bool AShellProjectPlayerController::IsActiveWorldScreenTyping() const
{
	const UShellWorldScreen* Screen = GetWorldScreenOrNull();
	return Screen != nullptr
		&& Screen->IsInputActive()
		&& Screen->GetScreenComponent() != nullptr
		&& Screen->GetScreenComponent()->IsVisible()
		&& Screen->GetTerminalWidget() != nullptr;
}

void AShellProjectPlayerController::ApplyShellPresentation()
{
	// 世界屏是唯一呈现载体（HUD 视口态已移除，仅保留手持两态）。
	UShellWorldScreen* WorldScreen = GetWorldScreenOrNull();
	AShellProjectCharacter* Char = Cast<AShellProjectCharacter>(GetPawn());

	// 输入接管统一由世界屏组件持有：仅 InputWindow 态激活
	//（左键/滚轮转发到面片 + 置位标志让角色忽略移动/视角）。
	if (WorldScreen)
	{
		WorldScreen->SetInputActive(PresentationState == EShellPresentationState::InputWindow);
	}

	// M5：悬浮按钮只在 InputWindow 态可交互 —— 手持态面片只有 70x44cm，
	// 悬浮按钮物理尺寸过小难以命中，且手持态不需要快捷指令。
	if (UShellFloatingQuickButton* Button = GetFloatingQuickButtonOrNull())
	{
		Button->SetInteractionActive(PresentationState == EShellPresentationState::InputWindow);
	}

	switch (PresentationState)
	{
	case EShellPresentationState::HeldInHand:
	{
		// 手持面板：显示世界屏实例，姿态=手持（前/左/下/小）。
		if (WorldScreen)
		{
			WorldScreen->SetScreenVisible(true);
			if (Char)
			{
				Char->SetShellScreenPose(EShellScreenPose::Hand);
			}
		}
		break;
	}
	case EShellPresentationState::InputWindow:
	{
		// 输入目标：前面大窗口 + 可输入，姿态=面前（近/居中/大）。
		// 焦点不再在此直接 FocusTerminal：由 UShellInputStateManager 的
		// UiTyping 态按"活动打字面"解析焦点目标（终端就绪后统一断言）。
		if (WorldScreen)
		{
			WorldScreen->SetScreenVisible(true);
			if (Char)
			{
				Char->SetShellScreenPose(EShellScreenPose::Front);
			}
		}
		break;
	}
	default:
	{
		break;
	}
	}

	// 输入状态交由 UShellInputStateManager 声明式裁决：
	// 面前态 UiTyping（焦点进终端），其余态 Gameplay（纯游戏输入）。
	// 不走 SetShellUIFocus —— 它是 UiBrowse 兜底的薄映射，呈现态切换必须明确 UiTyping/Gameplay。
	if (UShellInputStateManager* Mgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UShellInputStateManager>() : nullptr)
	{
		Mgr->RequestState(PresentationState == EShellPresentationState::InputWindow ? Shell::InputState::UiTyping : Shell::InputState::Gameplay);
	}
}

// --- M5：快捷指令全局热键 + 悬浮菜单开关键 --------------------------------------

UShellFloatingQuickButton* AShellProjectPlayerController::GetFloatingQuickButtonOrNull() const
{
	const AShellProjectCharacter* Char = Cast<AShellProjectCharacter>(GetPawn());
	return Char ? Char->GetFloatingQuickButton() : nullptr;
}

void AShellProjectPlayerController::HandleQuickCommandsChanged()
{
	// 绑定可能已变化（qcmd bind / 商店安装 / 后续 GUI 编辑）→ 全量重建 IMC。
	// 只在广播时调用，非每帧。
	RebuildHotkeyMapping();
}

void AShellProjectPlayerController::RebuildHotkeyMapping()
{
	const ULocalPlayer* LP = GetLocalPlayer();
	UEnhancedInputLocalPlayerSubsystem* Subsystem =
		LP ? LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	if (!Subsystem)
	{
		return;
	}

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);
	if (!EnhancedInput)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ShellProject] InputComponent 不是 EnhancedInputComponent，快捷指令热键未绑定"));
		return;
	}

	// 全量重建：Remove 旧上下文 + 重建 IMC（量级 = 快捷指令条数，≤99，开销可忽略）。
	if (QuickHotkeyMappingContext)
	{
		Subsystem->RemoveMappingContext(QuickHotkeyMappingContext);
		QuickHotkeyMappingContext = nullptr;
	}
	QuickHotkeyActions.Reset();

	// lambda 绑定挂在 InputComponent 上，不随 IMC 移除而失效；
	// 先按句柄清掉上一轮绑定，否则每次广播都累积一批死绑定。
	for (const uint32 Handle : QuickHotkeyBindingHandles)
	{
		EnhancedInput->RemoveBindingByHandle(Handle);
	}
	QuickHotkeyBindingHandles.Reset();

	UShellSubsystem* Shell = GetGameInstance() ? GetGameInstance()->GetSubsystem<UShellSubsystem>() : nullptr;
	if (!Shell)
	{
		return;
	}

	const TArray<FShellHotkeyBinding> Bindings = Shell->GetEffectiveHotkeys();
	if (Bindings.Num() == 0)
	{
		return;
	}

	QuickHotkeyMappingContext = NewObject<UInputMappingContext>(this, TEXT("ShellQuickHotkeyMapping"));
	for (int32 i = 0; i < Bindings.Num(); ++i)
	{
		const FShellHotkeyChord& Chord = Bindings[i].Chord;
		if (!Chord.IsBound())
		{
			continue;
		}

		UInputAction* Action = NewObject<UInputAction>(this, *FString::Printf(TEXT("ShellQuickHotkey%d"), i));
		Action->ValueType = EInputActionValueType::Boolean;
		QuickHotkeyActions.Add(Action);
		QuickHotkeyMappingContext->MapKey(Action, Chord.Key);

		// 取舍说明：IMC 只绑主键，修饰键在回调里按 Slate 的真实修饰键状态校验。
		// EnhancedInput 的组合键要走 UInputTriggerChordAction（多修饰键还需
		// 修饰键 action 链），复杂度与收益不成比例；裸键抢字的风险由
		// bRequireModifierForHotkey 在绑定层挡掉。
		const FShellHotkeyChord Captured = Chord;
		const FEnhancedInputActionEventBinding& Binding = EnhancedInput->BindActionValueLambda(Action, ETriggerEvent::Started,
			[this, Captured](const FInputActionValue&)
			{
				HandleQuickCommandHotkey(Captured);
			});
		QuickHotkeyBindingHandles.Add(Binding.GetHandle());
	}

	// 优先级 3：高于世界屏指针映射（默认 2），保证热键不被屏幕输入遮蔽。
	Subsystem->AddMappingContext(QuickHotkeyMappingContext, 3);
}

void AShellProjectPlayerController::HandleQuickCommandHotkey(const FShellHotkeyChord& InChord)
{
	// 修饰键校验：IMC 只绑了主键，这里必须核对真实修饰键状态，
	// 否则裸 K 会误触发绑定了 Ctrl+K 的指令。
	// 注意 MatchesModifiers 对"主键本身是修饰键"（如单键 LeftAlt 开关键）的豁免。
	const FModifierKeysState Mods = FSlateApplication::Get().GetModifierKeys();
	if (!MatchesModifiers(InChord, Mods))
	{
		return;
	}

	UShellSubsystem* Shell = GetGameInstance() ? GetGameInstance()->GetSubsystem<UShellSubsystem>() : nullptr;
	if (!Shell)
	{
		return;
	}

	// 双管道之一（终端未聚焦时可达）；面板聚焦时由 OnPreviewKeyDown 先消费。
	FString HitId;
	Shell->TryConsumeHotkey(InChord.Key, InChord.bCtrl, InChord.bAlt, InChord.bShift, InChord.bCmd, HitId);
}

void AShellProjectPlayerController::BuildMenuToggleBinding()
{
	if (MenuToggleMappingContext)
	{
		return; // 幂等：一次构建即可
	}

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);
	if (!EnhancedInput)
	{
		return;
	}

	const UShellSettings* Settings = UShellSettings::Get();
	const FShellHotkeyChord ToggleChord = Settings ? Settings->FloatingMenuToggleChord : FShellHotkeyChord();
	if (!ToggleChord.IsBound())
	{
		return;
	}

	MenuToggleAction = NewObject<UInputAction>(this, TEXT("ShellMenuToggleAction"));
	MenuToggleAction->ValueType = EInputActionValueType::Boolean;

	MenuToggleMappingContext = NewObject<UInputMappingContext>(this, TEXT("ShellMenuToggleMapping"));
	MenuToggleMappingContext->MapKey(MenuToggleAction, ToggleChord.Key);

	const FShellHotkeyChord Captured = ToggleChord;
	EnhancedInput->BindActionValueLambda(MenuToggleAction, ETriggerEvent::Started,
		[this, Captured](const FInputActionValue&)
		{
			const FModifierKeysState Mods = FSlateApplication::Get().GetModifierKeys();
			if (!MatchesModifiers(Captured, Mods))
			{
				return;
			}
			if (UShellFloatingQuickButton* Button = GetFloatingQuickButtonOrNull())
			{
				Button->ToggleMenu();
			}
		});

	if (const ULocalPlayer* LP = GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			// 优先级 3：与热键 IMC 同级（不同主键，互不冲突）。
			Subsystem->AddMappingContext(MenuToggleMappingContext, 3);
		}
	}
}
