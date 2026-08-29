#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/PlayerState.h"

#include "ShellProjectPlayerState.generated.h"

class UAbilitySystemComponent;
class UGameplayAbility;
class UShellProjectAttributeSet;

/**
 * GAS 接线示范（doc 05）：ASC 挂 PlayerState（死亡/重生存活），
 * Minimal 复制模式（单机），登录后由 Shell_UE 的 cast 命令按 Tag 激活。
 */
UCLASS()
class UE_SHELL_PROJECT_API AShellProjectPlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AShellProjectPlayerState();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Shell Project")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<UShellProjectAttributeSet> AttributeSet;

	/** 初始授予的能力（默认含示例 Fireball；宿主可自行增删）。 */
	UPROPERTY(EditDefaultsOnly, Category = "Shell Project")
	TArray<TSubclassOf<UGameplayAbility>> StartupAbilities;
};
