#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayEffect.h"

#include "ShellProjectAbility.generated.h"

class AShellFireballProjectile;

/**
 * 示例能力基类：向 Shell 终端打印反馈（宿主侧的带外输出桥）。
 */
UCLASS(Abstract)
class UE_SHELL_PROJECT_API UShellProjectAbility : public UGameplayAbility
{
	GENERATED_BODY()

protected:
	/** 向终端输出一行（找不到 Shell 子系统时静默跳过，例如无 GameInstance 的测试）。 */
	void PrintToShell(const FString& Line, bool bError = false) const;
};

/** 火球成本 GE（Instant，Mana -10）。原生类：无需内容资产。 */
UCLASS()
class UE_SHELL_PROJECT_API UShellFireballCostGE : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UShellFireballCostGE();
};

/** 火球冷却 GE（HasDuration 3s，授予 Cooldown.Fireball）。 */
UCLASS()
class UE_SHELL_PROJECT_API UShellFireballCooldownGE : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UShellFireballCooldownGE();
};

/**
 * 示例技能：Fireball —— Commit（成本+冷却）后从视角方向发射一发投射物。
 * 由 DT_ShellAbilities 的 fireball 行按 Tag=Ability.Skill.Fireball 激活。
 */
UCLASS()
class UE_SHELL_PROJECT_API UShellFireballAbility : public UShellProjectAbility
{
	GENERATED_BODY()

public:
	UShellFireballAbility();

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
};
