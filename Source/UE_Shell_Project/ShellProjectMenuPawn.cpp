#include "ShellProjectMenuPawn.h"

#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"

AShellProjectMenuPawn::AShellProjectMenuPawn()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(Root);
	Camera->SetFieldOfView(70.f);
}
