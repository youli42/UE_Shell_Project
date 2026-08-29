#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"

#include "ShellProjectCharacter.generated.h"

class UCameraComponent;
class UStaticMeshComponent;
class USpringArmComponent;

/**
 * 最小第三人称角色（GAS 示范）：ASC 由 PlayerState 持有并转发，
 * PossessedBy 时把 Avatar 切到本角色。视觉用引擎基础方块
 * （模板工程无人形网格体），弹簧臂 + 相机。
 */
UCLASS()
class UE_SHELL_PROJECT_API AShellProjectCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AShellProjectCharacter();

	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

private:
	/** PlayerState 侧 ASC 的 Avatar 绑定（客户端/本地共用）。 */
	void BindAbilityAvatar();

	UPROPERTY(VisibleAnywhere, Category = "Shell Project")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, Category = "Shell Project")
	TObjectPtr<UCameraComponent> FollowCamera;

	UPROPERTY(VisibleAnywhere, Category = "Shell Project")
	TObjectPtr<UStaticMeshComponent> BodyMesh;
};
