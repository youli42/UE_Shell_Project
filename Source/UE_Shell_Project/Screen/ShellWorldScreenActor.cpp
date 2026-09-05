#include "ShellWorldScreenActor.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/SceneComponent.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

#include "ShellProjectPlayerController.h"
#include "ShellWorldScreen.h"

AShellWorldScreenActor::AShellWorldScreenActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	WorldScreen = CreateDefaultSubobject<UShellWorldScreen>(TEXT("WorldScreen"));
	WorldScreen->SetupAttachment(SceneRoot);
	// 开始界面面片：1080p 分辨率 + 16:9 物理尺寸（96x54cm，约 24 英寸显示器）。
	// DrawSize 只管清晰度，物理大小由 ScreenWidthCm/HeightCm 决定。
	WorldScreen->SetDrawSize(FVector2D(1920.f, 1080.f));
	WorldScreen->SetScreenPhysicalSize(96.f, 54.f);
	// 编辑器放置时显示可视化代理面片（运行时 Widget 自身渲染，代理在游戏内隐藏）。
	WorldScreen->SetVisualProxyEnabled(true);
}

void AShellWorldScreenActor::BeginPlay()
{
	Super::BeginPlay();

	if (!WorldScreen)
	{
		return;
	}

	WorldScreen->SetScreenVisible(bAutoShowOnBeginPlay);

	if (bAutoShowOnBeginPlay || bAutoPrintBannerOnBeginPlay)
	{
		// 终端 Widget 在 NativeConstruct 才订阅 OnShellOutput；立即打印会丢首行。
		// 推迟到下一帧，保证面片上的 UShellTerminalWidget 已订阅。
		GetWorldTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateUObject(this, &AShellWorldScreenActor::ActivateLoginScreen));
	}
}

void AShellWorldScreenActor::ActivateLoginScreen()
{
	if (!WorldScreen)
	{
		return;
	}

	// 相机已在下一帧就绪（菜单 Pawn 已 spawn / possessed），让面片正对观察者。
	if (bAutoFaceCameraOnBeginPlay)
	{
		FaceCamera();
	}

	if (bAutoPrintBannerOnBeginPlay)
	{
		PrintBanner();
	}

	if (bAutoShowOnBeginPlay)
	{
		ShowLoginScreen();
	}
}

void AShellWorldScreenActor::ShowLoginScreen()
{
	if (!WorldScreen)
	{
		return;
	}

	WorldScreen->SetScreenVisible(true);
	// 激活自包含指针输入：左键点击/滚轮经 InteractionComponent 注入面片上的
	// Slate 事件（像普通 UI 一样可点可滚），同时置位输入接管标志。
	WorldScreen->SetInputActive(true);
	// 聚焦终端输入框，使键盘打字可用（login 流程沿用现有命令系统）。
	WorldScreen->FocusTerminal();

	// 输入模式/光标策略归 PlayerController 统一拥有：
	// GameAndUI + 显示光标 + 按住不隐藏光标（鼠标射线命中面片可点击，
	// 键盘焦点进终端打字；GameOnly 会把键盘焦点全部给 Pawn，导致打不了字）。
	if (UWorld* World = GetWorld())
	{
		if (AShellProjectPlayerController* ShellPC = Cast<AShellProjectPlayerController>(World->GetFirstPlayerController()))
		{
			ShellPC->SetShellUIFocus(true);
		}
		else if (APlayerController* PC = World->GetFirstPlayerController())
		{
			// 兜底：非本工程 PC（理论上不会出现）退回原生调用。
			FInputModeGameAndUI InputMode;
			InputMode.SetHideCursorDuringCapture(false);
			PC->SetInputMode(InputMode);
			PC->SetShowMouseCursor(true);
		}
	}
}

void AShellWorldScreenActor::PrintBanner()
{
	if (!WorldScreen || StartupBanner.IsEmpty())
	{
		return;
	}

	// 打印开机横幅（调用方通常先 ShowLoginScreen/FocusTerminal 再打印）。
	WorldScreen->PrintToTerminal(StartupBanner, EShellOutputType::System);
}

void AShellWorldScreenActor::FaceCamera()
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const APlayerController* PC = World->GetFirstPlayerController();
	if (!PC || !PC->PlayerCameraManager)
	{
		return;
	}

	// 让组件 +X（屏幕正面）指向相机，面片正对观察者。
	const FVector CameraLoc = PC->PlayerCameraManager->GetCameraLocation();
	const FVector WidgetLoc = GetActorLocation();
	const FVector ToCamera = CameraLoc - WidgetLoc;
	const FRotator Rot = FRotationMatrix::MakeFromX(ToCamera).Rotator();
	SetActorRotation(Rot);
}
