#include "ShellProjectPlayerController.h"

#include "Blueprint/UserWidget.h"
#include "Components/WidgetComponent.h"
#include "Engine/GameInstance.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/Pawn.h"
#include "InputAction.h"
#include "InputMappingContext.h"

#include "Shell/Terminal/ShellSubsystem.h"
#include "Shell/Terminal/ShellTerminalWidget.h"
#include "ShellProjectCharacter.h"

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

	TerminalToggleAction = LoadObject<UInputAction>(nullptr, TEXT("/Shell_UE/Shell/Input/IA_TerminalToggle.IA_TerminalToggle"));
	if (TerminalToggleAction)
	{
		EnhancedInput->BindAction(TerminalToggleAction, ETriggerEvent::Started, this, &AShellProjectPlayerController::HandleTerminalToggle);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[ShellProject] IA_TerminalToggle 加载失败，Tab 终端开关未绑定"));
	}
}

void AShellProjectPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
		{
			ShellMappingContext = LoadObject<UInputMappingContext>(nullptr, TEXT("/Shell_UE/Shell/Input/IMC_Shell.IMC_Shell"));
			if (ShellMappingContext)
			{
				InputSubsystem->AddMappingContext(ShellMappingContext, 0);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("[ShellProject] IMC_Shell 加载失败，终端输入映射未挂载"));
			}
		}
	}
}

void AShellProjectPlayerController::HandleTerminalToggle()
{
	// T12 前缀：双实例三态循环。仅当当前 Pawn 是游戏角色（游戏场景）时循环
	// 呈现状态；菜单（登录）场景仍走子系统 ToggleTerminal 原行为。
	if (Cast<AShellProjectCharacter>(GetPawn()))
	{
		CycleShellPresentation();
		return;
	}

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UShellSubsystem* Shell = GI->GetSubsystem<UShellSubsystem>())
		{
			Shell->ToggleTerminal(this);
		}
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

UWidgetComponent* AShellProjectPlayerController::GetShellScreenComponentOrNull() const
{
	return Cast<AShellProjectCharacter>(GetPawn()) ? Cast<AShellProjectCharacter>(GetPawn())->GetShellScreenComponent() : nullptr;
}

void AShellProjectPlayerController::ApplyShellPresentation()
{
	UShellTerminalWidget* Hud = EnsureHudShellWidget();
	UWidgetComponent* WorldScreen = GetShellScreenComponentOrNull();

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
			WorldScreen->SetVisibility(false);
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
			WorldScreen->SetVisibility(false);
		}
		break;
	}
	case EShellPresentationState::HeldInHand:
	{
		// 倾斜如拿在手里：显示世界 WidgetComponent 实例（角色 Tick 内面向相机 billboard）。
		Hud->SetVisibility(ESlateVisibility::Collapsed);
		if (WorldScreen)
		{
			WorldScreen->SetVisibility(true);
		}
		break;
	}
	case EShellPresentationState::InputWindow:
	{
		// 居中大窗口 + 可输入：取消缩小/下沉，居中铺 80%，切换输入模式让玩家打字。
		Hud->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		Hud->SetRenderScale(FVector2D(0.8f, 0.8f));
		Hud->SetRenderTranslation(FVector2D::ZeroVector);
		Hud->SetVisibility(ESlateVisibility::Visible);
		if (WorldScreen)
		{
			WorldScreen->SetVisibility(false);
		}
		break;
	}
	default:
	{
		break;
	}
	}

	// 仅 InputWindow 切 UIOnly（可输入）；其余呈现态保持 GameOnly（默认控制视角）。
	if (PresentationState == EShellPresentationState::InputWindow)
	{
		FInputModeUIOnly InputMode;
		if (Hud)
		{
			InputMode.SetWidgetToFocus(Hud->GetFocusTarget().ToSharedPtr());
		}
		SetInputMode(InputMode);
		SetShowMouseCursor(true);
	}
	else
	{
		SetInputMode(FInputModeGameOnly());
		SetShowMouseCursor(false);
	}
}
