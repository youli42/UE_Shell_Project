#include "ShellWorldScreen.h"

#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMesh.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "UObject/ConstructorHelpers.h"

#include "Shell/Terminal/ShellSubsystem.h"
#include "Shell/Terminal/ShellTerminalWidget.h"
#include "ShellWidgetInteractionComponent.h"

UShellWorldScreen::UShellWorldScreen()
{
	PrimaryComponentTick.bCanEverTick = false;

	// 载体：屏幕 widget 组件。默认 WidgetClass = UShellTerminalWidget，
	// 默认 DrawSize = 1920x1080（可由外部 SetDrawSize / 编辑器覆盖）。
	// ⚠️ 不要在构造函数里 SetupAttachment(this)：此时 this 是 CDO 模板，
	// 当该组件被用于蓝图 Actor（三层嵌套）复制时会命中
	// "Template Mismatch"（子组件挂到模板而非实例）。attach 推迟到 OnComponentCreated。
	ScreenComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("Screen"));
	ScreenComponent->SetDrawSize(DrawSize);
	ScreenComponent->SetTwoSided(bTwoSided);
	// ⚠️ 射线可命中：UWidgetInteractionComponent 通过 Visibility 通道的物理射线命中面片，
	// 若设 NoCollision，屏幕对射线"隐形"——无法 hover、无法点击（此前点击失效的根因）。
	// QueryOnly + 仅挡 Visibility、其余通道照常忽略：不影响游戏内碰撞与物理。
	ScreenComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ScreenComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	ScreenComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	ScreenComponent->SetGenerateOverlapEvents(false);
	// 初始隐藏，由 SetScreenVisible 显式决定（避免与调用方状态机冲突）。
	ScreenComponent->SetVisibility(false);

	// ⚠️ 默认 WidgetClass = UShellTerminalWidget。若不设，WidgetComponent 的
	// WidgetClass 为 NULL，面片将渲染空白（看不到屏幕）。
	WidgetClassToUse = UShellTerminalWidget::StaticClass();
	ScreenComponent->SetWidgetClass(WidgetClassToUse);

	// 交互：鼠标射线命中面片（默认）。InteractionSource 可外部覆盖。
	// 用薄子类：注入指针事件前可同步真实修饰键状态（基类快照成员不会自动更新，
	// 否则 Shift+拖选 / Ctrl+点击 等组合键失效）。
	InteractionComponent = CreateDefaultSubobject<UShellWidgetInteractionComponent>(TEXT("Interaction"));
	InteractionComponent->InteractionSource = EWidgetInteractionSource::Mouse;
	InteractionComponent->bShowDebug = false;

	// 可视化代理面片：编辑器放置时预览"显示器有多大"。
	// 用引擎基础平面，物理尺寸跟随 ScreenWidthCm/ScreenHeightCm，运行时通常隐藏。
	ProxyMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProxyMesh"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (PlaneMesh.Succeeded())
	{
	    ProxyMeshComponent->SetStaticMesh(PlaneMesh.Object);
	}
	ProxyMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ProxyMeshComponent->SetGenerateOverlapEvents(false);
	ProxyMeshComponent->SetHiddenInGame(true);        // 运行时隐藏：Widget 自身渲染
	ProxyMeshComponent->SetVisibility(bShowVisualProxy); // 编辑器预览可见
	// 旋转在 OnComponentCreated 统一设置（构造器里的值会被其覆盖，不再重复初始化）。
}

void UShellWorldScreen::OnComponentCreated()
{
	Super::OnComponentCreated();

	// 在实例化后才 attach（onComponentCreated 时 this 是真实实例，
	// 而非 CDO 模板），避免蓝图三层嵌套的 Template Mismatch。
	// 幂等：已有父组件则跳过。
	if (ScreenComponent && ScreenComponent->GetAttachParent() == nullptr)
	{
		ScreenComponent->SetupAttachment(this);
	}
	if (InteractionComponent && InteractionComponent->GetAttachParent() == nullptr)
	{
		InteractionComponent->SetupAttachment(this);
	}
	if (ProxyMeshComponent && ProxyMeshComponent->GetAttachParent() == nullptr)
	{
		// 代理面片贴在屏幕位置（Widget 自身渲染，代理仅供摆位预览）。
		ProxyMeshComponent->SetupAttachment(this);
		ProxyMeshComponent->SetRelativeLocation(FVector(0.f, 0.f, 0.f));
		// 旋转在 ApplyConfiguration 每次注册时幂等应用——不能只在这里设置：
		// 已放置实例的旋转从地图序列化数据恢复（attach parent 非空会跳过本分支），
		// 旧地图里保存的错误旋转将永远无法被代码修正。
	}

	// WidgetClass 在 OnRegister（进入世界）之前应用，使 UWidgetComponent
	// 的 Tick 能拿到正确的类；此处先做一次，随后 OnRegister 再幂等应用。
	ApplyConfiguration();
}

void UShellWorldScreen::OnRegister()
{
	Super::OnRegister();

	ApplyConfiguration();

	if (bAutoFocusOnRegister)
	{
		FocusTerminal();
	}
}

void UShellWorldScreen::OnUnregister()
{
	// 注销时解除指针映射，避免残留到下一次注册/世界切换。
	UnbindPointerInput();
	bInputActive = false;

	Super::OnUnregister();
}

void UShellWorldScreen::ApplyConfiguration()
{
	if (!ScreenComponent)
	{
		return;
	}

	if (WidgetClassToUse)
	{
		if (ScreenComponent->GetWidgetClass() != WidgetClassToUse)
		{
			ScreenComponent->SetWidgetClass(WidgetClassToUse);
		}
	}
	ScreenComponent->SetTwoSided(bTwoSided);

	// 物理尺寸 = DrawSize(像素, 当作 cm) × 相对缩放。
	// 引擎本地：宽=DrawSize.X cm、高=DrawSize.Y cm（本地 Y=宽, Z=高）。
	// 要得到 ScreenWidthCm × ScreenHeightCm 的物理大小，缩放：
	//   Scale.Y = ScreenWidthCm / DrawSize.X;  Scale.Z = ScreenHeightCm / DrawSize.Y;
	ApplyPhysicalSize();

	if (InteractionComponent)
	{
		InteractionComponent->InteractionSource = InteractionSource;
	}

	if (ProxyMeshComponent)
	{
		// /Engine/BasicShapes/Plane 躺在 XY 平面（局部 X=宽、Y=高、Z=法线）；
		// Widget 面片在 YZ 平面（宽=组件 Y、高=组件 Z、法线=+X）。
		// 期望映射 宽(X)→Y、高(Y)→Z、法线(Z)→X，等价
		// UKismetMathLibrary::MakeRotationFromAxes(Forward=(0,1,0), Right=(0,0,1), Up=(1,0,0))
		// 的结果（由引擎实测计算，避免欧拉角组合顺序的约定歧义）。
		// 在此幂等应用：已放置实例的旋转从地图序列化恢复，仅靠构造/
		// OnComponentCreated 设置无法修正旧地图里保存的错误旋转。
		ProxyMeshComponent->SetRelativeRotation(FRotator(0.f, 90.f, -90.f));
		ProxyMeshComponent->SetVisibility(bShowVisualProxy);
	}
}

/** 根据分辨率(DrawSize)与物理尺寸(ScreenWidthCm/HeightCm)设置各组件相对缩放。 */
void UShellWorldScreen::ApplyPhysicalSize()
{
	if (!ScreenComponent)
	{
		return;
	}

	const float WidthCm = FMath::Max(ScreenWidthCm, 1.f);
	const float HeightCm = FMath::Max(ScreenHeightCm, 1.f);

	// Widget: 本地宽=DrawSize.X cm，本地高=DrawSize.Y cm。
	const float WidgetScaleY = WidthCm / FMath::Max(DrawSize.X, 1.f);
	const float WidgetScaleZ = HeightCm / FMath::Max(DrawSize.Y, 1.f);
	ScreenComponent->SetRelativeScale3D(FVector(1.f, WidgetScaleY, WidgetScaleZ));

	// 代理面片（/Engine/BasicShapes/Plane = 100x100 cm @ scale 1）。
	// 相对缩放作用在平面【局部轴】上（旋转之前）：局部 X=宽、Y=高。
	// 经旋转映射到组件 Y(宽)/Z(高) 后与 Widget 同向 —— 缩放写 (W/100, H/100, 1)。
	// （此前误按组件轴写 (1, W/100, H/100)，导致代理恒 100cm 宽、高错位。）
	if (ProxyMeshComponent)
	{
		const float ProxyScaleX = WidthCm / 100.f;
		const float ProxyScaleY = HeightCm / 100.f;
		ProxyMeshComponent->SetRelativeScale3D(FVector(ProxyScaleX, ProxyScaleY, 1.f));
	}
}

UShellTerminalWidget* UShellWorldScreen::GetTerminalWidget() const
{
	return ScreenComponent ? Cast<UShellTerminalWidget>(ScreenComponent->GetWidget()) : nullptr;
}

void UShellWorldScreen::SetDrawSize(const FVector2D& InDrawSize)
{
	DrawSize = InDrawSize;
	if (ScreenComponent)
	{
		ScreenComponent->SetDrawSize(DrawSize);
	}
	// 分辨率变了 → 物理尺寸不变的话，重新换算相对缩放。
	ApplyPhysicalSize();
}

void UShellWorldScreen::SetScreenPhysicalSize(float InWidthCm, float InHeightCm)
{
	ScreenWidthCm = InWidthCm;
	ScreenHeightCm = InHeightCm;
	ApplyPhysicalSize();
}

void UShellWorldScreen::SetVisualProxyEnabled(bool bEnabled)
{
	bShowVisualProxy = bEnabled;
	if (ProxyMeshComponent)
	{
		ProxyMeshComponent->SetVisibility(bEnabled);
	}
}

void UShellWorldScreen::SetScreenVisible(bool bInVisible)
{
	if (ScreenComponent)
	{
		ScreenComponent->SetVisibility(bInVisible);
	}
}

void UShellWorldScreen::FocusTerminal()
{
	UShellTerminalWidget* Terminal = GetTerminalWidget();
	if (!Terminal)
	{
		return;
	}

	// 与 AShellProjectPlayerController::ApplyShellPresentation 一致：
	// 聚焦需在 Slate 窗口就绪后于下一帧执行（当前帧获取的 FocusTarget 才有效）。
	// 注意 TSharedRef 不能作动态委托载荷，用 lambda 重载。
	const TSharedRef<SWidget> FocusTarget = Terminal->GetFocusTarget();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick([FocusTarget]()
		{
			FSlateApplication::Get().SetKeyboardFocus(FocusTarget, EFocusCause::SetDirectly);
		});
	}
}

void UShellWorldScreen::Click(bool bPressed)
{
	if (!InteractionComponent)
	{
		return;
	}

	// 注入前同步真实修饰键状态：交互组件按快照构造事件，不同步则
	// Shift+拖选文本、Ctrl+点击 等组合全部以"裸点击"送达。
	InteractionComponent->SyncModifierKeys();

	if (bPressed)
	{
		InteractionComponent->PressPointerKey(EKeys::LeftMouseButton);
	}
	else
	{
		InteractionComponent->ReleasePointerKey(EKeys::LeftMouseButton);
	}
}

void UShellWorldScreen::ScrollWheel(float DeltaY)
{
	if (InteractionComponent)
	{
		// 同 Click：Ctrl+滚轮等组合键语义需要真实修饰键状态。
		InteractionComponent->SyncModifierKeys();
		InteractionComponent->ScrollWheel(DeltaY);
	}
}

void UShellWorldScreen::SetInputActive(bool bActive)
{
	bInputActive = bActive;

	if (bActive)
	{
		BindPointerInput();
	}
	else
	{
		UnbindPointerInput();
	}
}

void UShellWorldScreen::EnsurePointerInputBuilt()
{
	if (PointerClickAction)
	{
		return; // 幂等
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	// 运行时惰性构建（构造器内 NewObject 会触发 UObjectGlobals 致命错误）。
	PointerClickAction = NewObject<UInputAction>(Owner, TEXT("ShellScreenPointerClick"));
	PointerClickAction->ValueType = EInputActionValueType::Boolean;

	PointerWheelAction = NewObject<UInputAction>(Owner, TEXT("ShellScreenPointerWheel"));
	PointerWheelAction->ValueType = EInputActionValueType::Axis1D;

	PointerMappingContext = NewObject<UInputMappingContext>(Owner, TEXT("ShellScreenPointerMappingContext"));
	PointerMappingContext->MapKey(PointerClickAction, EKeys::LeftMouseButton);
	PointerMappingContext->MapKey(PointerWheelAction, EKeys::MouseWheelAxis);
}

bool UShellWorldScreen::BindPointerInput()
{
	if (bPointerInputBound)
	{
		// 绑定已就绪，只需确保映射上下文在位（重复 Add 幂等）。
		if (PointerMappingContext)
		{
			if (const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
			{
				if (ULocalPlayer* LP = PC->GetLocalPlayer())
				{
					if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
					{
						Subsystem->AddMappingContext(PointerMappingContext, 2);
					}
				}
			}
		}
		return true;
	}

	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;

	// 尚未就绪（如关卡加载早期调用 SetInputActive）：按帧重试而非静默失败，
	// 否则 bInputActive 为 true 却永远没有绑定（输入死区）。
	const bool bNotReady = !Owner || !World || !PC || !PC->IsLocalController();
	if (bNotReady)
	{
		if (bInputActive && PointerBindRetryCount < MaxPointerBindRetries)
		{
			++PointerBindRetryCount;
			if (UWorld* RetryWorld = GetWorld())
			{
				RetryWorld->GetTimerManager().SetTimerForNextTick(
					FTimerDelegate::CreateUObject(this, &UShellWorldScreen::RetryBindPointerInput));
			}
		}
		else if (bInputActive)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[ShellWorldScreen] SetInputActive(true) 绑定失败：PlayerController 在 %d 次重试后仍未就绪"),
				PointerBindRetryCount);
		}
		return false;
	}

	EnsurePointerInputBuilt();
	if (!PointerClickAction || !PointerWheelAction || !PointerMappingContext)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ShellWorldScreen] SetInputActive(true) 失败：指针输入构建失败"));
		return false;
	}

	// Owner Actor 需要把 InputComponent 压入 PC 输入栈才能收到增强输入事件。
	Owner->EnableInput(PC);
	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(Owner->InputComponent);
	if (!EnhancedInput)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ShellWorldScreen] SetInputActive(true) 失败：Owner InputComponent 不是 EnhancedInputComponent"));
		return false;
	}

	EnhancedInput->BindAction(PointerClickAction, ETriggerEvent::Started, this, &UShellWorldScreen::HandlePointerPress);
	EnhancedInput->BindAction(PointerClickAction, ETriggerEvent::Completed, this, &UShellWorldScreen::HandlePointerRelease);
	EnhancedInput->BindAction(PointerWheelAction, ETriggerEvent::Triggered, this, &UShellWorldScreen::HandlePointerWheel);

	if (ULocalPlayer* LP = PC->GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			// 优先级 2：与 IMC_Shell(0)/角色 IMC(1) 并存。
			Subsystem->AddMappingContext(PointerMappingContext, 2);
		}
	}

	bPointerInputBound = true;
	PointerBindRetryCount = 0;
	return true;
}

void UShellWorldScreen::RetryBindPointerInput()
{
	// 定时器重试入口（FTimerDelegate 要求 void 返回）。
	// 中途被停用（SetInputActive(false)）则不再继续。
	if (bInputActive && !bPointerInputBound)
	{
		BindPointerInput();
	}
}

void UShellWorldScreen::UnbindPointerInput()
{
	// 只移除映射上下文：绑定保留但事件不再触发，重复激活无需重复绑定。
	if (!PointerMappingContext)
	{
		return;
	}

	if (const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		if (ULocalPlayer* LP = PC->GetLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
			{
				Subsystem->RemoveMappingContext(PointerMappingContext);
			}
		}
	}
}

bool UShellWorldScreen::IsPointerOverSelf() const
{
	// 鼠标射线命中"本屏"才算数：多屏共存时，射线可能落在别人的面片上——
	// 此时由被悬停的那块屏负责转发（各自组件都从同一光标发射线，
	// 若不加门槛，被悬停屏会收到双份指针事件）。
	return InteractionComponent
		&& ScreenComponent
		&& InteractionComponent->GetHoveredWidgetComponent() == ScreenComponent;
}

void UShellWorldScreen::HandlePointerPress(const FInputActionValue& Value)
{
	// 门槛统一：激活 + 可见 + 指针确实落在本屏。
	if (bInputActive && ScreenComponent && ScreenComponent->IsVisible() && IsPointerOverSelf())
	{
		// 像普通 UI 一样把键盘焦点给终端输入框。两个引擎机制决定了必须这样做：
		//  1) 虚拟点击（PressPointerKey）里 SEditableText 的 SetUserFocus 只作用于
		//     交互组件的【虚拟用户】——光标显示但物理键盘不路由；
		//  2) 物理点击下真实光标处无屏幕空间 widget（世界面片是 3D），Slate 的
		//     「焦点跟随点击」会把键盘用户的焦点移到游戏视口（SViewport 支持键盘焦点）。
		// 因此点击后必须重新断言【键盘用户】焦点，打字才可用（与启动时
		// ShowLoginScreen → FocusTerminal 同一路径）。
		FocusTerminal();
		Click(true);
	}
}

void UShellWorldScreen::HandlePointerRelease(const FInputActionValue& Value)
{
	if (bInputActive && ScreenComponent && ScreenComponent->IsVisible() && IsPointerOverSelf())
	{
		Click(false);
	}
}

void UShellWorldScreen::HandlePointerWheel(const FInputActionValue& Value)
{
	if (bInputActive && ScreenComponent && ScreenComponent->IsVisible() && IsPointerOverSelf())
	{
		ScrollWheel(Value.Get<float>());
	}
}

void UShellWorldScreen::PrintToTerminal(const FString& InText, EShellOutputType InType)
{
	if (const UWorld* World = GetWorld())
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			if (UShellSubsystem* Shell = GI->GetSubsystem<UShellSubsystem>())
			{
				Shell->Print(InText, InType);
			}
		}
	}
}

void UShellWorldScreen::SubmitTerminalLine(const FString& InLine)
{
	if (const UWorld* World = GetWorld())
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			if (UShellSubsystem* Shell = GI->GetSubsystem<UShellSubsystem>())
			{
				Shell->SubmitTerminalLine(InLine);
			}
		}
	}
}
