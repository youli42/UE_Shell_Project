#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "ShellFireballProjectile.generated.h"

class UPointLightComponent;
class UProjectileMovementComponent;
class UStaticMeshComponent;

/** Fireball 的视觉投射物：发光小球直线飞行，3 秒后自毁。 */
UCLASS()
class UE_SHELL_PROJECT_API AShellFireballProjectile : public AActor
{
	GENERATED_BODY()

public:
	AShellFireballProjectile();

private:
	UPROPERTY(VisibleAnywhere, Category = "Shell Project")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, Category = "Shell Project")
	TObjectPtr<UProjectileMovementComponent> Movement;

	UPROPERTY(VisibleAnywhere, Category = "Shell Project")
	TObjectPtr<UPointLightComponent> Light;
};
