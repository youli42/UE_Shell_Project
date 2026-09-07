#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "UObject/SoftObjectPtr.h"

#include "ShellProjectPlayerController.generated.h"

class UInputAction;
class UInputMappingContext;
class UShellTerminalWidget;
class UShellWorldScreen;

/**
 * 双实例-演示用的 shell 呈现状态（T12 前缀）。
 *
 * Tab 在两态间循环切换（仅保留世界屏手持态，已移除 HUD 视口态）：
 *  - HeldInHand     手持面板（世界 WidgetComponent 实例，放低可见）
 *  - InputWindow    面前大窗口 + 可输入（世界屏 Front 姿态，输入接管 + 焦点给终端）
 */
UENUM(BlueprintType)
enum class EShellPresentationState : uint8
{
	HeldInHand      UMETA(DisplayName = "Held In Hand"),
	InputWindow     UMETA(DisplayName = "Input Window"),
};

/**
 * 宿主接入示范（Shell_UE 接线方式之一）：
 *  - BeginPlay 挂载插件的 IMC_Shell（Tab 映射，可在插件内容里重绑定）；
 *  - SetupInputComponent 把 IA_TerminalToggle 绑定到 HandleTerminalToggle。
 * 宿主游戏可自由替换为自己的绑定方式；本类只是"如何接线"的活示例。
 *
 * T12 前缀：HandleTerminalToggle 现在是"双实例三态循环"——只有当前 Pawn 是
 * AShellProjectCharacter（游戏场景）时才循环呈现状态；菜单（登录）场景仍退回
 * UShellSubsystem::ToggleTerminal 原行为。
 */
UCLASS()
class UE_SHELL_PROJECT_API AShellProjectPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

	/** 当前呈现状态（只读）。 */
	EShellPresentationState GetPresentationState() const { return PresentationState; }

	/**
	 * 前进到下一个呈现状态并应用（Tab 触发；宿主/蓝图亦可驱动）。
	 * 两态循环：
	 *  HeldInHand -> InputWindow -> HeldInHand ...
	 */
	UFUNCTION(BlueprintCallable, Category = "Shell Presentation")
	void CycleShellPresentation();

	/** 直接跳到指定状态（0=HeldInHand, 1=InputWindow），越界夹取。 */
	UFUNCTION(BlueprintCallable, Category = "Shell Presentation")
	void SetShellPresentationState(int32 InState);

	/**
	 * 输入模式/光标策略的唯一入口（所有场景统一由本控制器拥有）：
	 *  - true  GameAndUI + 显示光标，且按住左键不隐藏光标/不捕获视口
	 *         （世界面片上的点击体验与普通 UI 一致）。
	 *  - false GameOnly + 隐藏光标（纯游戏输入）。
	 */
	UFUNCTION(BlueprintCallable, Category = "Shell Presentation")
	void SetShellUIFocus(bool bUIFocused);

protected:
	/** 持有角色后自动应用默认呈现态（HeldInHand，手持面板）。 */
	virtual void OnPossess(APawn* InPawn) override;

private:
	void HandleTerminalToggle();

	/** HostOwned 流控键范式下的 ESC：退出输入接管态（InputWindow → HeldInHand）。 */
	void HandleEscapeUI();

	/** 终端面板捕获的宿主流控键（HostOwned 范式）：Tab=循环呈现态、ESC=退出输入态。
	 *  仅在面板持有键盘焦点（InputWindow 态）时到达；非 UI 态走 IMC_Shell 游戏输入管道，两路互补。 */
	UFUNCTION()
	void HandleHostFlowKey(FName KeyName);

	/** 把当前状态落到各实例：显示/隐藏、姿态、输入接管。 */
	void ApplyShellPresentation();

	/** 当前 Pawn 若无世界屏组件则返回 null（菜单/非角色场景）。 */
	UShellWorldScreen* GetWorldScreenOrNull() const;

	/** Tab 终端开关动作（插件资产；软引用，可由蓝图/编辑器替换绑定）。 */
	UPROPERTY(EditAnywhere, Category = "Shell|Input")
	TSoftObjectPtr<UInputAction> TerminalToggleAction = TSoftObjectPtr<UInputAction>(
		FSoftObjectPath(TEXT("/Shell_UE/Shell/Input/IA_TerminalToggle.IA_TerminalToggle")));

	/** 插件终端输入映射（软引用，可替换）。 */
	UPROPERTY(EditAnywhere, Category = "Shell|Input")
	TSoftObjectPtr<UInputMappingContext> ShellMappingContext = TSoftObjectPtr<UInputMappingContext>(
		FSoftObjectPath(TEXT("/Shell_UE/Shell/Input/IMC_Shell.IMC_Shell")));

	/**
	 * ESC 流控动作（运行时构建，UPROPERTY 防 GC）。
	 * HostOwned 范式下终端面板不消费 ESC（EShellFlowKeyMode），由本控制器
	 * 统一处理：InputWindow 态退出回默认呈现；其余态忽略。
	 */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> EscapeUIAction;

	/** ESC 流控映射上下文（运行时构建，与 IMC_Shell(0) 并存）。 */
	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> EscapeMappingContext;

	/** 当前呈现状态（Tab 循环）。 */
	EShellPresentationState PresentationState = EShellPresentationState::HeldInHand;
};
