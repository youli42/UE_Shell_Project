#pragma once

#include "CoreMinimal.h"
#include "Components/WidgetInteractionComponent.h"

#include "ShellWidgetInteractionComponent.generated.h"

/**
 * UWidgetInteractionComponent 的薄子类（世界屏专用）。
 *
 * 引擎基类的 ModifierKeys 是 protected 快照成员，且【不会】从物理键盘自动更新——
 * 不主动同步，经交互组件注入的指针/按键事件会丢失 Shift/Ctrl/Alt 组合
 * （Shift+拖选文本、Ctrl+点击等全部失效）。本子类暴露同步入口，
 * UShellWorldScreen 在每次注入前同步真实修饰键状态。
 *
 * 预留扩展点：后续如需注入键盘事件（PressKey 家族）、暴露 hover 路径等，
 * 在此子类集中补充，避免散落调用方。
 */
UCLASS(ClassGroup = (Shell), meta = (BlueprintSpawnableComponent))
class UE_SHELL_PROJECT_API UShellWidgetInteractionComponent : public UWidgetInteractionComponent
{
	GENERATED_BODY()

public:
	/** 把当前物理修饰键状态同步进事件快照（每次注入指针/按键事件前调用）。 */
	void SyncModifierKeys();
};
