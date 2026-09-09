#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "UObject/SoftObjectPtr.h"

#include "ShellProjectPlayerController.generated.h"

class UInputAction;
class UInputMappingContext;
class UShellFloatingQuickButton;
class UShellInputStateManager;
class UShellTerminalWidget;
class UShellWorldScreen;
struct FShellHotkeyChord;
struct FShellHotkeyBinding;

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
	 * 输入状态入口（公开 BlueprintCallable 签名不变）。
	 * 只表达意图，不判断打字面、不指定状态名 —— 由 UShellInputStateManager 裁决：
	 *  - true  RequestUiState：有活动打字面 → UiTyping（聚焦终端）；
	 *          否则 UiBrowse（只显示 UI、不抢键盘焦点）。
	 *  - false RequestGameplayState：Gameplay（纯游戏输入、无光标）。
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

	/** 输入状态管理器（GameInstance 子系统；不可用时返回 null）。 */
	UShellInputStateManager* GetInputStateManager() const;

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

	// --- M5：快捷指令全局热键 + 悬浮菜单开关键 ---------------------------------

	/** 按 UShellSubsystem::GetEffectiveHotkeys 全量重建热键 IMC（仅变更时调用）。 */
	void RebuildHotkeyMapping();

	/** 菜单开关键绑定（运行时构建，一次即可；玩家改开关键走 qcmd togglekey）。 */
	void BuildMenuToggleBinding();

	/** 热键触发：修饰键按 Slate 真实状态校验后交给子系统消费。 */
	void HandleQuickCommandHotkey(const FShellHotkeyChord& InChord);

	UFUNCTION()
	void HandleQuickCommandsChanged();

	/** 当前 Pawn 的悬浮按钮（非角色场景返回 null）。 */
	UShellFloatingQuickButton* GetFloatingQuickButtonOrNull() const;

	/** 热键映射上下文（运行时构建；重建 = Remove + Add，非每帧）。 */
	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> QuickHotkeyMappingContext;

	/** 菜单开关键动作 + 映射（运行时构建）。 */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> MenuToggleAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> MenuToggleMappingContext;

	/** 热键动作表（UPROPERTY 防 GC；与 IMC 一起在重建时整体替换）。 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UInputAction>> QuickHotkeyActions;

	/**
	 * lambda 绑定句柄表（M5 焦点修复顺带）：BindActionValueLambda 的绑定挂在
	 * InputComponent 上，不随 IMC 移除而失效 —— 重建前必须按句柄清掉，
	 * 否则每次 OnQuickCommandsChanged 都累积一批永不触发的死绑定。
	 */
	TArray<uint32> QuickHotkeyBindingHandles;

	/** 当前呈现状态（Tab 循环）。 */
	EShellPresentationState PresentationState = EShellPresentationState::HeldInHand;
};
