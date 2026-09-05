#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Components/WidgetInteractionComponent.h"
#include "Shell/Terminal/ShellTypes.h"

#include "ShellWorldScreen.generated.h"

class UShellTerminalWidget;
class UEnhancedInputComponent;
class UInputAction;
class UInputMappingContext;
class UStaticMesh;
class UStaticMeshComponent;
class UUserWidget;
class UWidgetComponent;
class UWidgetInteractionComponent;
class SWidget;
struct FInputActionValue;

/**
 * 可复用的"世界终端屏幕"组件（R1）。
 *
 * 把"终端显示在世界里的面片上 + 可交互"抽象成单一 USceneComponent：
 * 组件内部自建并管理
 *  - UWidgetComponent        （载体，WidgetClass 默认 = UShellTerminalWidget）
 *  - UWidgetInteractionComponent（交互，鼠标射线命中面片 / 输入接管）
 *
 * 通用接口集中于此（打字聚焦/点击/滚轮/输入接管），避免散落在角色/控制器：
 *  - SetDrawSize    面片分辨率
 *  - SetScreenVisible
 *  - FocusTerminal  聚焦终端 SEditableText（打字）
 *  - Click          世界交互组件 Press/Release
 *  - ScrollWheel    滚轮滚动
 *  - SetInputActive 输入接管：true 时绑定左键/滚轮转发（像 UI 一样可点击/滚动），
 *                   并置位标志供调用方（角色）忽略移动/视角
 *
 * 两处场景共用同一套架构：
 *  - 开始界面：场景面片（固定相机 + 鼠标射线命中面片 + FocusTerminal 打字）
 *  - 游戏内手持：角色身上的世界盘（由角色驱动本组件的 transform：Front↔Hand）
 *
 * 终端数据模型仍走 UShellSubsystem::OnShellOutput 广播（多实例渲染），
 * 本组件只是其中一个实例，不改插件 Shell_UE。
 */
UCLASS(ClassGroup = (Shell), Blueprintable, BlueprintType, meta = (BlueprintSpawnableComponent))
class UE_SHELL_PROJECT_API UShellWorldScreen : public USceneComponent
{
	GENERATED_BODY()

public:
	UShellWorldScreen();

	// -------------------------------------------------------------------------
	// 通用接口（R1）——集中在此，调用方无需关心内部组件。
	// -------------------------------------------------------------------------

	/** 设置面片（widget）渲染分辨率。 */
	UFUNCTION(BlueprintCallable, Category = "Shell|WorldScreen")
	void SetDrawSize(const FVector2D& InDrawSize);

	/** 设置面片在世界里的物理尺寸（厘米），与分辨率解耦。 */
	UFUNCTION(BlueprintCallable, Category = "Shell|WorldScreen")
	void SetScreenPhysicalSize(float InWidthCm, float InHeightCm);

	/** 设置可视化代理面片显示/隐藏（编辑器放置预览用）。 */
	UFUNCTION(BlueprintCallable, Category = "Shell|WorldScreen")
	void SetVisualProxyEnabled(bool bEnabled);

	/** 显示 / 隐藏面片。 */
	UFUNCTION(BlueprintCallable, Category = "Shell|WorldScreen")
	void SetScreenVisible(bool bInVisible);

	/** 聚焦终端输入框（SEditableText），使键盘打字可用。 */
	UFUNCTION(BlueprintCallable, Category = "Shell|WorldScreen")
	void FocusTerminal();

	/** 世界交互组件点击：bPressed=true 按下左键，false 释放。 */
	UFUNCTION(BlueprintCallable, Category = "Shell|WorldScreen")
	void Click(bool bPressed);

	/** 滚轮滚动（DeltaY 正=向下滚动）。 */
	UFUNCTION(BlueprintCallable, Category = "Shell|WorldScreen")
	void ScrollWheel(float DeltaY);

	/**
	 * 设置"输入接管"：
	 *  - true  绑定左键按下/释放与滚轮到本屏幕（世界面片像 UI 一样可点击/滚动），
	 *          并置位标志（调用方通常为角色，应忽略移动/视角）。
	 *  - false 解除绑定（移除映射上下文，事件不再触发）。
	 */
	UFUNCTION(BlueprintCallable, Category = "Shell|WorldScreen")
	void SetInputActive(bool bActive);

	/** 往终端输出一行（走 UShellSubsystem::OnShellOutput 广播，所有终端实例同时可见）。 */
	UFUNCTION(BlueprintCallable, Category = "Shell|WorldScreen")
	void PrintToTerminal(const FString& InText, EShellOutputType InType = EShellOutputType::System);

	/** 以用户身份往终端提交一行命令（与真实回车同路径，含提示符回显）。 */
	UFUNCTION(BlueprintCallable, Category = "Shell|WorldScreen")
	void SubmitTerminalLine(const FString& InLine);

	// -------------------------------------------------------------------------
	// 对外暴露内部组件（角色/控制器需要 transform 动画 / 显式交互时用）。
	// -------------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Shell|WorldScreen")
	UWidgetComponent* GetScreenComponent() const { return ScreenComponent; }

	UFUNCTION(BlueprintPure, Category = "Shell|WorldScreen")
	class UShellWidgetInteractionComponent* GetInteractionComponent() const { return InteractionComponent; }

	/** 面片上承载的终端 Widget（若已初始化），仅当 WidgetClass 为终端时非空。 */
	UFUNCTION(BlueprintPure, Category = "Shell|WorldScreen")
	UShellTerminalWidget* GetTerminalWidget() const;

	UFUNCTION(BlueprintPure, Category = "Shell|WorldScreen")
	bool IsInputActive() const { return bInputActive; }

	// -------------------------------------------------------------------------
	// 可配置项（编辑前 / 蓝图设置）。
	// -------------------------------------------------------------------------

	/**
	 * 面片渲染分辨率（像素）——只决定清晰度，不影响世界里的物理大小。
	 * 默认 1920x1080（1080p）。物理大小由 ScreenWidthCm / ScreenHeightCm 决定。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shell|WorldScreen|Screen",
		meta = (ToolTip = "渲染分辨率(像素)，只影响清晰度；世界物理大小由 ScreenWidthCm/ScreenHeightCm 决定"))
	FVector2D DrawSize = FVector2D(1920.f, 1080.f);

	/**
	 * 面片在世界里的物理宽度（UE 厘米）。默认 96cm（约 24 英寸显示器宽）。
	 * 与 DrawSize 解耦：DrawSize 只管清晰度，这里管实际显示多大。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shell|WorldScreen|Screen",
		meta = (ToolTip = "面片在世界里的物理宽度(厘米)；与 DrawSize(分辨率) 独立"))
	float ScreenWidthCm = 96.f;

	/** 面片在世界里的物理高度（UE 厘米），与 DrawSize 独立。默认 54cm（16:9）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shell|WorldScreen|Screen",
		meta = (ToolTip = "面片在世界里的物理高度(厘米)；与 DrawSize(分辨率) 独立"))
	float ScreenHeightCm = 54.f;

	/**
	 * 可视化代理面片：一个跟随屏幕物理尺寸的 StaticMesh 平面，
	 * 用于编辑器放置时预览"显示器有多大"。运行时通常隐藏（Widget 自身渲染）。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shell|WorldScreen|Screen",
		meta = (ToolTip = "放置时的可视化预览面片（显示/隐藏），物理尺寸=屏幕尺寸"))
	bool bShowVisualProxy = true;

	/** 面片双面渲染。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shell|WorldScreen|Screen")
	bool bTwoSided = true;

	/** 交互源：Mouse=鼠标射线命中面片；World=沿组件朝向；CenterScreen=屏幕中心。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shell|WorldScreen|Screen")
	EWidgetInteractionSource InteractionSource = EWidgetInteractionSource::Mouse;

	/** 面片承载的 widget 类型，默认 UShellTerminalWidget（可换其它 UUserWidget）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shell|WorldScreen|Screen")
	TSubclassOf<UUserWidget> WidgetClassToUse;

	/** 是否在注册时自动聚焦（开始界面可设为 true，进入即打字）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shell|WorldScreen|Screen")
	bool bAutoFocusOnRegister = false;

protected:
	virtual void OnComponentCreated() override;
	virtual void OnRegister() override;
	virtual void OnUnregister() override;

private:
	/** 把可配置项应用到子组件（幂等）。 */
	void ApplyConfiguration();

	/** 依据 DrawSize(分辨率) 与 ScreenWidthCm/HeightCm(物理尺寸) 设置组件相对缩放。 */
	void ApplyPhysicalSize();

	// -------------------------------------------------------------------------
	// 自包含指针输入（像 UI 一样可点击/滚动的转发链路）：
	// 惰性构建 IA/IMC → Owner Actor EnableInput → 绑定 Press/Release/Wheel
	// → InteractionComponent 注入 Slate 事件。激活/停用只增删映射上下文。
	// -------------------------------------------------------------------------

	/** 惰性创建点击/滚轮 IA + IMC（运行时调用，构造器内 NewObject 会崩溃）。幂等。 */
	void EnsurePointerInputBuilt();

	/** 在 Owner Actor 的增强输入组件上创建一次性绑定并挂载映射上下文。 */
	bool BindPointerInput();

	/** 绑定重试入口（定时器回调；PlayerController 未就绪时按帧重试，封顶防死循环）。 */
	void RetryBindPointerInput();

	/** 移除映射上下文（绑定保留，停用态事件不再触发）。 */
	void UnbindPointerInput();

	/**
	 * 本屏当前是否正被指针射线命中（点击/滚轮转发与聚焦断言的统一门槛）：
	 * 多块屏共存时，鼠标射线可能命中"别人的面片"——此时不能由本屏转发
	 * （否则被悬停的屏会收到双份指针事件），也不应抢焦点。
	 */
	bool IsPointerOverSelf() const;

	/** 左键按下 → Click(true)。 */
	void HandlePointerPress(const FInputActionValue& Value);

	/** 左键释放 → Click(false)。 */
	void HandlePointerRelease(const FInputActionValue& Value);

	/** 滚轮 → ScrollWheel。 */
	void HandlePointerWheel(const FInputActionValue& Value);

	UPROPERTY(VisibleAnywhere, Category = "Shell|WorldScreen|Internal")
	TObjectPtr<UWidgetComponent> ScreenComponent;

	UPROPERTY(VisibleAnywhere, Category = "Shell|WorldScreen|Internal")
	TObjectPtr<class UShellWidgetInteractionComponent> InteractionComponent;

	/** 可视化代理面片（编辑器放置预览用；运行时通常隐藏）。 */
	UPROPERTY(VisibleAnywhere, Category = "Shell|WorldScreen|Internal")
	TObjectPtr<UStaticMeshComponent> ProxyMeshComponent;

	/** 输入接管标志。 */
	bool bInputActive = false;

	/** 左键点击动作（运行时构建，UPROPERTY 防 GC）。 */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> PointerClickAction;

	/** 滚轮动作（运行时构建）。 */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> PointerWheelAction;

	/** 指针输入映射上下文（运行时构建）。 */
	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> PointerMappingContext;

	/** 一次性绑定已完成标志（重复 SetInputActive(true) 不重复绑定）。 */
	bool bPointerInputBound = false;

	/** 绑定重试计数（PC 尚未就绪的早期 SetInputActive 时按帧重试，封顶防死循环）。 */
	int32 PointerBindRetryCount = 0;

	/** 绑定重试上限（~1 秒 @60fps）。 */
	static constexpr int32 MaxPointerBindRetries = 60;
};
