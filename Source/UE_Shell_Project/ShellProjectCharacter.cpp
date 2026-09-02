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

	// 双实例-世界实例："手持屏幕"。挂角色根，默认隐藏（仅 HeldInHand 态显示）。
	// DrawSize 固定为渲染分辨率（不随内容自适应）；世界缩放拉近到可读尺寸。
	ShellScreen = CreateDefaultSubobject<UWidgetComponent>(TEXT("ShellScreen"));
    ShellScreen->SetupAttachment(FollowCamera);
	ShellScreen->SetWidgetClass(UShellTerminalWidget::StaticClass());
	ShellScreen->SetDrawSize(FVector2D(128.f, 80.f));
	ShellScreen->SetRelativeLocation(FVector(70.f, -40.f, -15.f));
	ShellScreen->SetRelativeScale3D(FVector(0.5f));
	ShellScreen->SetVisibility(false); // 初始隐藏
	ShellScreen->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ShellScreen->SetGenerateOverlapEvents(false);
	ShellScreen->SetTwoSided(true);

	// 输入动作 + IMC 在运行时惰性构建（构造器内 NewObject 会触发
	// UObjectGlobals.cpp:4880 致命错误——见 EnsureInputBuilt）。
}

UShellTerminalWidget* AShellProjectCharacter::GetShellScreenWidget() const
{
	return ShellScreen ? Cast<UShellTerminalWidget>(ShellScreen->GetWidget()) : nullptr;
}

void AShellProjectCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateShellScreenBillboard();
}

void AShellProjectCharacter::UpdateShellScreenBillboard()
{
	if (!ShellScreen || !ShellScreen->IsVisible() || !ShellScreen->IsRegistered())
	{
		return;
	}

	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;

	// 以玩家视角为相机参考（此处用 Pawn 位置近似，避免对玩家屏蔽遮挡）。
    const FVector CameraLoc = FollowCamera->GetComponentLocation(); // 面板朝向相机
	const FVector WidgetLoc = ShellScreen->GetComponentLocation();

	// billboard：让组件 +X 由屏幕指向相机（面板正面朝玩家）。
	const FVector ToCamera = CameraLoc - WidgetLoc;
	FRotator Rot = FRotationMatrix::MakeFromX(ToCamera).Rotator();

	// 手持倾角：轻微往下前倾 + 一点滚动，营造"拿在手里"而非"贴墙海报"。
	Rot.Pitch -= 8.f;
	Rot.Roll += 6.f;

	ShellScreen->SetWorldRotation(Rot);
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
