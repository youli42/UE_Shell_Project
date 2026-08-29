#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"

#include "ShellProjectPlayerController.generated.h"

class UInputAction;
class UInputMappingContext;

/**
 * 宿主接入示范（Shell_UE 接线方式之一）：
 *  - BeginPlay 挂载插件的 IMC_Shell（Tab 映射，可在插件内容里重绑定）；
 *  - SetupInputComponent 把 IA_TerminalToggle 绑定到 UShellSubsystem::ToggleTerminal。
 * 宿主游戏可自由替换为自己的绑定方式；本类只是"如何接线"的活示例。
 */
UCLASS()
class UE_SHELL_PROJECT_API AShellProjectPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

private:
	void HandleTerminalToggle();

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> ShellMappingContext;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> TerminalToggleAction;
};
