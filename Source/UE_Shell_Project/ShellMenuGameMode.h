#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"

#include "ShellMenuGameMode.generated.h"

/**
 * 菜单关卡 GameMode。
 *
 * 终端登录页改由场景里的 AShellWorldScreenActor（世界面片）承载：
 * 本类不再自动打开全屏 HUD 终端、也不再自动提交 `login`。
 * 只注册宿主自定义命令（hello / roll），登录流交给场景面片自行驱动。
 */
UCLASS()
class UE_SHELL_PROJECT_API AShellMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AShellMenuGameMode();

	virtual void BeginPlay() override;
};
