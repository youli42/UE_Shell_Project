#include "ShellProjectCharacter.h"

#include "AbilitySystemComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMesh.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "UObject/ConstructorHelpers.h"

#include "Shell/Terminal/ShellTerminalWidget.h"
#include "ShellProjectPlayerState.h"
#include "ShellWorldScreen.h"

AShellProjectCharacter::AShellProjectCharacter()
{
	// billboard 需要每帧更新——但只在世界屏幕可见时才有实际工作。
	PrimaryActorTick.bCanEverTick = true;

	// 第一人称：角色朝向跟随鼠标偏航，移动方向 = 视线方向。
	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	GetCharacterMovement()->bOrientRotationToMovement = false;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));

	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(GetCapsuleComponent());
	if (CubeMesh.Succeeded())
	{
		BodyMesh->SetStaticMesh(CubeMesh.Object);
	}
	BodyMesh->SetWorldScale3D(FVector(0.35f, 0.35f, 0.7f));
	BodyMesh->SetRelativeLocation(FVector(0.f, 0.f, 40.f));

	// 第一人称相机：直接挂胶囊，抬高到眼高，跟随控制器旋转（鼠标视角）。
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(GetCapsuleComponent());
	FollowCamera->SetRelativeLocation(FVector(0.f, 0.f, 64.f));
	FollowCamera->bUsePawnControlRotation = true;

	// 双实例-世界实例："手持屏幕"，统一走 UShellWorldScreen（显示+交互+输入接管）。
	// 挂相机下，默认隐藏（HeldInHand/InputWindow 态显示）；DrawSize 只管清晰度，
	// 物理尺寸由姿态动画驱动（Front 96x60 / Hand 70.4x44 cm）。
	WorldScreen = CreateDefaultSubobject<UShellWorldScreen>(TEXT("WorldScreen"));
	WorldScreen->SetupAttachment(FollowCamera);
	WorldScreen->SetDrawSize(FVector2D(1280.f, 800.f));
	WorldScreen->SetScreenPhysicalSize(96.f, 60.f);
	WorldScreen->SetVisualProxyEnabled(false); // 第一人称手持无需编辑器预览代理
	WorldScreen->SetScreenVisible(false);

	// 输入动作 + IMC 在运行时惰性构建（构造器内 NewObject 会触发
	// UObjectGlobals.cpp:4880 致命错误——见 EnsureInputBuilt）。
}

UWidgetComponent* AShellProjectCharacter::GetShellScreenComponent() const
{
	return WorldScreen ? WorldScreen->GetScreenComponent() : nullptr;
}

UShellTerminalWidget* AShellProjectCharacter::GetShellScreenWidget() const
{
	return WorldScreen ? WorldScreen->GetTerminalWidget() : nullptr;
}

bool AShellProjectCharacter::IsShellInputActive() const
{
	return WorldScreen && WorldScreen->IsInputActive();
}

void AShellProjectCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	InterpShellScreenToPose(DeltaSeconds);
	UpdateShellScreenBillboard();
}

void AShellProjectCharacter::SetShellScreenPose(EShellScreenPose InPose)
{
	ShellPose = InPose;
}

void AShellProjectCharacter::InterpShellScreenToPose(float DeltaSeconds)
{
	if (!WorldScreen)
	{
		return;
	}

	// 目标姿态（相对相机局部 + 物理尺寸）：
	// Front 近/居中/大 96x60cm，Hand 前/左/下/小 70.4x44cm（沿用旧 scale 0.075/0.055 @1280x800）。
	FVector TargetLoc;
	float TargetWidthCm;
	float TargetHeightCm;
	if (ShellPose == EShellScreenPose::Front)
	{
		TargetLoc      = FVector(78.f, 0.f, -12.f);
		TargetWidthCm  = 96.f;
		TargetHeightCm = 60.f;
	}
	else
	{
		TargetLoc      = FVector(62.f, -26.f, -42.f);
		TargetWidthCm  = 70.4f;
		TargetHeightCm = 44.f;
	}

	// 平滑插值（速度 ~8/s）：位置与物理尺寸（组件内部换算面片相对缩放）。
	const float Speed = 8.f;
	const FVector NewLoc = FMath::VInterpTo(WorldScreen->GetRelativeLocation(), TargetLoc, DeltaSeconds, Speed);
	const float NewWidth = FMath::FInterpTo(WorldScreen->ScreenWidthCm, TargetWidthCm, DeltaSeconds, Speed);
	const float NewHeight = FMath::FInterpTo(WorldScreen->ScreenHeightCm, TargetHeightCm, DeltaSeconds, Speed);
	WorldScreen->SetRelativeLocation(NewLoc);
	WorldScreen->SetScreenPhysicalSize(NewWidth, NewHeight);
}

void AShellProjectCharacter::UpdateShellScreenBillboard()
{
	UWidgetComponent* Screen = GetShellScreenComponent();
	if (!Screen || !Screen->IsVisible() || !Screen->IsRegistered())
	{
		return;
	}

	// 以玩家视角为相机参考（面板正面朝向相机）。
	const FVector CameraLoc = FollowCamera->GetComponentLocation();
	const FVector WidgetLoc = Screen->GetComponentLocation();

	// billboard：让组件 +X 由屏幕指向相机（面板正面朝玩家）。
	const FVector ToCamera = CameraLoc - WidgetLoc;
	FRotator Rot = FRotationMatrix::MakeFromX(ToCamera).Rotator();

	// 手持倾角：轻微往下前倾 + 一点滚动，营造"拿在手里"而非"贴墙海报"。
	Rot.Pitch -= 8.f;
	Rot.Roll += 6.f;

	Screen->SetWorldRotation(Rot);
}

void AShellProjectCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	BindAbilityAvatar();
}

void AShellProjectCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	BindAbilityAvatar();
}

void AShellProjectCharacter::BindAbilityAvatar()
{
	if (AShellProjectPlayerState* ShellPlayerState = GetPlayerState<AShellProjectPlayerState>())
	{
		if (UAbilitySystemComponent* ASC = ShellPlayerState->GetAbilitySystemComponent())
		{
			// Owner = PlayerState（ASC 挂载处），Avatar = 本角色。
			ASC->InitAbilityActorInfo(ShellPlayerState, this);
		}
	}
}

UAbilitySystemComponent* AShellProjectCharacter::GetAbilitySystemComponent() const
{
	if (const AShellProjectPlayerState* ShellPlayerState = GetPlayerState<AShellProjectPlayerState>())
	{
		return ShellPlayerState->GetAbilitySystemComponent();
	}
	return nullptr;
}

void AShellProjectCharacter::BeginPlay()
{
	Super::BeginPlay();
	BuildAndAddCharacterInputMapping();
}

void AShellProjectCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// 确保动作/IMC 已创建（运行时惰性构建，构造器内不能 NewObject）。
	EnsureInputBuilt();

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EnhancedInput)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ShellProject] Character InputComponent 不是 EnhancedInputComponent，移动/视角未绑定"));
		return;
	}

	// 1D 动作：W=+1 / S=-1，D=+1 / A=-1 —— 无 swizzle 歧义。
	if (MoveForwardAction)
	{
		EnhancedInput->BindAction(MoveForwardAction, ETriggerEvent::Triggered, this, &AShellProjectCharacter::MoveForward);
	}
	if (MoveRightAction)
	{
		EnhancedInput->BindAction(MoveRightAction, ETriggerEvent::Triggered, this, &AShellProjectCharacter::MoveRight);
	}
	if (LookAction)
	{
		EnhancedInput->BindAction(LookAction, ETriggerEvent::Triggered, this, &AShellProjectCharacter::Look);
	}
	if (JumpAction)
	{
		EnhancedInput->BindAction(JumpAction, ETriggerEvent::Started, this, &AShellProjectCharacter::HandleJump);
		EnhancedInput->BindAction(JumpAction, ETriggerEvent::Completed, this, &AShellProjectCharacter::HandleJumpReleased);
	}
	// 左键点击/滚轮不再在此绑定：UShellWorldScreen 自包含指针输入
	// 由 PlayerController 的 InputWindow 态（SetInputActive）统一激活。
}

void AShellProjectCharacter::EnsureInputBuilt()
{
	if (MoveForwardAction)
	{
		return; // 幂等
	}

	// 1D 前后/左右动作（避免 2D 键盘 swizzle 把 W/S 也投到 X 轴）。
	MoveForwardAction = NewObject<UInputAction>(this, TEXT("MoveForwardAction"));
	MoveForwardAction->ValueType = EInputActionValueType::Axis1D;
	MoveRightAction = NewObject<UInputAction>(this, TEXT("MoveRightAction"));
	MoveRightAction->ValueType = EInputActionValueType::Axis1D;
	LookAction = NewObject<UInputAction>(this, TEXT("LookAction"));
	LookAction->ValueType = EInputActionValueType::Axis2D;
	JumpAction = NewObject<UInputAction>(this, TEXT("JumpAction"));
	JumpAction->ValueType = EInputActionValueType::Boolean;

	CharacterMappingContext = NewObject<UInputMappingContext>(this, TEXT("CharacterMappingContext"));
	CharacterMappingContext->MapKey(MoveForwardAction, EKeys::W);                 // 前
	FEnhancedActionKeyMapping& SMapping = CharacterMappingContext->MapKey(MoveForwardAction, EKeys::S);
	SMapping.Modifiers.Add(NewObject<UInputModifierNegate>(this));                 // 后
	CharacterMappingContext->MapKey(MoveRightAction, EKeys::D);                   // 右
	FEnhancedActionKeyMapping& AMapping = CharacterMappingContext->MapKey(MoveRightAction, EKeys::A);
	AMapping.Modifiers.Add(NewObject<UInputModifierNegate>(this));                 // 左
	CharacterMappingContext->MapKey(LookAction, EKeys::Mouse2D);                  // 视角
	CharacterMappingContext->MapKey(JumpAction, EKeys::SpaceBar);                 // 跳
}

void AShellProjectCharacter::BuildAndAddCharacterInputMapping()
{
	EnsureInputBuilt();

	if (!CharacterMappingContext)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ShellProject] CharacterMappingContext 未构建"));
		return;
	}

	APlayerController* PC = Cast<APlayerController>(GetController());
	ULocalPlayer* LocalPlayer = PC ? PC->GetLocalPlayer() : nullptr;
	if (!LocalPlayer)
	{
		return;
	}

	if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
	{
		// 优先级 1：与 Shell 的 IMC_Shell(0) 并存，互不冲突。
		InputSubsystem->AddMappingContext(CharacterMappingContext, 1);
	}
}

void AShellProjectCharacter::MoveForward(const FInputActionValue& Value)
{
	// 输入接管（InputWindow 态）：忽略 WASD，把键盘让给 UI 交互。
	if (IsShellInputActive())
	{
		return;
	}

	const float Axis = Value.Get<float>();

	AController* PC = GetController();
	if (!PC)
	{
		return;
	}

	// 以控制器（相机）偏航为基准的前进方向：+前 -后。
	const FRotator YawRotation(0.f, PC->GetControlRotation().Yaw, 0.f);
	const FVector Forward = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	AddMovementInput(Forward, Axis);
}

void AShellProjectCharacter::MoveRight(const FInputActionValue& Value)
{
	// 输入接管：忽略 A/D，把键盘让给 UI 交互。
	if (IsShellInputActive())
	{
		return;
	}

	const float Axis = Value.Get<float>();

	AController* PC = GetController();
	if (!PC)
	{
		return;
	}

	// 以控制器（相机）偏航为基准的右向：+右 -左。
	const FRotator YawRotation(0.f, PC->GetControlRotation().Yaw, 0.f);
	const FVector Right = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
	AddMovementInput(Right, Axis);
}

void AShellProjectCharacter::Look(const FInputActionValue& Value)
{
	// 输入接管：忽略鼠标视角，把鼠标让给 UI 交互（点击/滚轮）。
	if (IsShellInputActive())
	{
		return;
	}

	const FVector2D Axis = Value.Get<FVector2D>();

	AddControllerYawInput(Axis.X);
	AddControllerPitchInput(-Axis.Y); // 鼠标上移 = 抬头
}

void AShellProjectCharacter::HandleJump()
{
	Jump();
}

void AShellProjectCharacter::HandleJumpReleased()
{
	StopJumping();
}
