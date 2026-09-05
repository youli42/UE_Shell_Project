#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"

#include "ShellProjectMenuPawn.generated.h"

class UCameraComponent;

/**
 * 菜单关卡占位 Pawn：一个固定相机、无移动输入。
 * 菜单阶段的交互全部由终端（UIOnly 输入模式）承担。
 */
UCLASS()
class UE_SHELL_PROJECT_API AShellProjectMenuPawn : public APawn
{
	GENERATED_BODY()

public:
	AShellProjectMenuPawn();

private:
	UPROPERTY(VisibleAnywhere, Category = "Shell Project")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Shell Project")
	TObjectPtr<UCameraComponent> Camera;
};
