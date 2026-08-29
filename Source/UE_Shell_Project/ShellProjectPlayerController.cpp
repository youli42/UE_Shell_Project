#include "ShellProjectPlayerController.h"

#include "Engine/GameInstance.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Shell/Terminal/ShellSubsystem.h"

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
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UShellSubsystem* Shell = GameInstance->GetSubsystem<UShellSubsystem>())
		{
			Shell->ToggleTerminal(this);
		}
	}
}
