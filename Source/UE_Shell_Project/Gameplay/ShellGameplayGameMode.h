#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"

#include "ShellGameplayGameMode.generated.h"

class UShellSessionHUD;

/**
 * Gameplay 关卡 GameMode —— 登录守卫 + 会话 HUD：
 *  - 未登录到达本关卡（绕过 login 的兜底路径）→ 弹回菜单关卡；
 *  - 已登录 → 显示 UShellSessionHUD（"Logged in as <user>"）并切换游戏输入模式。
 */
UCLASS()
class UE_SHELL_PROJECT_API AShellGameplayGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AShellGameplayGameMode();

	virtual void BeginPlay() override;

private:
	/** 下一 tick 校验会话并装配 HUD（避开关卡初始化竞态）。 */
	void ValidateSessionAndSetupHUD();

	UPROPERTY(Transient)
	TObjectPtr<UShellSessionHUD> SessionHUD;
};
