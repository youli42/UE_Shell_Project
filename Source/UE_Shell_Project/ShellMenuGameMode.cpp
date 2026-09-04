#include "ShellMenuGameMode.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Shell/Terminal/ShellSubsystem.h"

#include "ShellProjectCommands.h"
#include "ShellProjectMenuPawn.h"
#include "ShellProjectPlayerController.h"

AShellMenuGameMode::AShellMenuGameMode()
{
	DefaultPawnClass = AShellProjectMenuPawn::StaticClass();
	PlayerControllerClass = AShellProjectPlayerController::StaticClass();
}

void AShellMenuGameMode::BeginPlay()
{
	Super::BeginPlay();

	// 终端登录页改由场景里的 AShellWorldScreenActor（世界面片）承载：
	// 这里不再自动打开全屏 HUD 终端、也不再自动提交 login。
	// 仅注册宿主自定义命令（hello / roll），使终端立即可用。
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UShellSubsystem* Shell = GameInstance->GetSubsystem<UShellSubsystem>())
		{
			ShellProjectCommands::RegisterProjectCommands(Shell->GetRegistry());
		}
	}
}
