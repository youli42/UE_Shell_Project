#include "ShellWorldScreen.h"

#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Framework/Application/SlateApplication.h"
#include "UObject/ConstructorHelpers.h"

#include "Shell/Terminal/ShellSubsystem.h"
#include "Shell/Terminal/ShellTerminalWidget.h"

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
	ScreenComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ScreenComponent->SetGenerateOverlapEvents(false);
	// 初始隐藏，由 SetScreenVisible 显式决定（避免与调用方状态机冲突）。
	ScreenComponent->SetVisibility(false);

	// ⚠️ 默认 WidgetClass = UShellTerminalWidget。若不设，WidgetComponent 的
	// WidgetClass 为 NULL，面片将渲染空白（看不到屏幕）。
	WidgetClassToUse = UShellTerminalWidget::StaticClass();
	ScreenComponent->SetWidgetClass(WidgetClassToUse);

	// 交互：鼠标射线命中面片（默认）。InteractionSource 可外部覆盖。
	InteractionComponent = CreateDefaultSubobject<UWidgetInteractionComponent>(TEXT("Interaction"));
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
        ProxyMeshComponent->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));
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
		// 代理面片贴在屏幕背面（-X，本地 X 是厚度方向），避免与 Widget 平面 z-fighting，
		// 同时保留编辑器预览（面板正面朝向相机，代理在背后供摆位参考）。
		ProxyMeshComponent->SetupAttachment(this);
		ProxyMeshComponent->SetRelativeLocation(FVector(0.f, 0.f, 0.f));

		// 立起来：/Engine/BasicShapes/Plane 是 XY 平面（宽=X,高=Y,法线=Z,躺着）。
		// RotZ(90)·RotX(90)（FRotator(90,0,90)）把它转成 YZ 竖直、法线朝 +X，
		// 使「宽=组件Y、高=组件Z」与 Widget 屏幕（宽=Y,高=Z,法线=+X）完全同向。
		ProxyMeshComponent->SetRelativeRotation(FRotator(90.f, 0.f, 90.f));
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
	// 经 RotZ(90)·RotX(90) 旋转后：Plane 宽(本地X)落组件Y、高(本地Y)落组件Z，
	// 与 Widget 同向 —— 所以 Scale.Y 撑宽、Scale.Z 撑高。
	if (ProxyMeshComponent)
	{
		const float ProxyScaleY = WidthCm / 100.f;
		const float ProxyScaleZ = HeightCm / 100.f;
		ProxyMeshComponent->SetRelativeScale3D(FVector(1.f, ProxyScaleY, ProxyScaleZ));
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
	const TSharedRef<SWidget> FocusTarget = Terminal->GetFocusTarget();
	FSlateApplication::Get().SetKeyboardFocus(FocusTarget, EFocusCause::SetDirectly);
}

void UShellWorldScreen::Click(bool bPressed)
{
	if (!InteractionComponent)
	{
		return;
	}

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
		InteractionComponent->ScrollWheel(DeltaY);
	}
}

void UShellWorldScreen::SetInputActive(bool bActive)
{
	bInputActive = bActive;
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
