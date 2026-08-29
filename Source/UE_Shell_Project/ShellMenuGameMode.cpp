#include "ShellMenuGameMode.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Shell/Terminal/ShellSubsystem.h"
#include "TimerManager.h"

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
	TryStartLoginFlow();
}

void AShellMenuGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	// 独立 PIE 中 PC 通常在世界 BeginPlay 前就绪；万一时序相反，
	// 这里补一次触发（幂等：bLoginFlowStarted 保护）。
	TryStartLoginFlow();
}

void AShellMenuGameMode::TryStartLoginFlow()
{
	if (bLoginFlowStarted)
	{
		return;
	}

	UGameInstance* GameInstance = GetGameInstance();
	UShellSubsystem* Shell = GameInstance ? GameInstance->GetSubsystem<UShellSubsystem>() : nullptr;
	APlayerController* PC = GameInstance ? GameInstance->GetFirstLocalPlayerController() : nullptr;

	if (!Shell || !PC)
	{
		// 下一 tick 重试（上限约 2 秒），等 PlayerController / 子系统就绪。
		if (++LoginFlowRetryCount < 120 && GetWorld())
		{
			GetWorldTimerManager().SetTimerForNextTick(
				FTimerDelegate::CreateUObject(this, &AShellMenuGameMode::TryStartLoginFlow));
		}
		return;
	}

	bLoginFlowStarted = true;

	// 宿主自定义命令注册（hello / roll）：幂等，进菜单后终端立即可用。
	ShellProjectCommands::RegisterProjectCommands(Shell->GetRegistry());

	if (!Shell->IsTerminalOpen())
	{
		Shell->ToggleTerminal(PC);
	}
	// 以用户身份提交（与真实回车同路径）：滚动区会留下
	// "root@blui:/# login" 回显行，随后进入 username: 交互。
	Shell->SubmitTerminalLine(TEXT("login"));
}
