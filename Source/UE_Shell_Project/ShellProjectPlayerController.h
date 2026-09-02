#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"

#include "ShellProjectPlayerController.generated.h"

class UInputAction;
class UInputMappingContext;
class UWidgetComponent;
class UShellTerminalWidget;

/**
 * 双实例-演示用的 shell 呈现状态（T12 前缀）。
 *
 * Tab 在四态间循环切换：
 *  - Shrink         缩小显示画面（HUD 视口实例，锚左下角）
 *  - ShrinkOccluded 缩小且下半部分被遮挡（HUD 实例下沉到屏幕边缘/底槽）
 *  - HeldInHand     倾斜如同拿在手里（世界 WidgetComponent 实例，面向相机）
 *  - InputWindow    居中大窗口 + 可输入（切到 UIOnly，焦点给终端输入框）
 */
UENUM(BlueprintType)
enum class EShellPresentationState : uint8
{
	Shrink          UMETA(DisplayName = "Shrink"),
	ShrinkOccluded  UMETA(DisplayName = "Shrink + Occluded"),
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
	 * 三个状态循环：
	 *  Shrink -> ShrinkOccluded -> HeldInHand -> Shrink ...
	 */
	UFUNCTION(BlueprintCallable, Category = "Shell Presentation")
	void CycleShellPresentation();

	/** 直接跳到指定状态（0=Shrink, 1=ShrinkOccluded, 2=HeldInHand），越界夹取。 */
	UFUNCTION(BlueprintCallable, Category = "Shell Presentation")
	void SetShellPresentationState(int32 InState);

protected:
	/** 持有角色后自动应用默认呈现态（Shrink，左下角常显）。 */
	virtual void OnPossess(APawn* InPawn) override;

private:
	void HandleTerminalToggle();

	/** 把当前状态落到各实例：显示/隐藏、缩放/平移、billboard。 */
	void ApplyShellPresentation();

	/** 惰性创建 HUD 视口实例（同一个 UShellSubsystem 输出的双实例之一）。 */
	UShellTerminalWidget* EnsureHudShellWidget();

	/** 当前 Pawn 若无 ShellScreen 世界组件则返回 null（菜单/非角色场景）。 */
	UWidgetComponent* GetShellScreenComponentOrNull() const;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> ShellMappingContext;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> TerminalToggleAction;

	/** 双实例-HUD 实例：视口底左的终端（RenderTransform 做缩小/下沉）。 */
	UPROPERTY(Transient)
	TObjectPtr<UShellTerminalWidget> HudShellWidget;

	/** 当前呈现状态（Tab 循环）。 */
	EShellPresentationState PresentationState = EShellPresentationState::Shrink;
};
