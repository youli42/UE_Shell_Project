#include "ShellFireballProjectile.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "UObject/ConstructorHelpers.h"

AShellFireballProjectile::AShellFireballProjectile()
{
	PrimaryActorTick.bCanEverTick = false;
	InitialLifeSpan = 3.f;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	if (SphereMesh.Succeeded())
	{
		Mesh->SetStaticMesh(SphereMesh.Object);
	}
	Mesh->SetWorldScale3D(FVector(0.3f));
	Mesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));

	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(Mesh);
	Light->SetIntensity(8000.f);
	Light->SetAttenuationRadius(600.f);
	Light->SetLightFColor(FColor(255, 140, 40));

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->UpdatedComponent = Mesh;
	Movement->InitialSpeed = 2600.f;
	Movement->MaxSpeed = 2600.f;
	Movement->bRotationFollowsVelocity = true;
	Movement->ProjectileGravityScale = 0.f;
}
