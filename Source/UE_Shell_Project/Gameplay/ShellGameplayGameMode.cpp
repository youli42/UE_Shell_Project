#include "ShellGameplayGameMode.h"

#include "Blueprint/UserWidget.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Shell/Terminal/ShellSessionHUD.h"
#include "Shell/Terminal/ShellSessionSubsystem.h"
#include "Shell/Terminal/ShellSettings.h"
#include "TimerManager.h"

#include "ShellProjectCharacter.h"
#include "ShellProjectPlayerController.h"
#include "ShellProjectPlayerState.h"

AShellGameplayGameMode::AShellGameplayGameMode()
{
	DefaultPawnClass = AShellProjectCharacter::StaticClass();
	PlayerControllerClass = AShellProjectPlayerController::StaticClass();
	// ASC 挂在自定义 PlayerState 上 —— 不设置则用默认 APlayerState，
	// cast/attributes 会报 "no AbilitySystemComponent"。
	PlayerStateClass = AShellProjectPlayerState::StaticClass();
}

void AShellGameplayGameMode::BeginPlay()
{
	Super::BeginPlay();
	GetWorldTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateUObject(this, &AShellGameplayGameMode::ValidateSessionAndSetupHUD));
}

void AShellGameplayGameMode::ValidateSessionAndSetupHUD()
{
	UGameInstance* GameInstance = GetGameInstance();
	UShellSessionSubsystem* Session = GameInstance ? GameInstance->GetSubsystem<UShellSessionSubsystem>() : nullptr;
	APlayerController* PC = GameInstance ? GameInstance->GetFirstLocalPlayerController() : nullptr;
	if (!GameInstance || !PC)
	{
		return;
	}

	if (!Session || !Session->IsLoggedIn())
	{
		// 兜底守卫：未登录却到达 Gameplay（如 open 命令白名单被绕过）→ 弹回菜单。
		const UShellSettings* Settings = UShellSettings::Get();
		const FString MenuPath = Settings ? Settings->MenuLevelPath.ToString() : FString();
		if (!MenuPath.IsEmpty())
		{
			PC->ClientTravel(MenuPath, TRAVEL_Absolute);
		}
		return;
	}

	SessionHUD = CreateWidget<UShellSessionHUD>(PC);
	if (SessionHUD)
	{
		SessionHUD->AddToViewport(10);
	}

	// 输入模式/光标策略统一由 AShellProjectPlayerController 拥有：
	// OnPossess → ApplyShellPresentation（默认 Shrink 态）即 GameOnly + 隐藏光标。
}
