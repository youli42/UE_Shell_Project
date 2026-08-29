#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"

#include "ShellMenuGameMode.generated.h"

/**
 * 菜单关卡 GameMode —— "终端即登录页"：
 * 关卡开始后自动打开终端，并以用户身份提交 `login`，
 * 使玩家直接进入 username:/password: 交互（走 UI 提交路径，带提示符回显）。
 * 登录成功后的跳转由插件的 login 命令链路完成（bAutoTravelOnLogin）。
 */
UCLASS()
class UE_SHELL_PROJECT_API AShellMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AShellMenuGameMode();

	virtual void BeginPlay() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;

private:
	/** 打开终端并自动提交 login；PlayerController 尚未就绪时逐 tick 重试。 */
	void TryStartLoginFlow();

	bool bLoginFlowStarted = false;
	int32 LoginFlowRetryCount = 0;
};
