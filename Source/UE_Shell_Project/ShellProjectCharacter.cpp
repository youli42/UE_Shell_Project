#include "ShellProjectCharacter.h"

#include "AbilitySystemComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/SpringArmComponent.h"
#include "UObject/ConstructorHelpers.h"

#include "ShellProjectPlayerState.h"

AShellProjectCharacter::AShellProjectCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));

	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(GetCapsuleComponent());
	if (CubeMesh.Succeeded())
	{
		BodyMesh->SetStaticMesh(CubeMesh.Object);
	}
	BodyMesh->SetWorldScale3D(FVector(0.35f, 0.35f, 0.7f));
	BodyMesh->SetRelativeLocation(FVector(0.f, 0.f, 40.f));

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 420.f;
	CameraBoom->SetRelativeRotation(FRotator(-20.f, 0.f, 0.f));
	CameraBoom->bEnableCameraLag = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
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
