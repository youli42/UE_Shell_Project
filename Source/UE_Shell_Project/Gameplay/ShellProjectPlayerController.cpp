#include "ShellProjectPlayerController.h"

#include "Blueprint/UserWidget.h"
#include "Engine/GameInstance.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/Pawn.h"
#include "InputAction.h"
#include "InputMappingContext.h"

#include "Shell/Terminal/ShellSubsystem.h"
#include "Shell/Terminal/ShellTerminalWidget.h"
#include "ShellProjectCharacter.h"
#include "ShellWorldScreen.h"

namespace
{
	/** 下一个呈现状态（0 -> 1 -> 2 -> 3 -> 0）。 */
	EShellPresentationState NextPresentationState(EShellPresentationState InState)
	{
		return static_cast<EShellPresentationState>((static_cast<uint8>(InState) + 1) % 4);
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
}

void AShellProjectPlayerController::SetShellUIFocus(bool bUIFocused)
{
	if (bUIFocused)
	{
		// GameAndUI + 按住左键时不隐藏光标：默认构造的 bHideCursorDuringCapture=true
		// 会让按下瞬间视口捕获并隐藏光标，世界面片上的点击体验不像普通 UI。
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		SetInputMode(InputMode);
		SetShowMouseCursor(true);
	}
	else
	{
		SetInputMode(FInputModeGameOnly());
		SetShowMouseCursor(false);
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

void AShellProjectPlayerController::HandleEscapeUI()
{
	// HostOwned 流控键范式：ESC = 退出输入接管态。
	// 仅 InputWindow 态有"UI 输入态"可退（回默认 Shrink：世界屏隐藏输入接管、
	// 恢复 GameOnly + 光标隐藏，由 ApplyShellPresentation 统一落地）；
	// 其余状态无 UI 输入态，忽略。
	if (PresentationState == EShellPresentationState::InputWindow)
	{
		SetShellPresentationState(static_cast<int32>(EShellPresentationState::Shrink));
	}
}

void AShellProjectPlayerController::CycleShellPresentation()
{
	PresentationState = NextPresentationState(PresentationState);
	ApplyShellPresentation();
}

void AShellProjectPlayerController::SetShellPresentationState(int32 InState)
{
	const int32 Clamped = FMath::Clamp(InState, 0, 3);
	PresentationState = static_cast<EShellPresentationState>(Clamped);
	ApplyShellPresentation();
}

void AShellProjectPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	// 进入游戏场景（手持角色）即应用默认态：Shrink（左下角常显一部分）。
	if (Cast<AShellProjectCharacter>(InPawn))
	{
		ApplyShellPresentation();
	}
}

UShellTerminalWidget* AShellProjectPlayerController::EnsureHudShellWidget()
{
	if (HudShellWidget.Get() == nullptr)
	{
		HudShellWidget = CreateWidget<UShellTerminalWidget>(this);
		if (HudShellWidget.Get() != nullptr)
		{
			UE_LOG(LogTemp, Warning, TEXT("[ShellProject] Presentation HUD widget created"));
			// 让终端有内容：打印一条欢迎横幅（会广播给 HUD + 世界双实例）。
			if (UGameInstance* GI = GetGameInstance())
			{
				if (UShellSubsystem* Shell = GI->GetSubsystem<UShellSubsystem>())
				{
					Shell->Print(TEXT("shell presentation: type 'help' for commands"), EShellOutputType::System);
				}
			}
		}
	}
	return HudShellWidget.Get();
}

UShellWorldScreen* AShellProjectPlayerController::GetWorldScreenOrNull() const
{
	const AShellProjectCharacter* Char = Cast<AShellProjectCharacter>(GetPawn());
	return Char ? Char->GetWorldScreen() : nullptr;
}

void AShellProjectPlayerController::ApplyShellPresentation()
{
	UShellTerminalWidget* Hud = EnsureHudShellWidget();
	UShellWorldScreen* WorldScreen = GetWorldScreenOrNull();
	AShellProjectCharacter* Char = Cast<AShellProjectCharacter>(GetPawn());

	// 输入接管统一由世界屏组件持有：仅 InputWindow 态激活
	//（左键/滚轮转发到面片 + 置位标志让角色忽略移动/视角）。
	if (WorldScreen)
	{
		WorldScreen->SetInputActive(PresentationState == EShellPresentationState::InputWindow);
	}

	if (Hud)
	{
		// 只在首次加入视口时 add；后续切换只改变可见性与 RenderTransform。
		if (!Hud->IsInViewport())
		{
			Hud->AddToViewport(50);
		}
	}

	switch (PresentationState)
	{
	case EShellPresentationState::Shrink:
	{
		// 缩小显示画面：锚左下角缩小到一半，周围可见世界。
		Hud->SetRenderTransformPivot(FVector2D(0.f, 1.f));
		Hud->SetRenderScale(FVector2D(0.5f, 0.5f));
		Hud->SetRenderTranslation(FVector2D::ZeroVector);
		Hud->SetVisibility(ESlateVisibility::Visible);
		if (WorldScreen)
		{
			WorldScreen->SetScreenVisible(false);
		}
		break;
	}
	case EShellPresentationState::ShrinkOccluded:
	{
		// 缩小且下半部分被遮挡：在缩小基础上再下沉，使下半被视口底边"槽"盖住。
		Hud->SetRenderTransformPivot(FVector2D(0.f, 1.f));
		Hud->SetRenderScale(FVector2D(0.35f, 0.35f));
		Hud->SetRenderTranslation(FVector2D(0.f, 300.f));
		Hud->SetVisibility(ESlateVisibility::Visible);
		if (WorldScreen)
		{
			WorldScreen->SetScreenVisible(false);
		}
		break;
	}
	case EShellPresentationState::HeldInHand:
	{
		// 倾斜如拿在手里：显示世界屏实例，姿态=手持（前/左/下/小）。
		Hud->SetVisibility(ESlateVisibility::Collapsed);
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
		// 用世界屏，姿态=面前（近/居中/大，可输入），隐藏视口 HUD。
		Hud->SetVisibility(ESlateVisibility::Collapsed);
		if (WorldScreen)
		{
			WorldScreen->SetScreenVisible(true);
			if (Char)
			{
				Char->SetShellScreenPose(EShellScreenPose::Front);
			}
			// 聚焦终端输入框（组件内部延迟到下一帧，待 widget 窗口就绪）。
			WorldScreen->FocusTerminal();
		}
		break;
	}
	default:
	{
		break;
	}
	}

	// 输入模式/光标策略统一入口：面前态 GameAndUI + 光标（按住不隐藏），
	// 世界屏自包含指针输入处理点击/滚轮；其余态 GameOnly 纯游戏输入。
	SetShellUIFocus(PresentationState == EShellPresentationState::InputWindow);
}
