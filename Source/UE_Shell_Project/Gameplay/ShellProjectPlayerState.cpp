#include "ShellProjectPlayerState.h"

#include "Abilities/GameplayAbility.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/Pawn.h"

#include "ShellProjectAbility.h"
#include "ShellProjectAttributeSet.h"

AShellProjectPlayerState::AShellProjectPlayerState()
{
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	// 单机/独立 PIE：Minimal 即可。
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	AttributeSet = CreateDefaultSubobject<UShellProjectAttributeSet>(TEXT("AttributeSet"));

	StartupAbilities.Add(UShellFireballAbility::StaticClass());
}

UAbilitySystemComponent* AShellProjectPlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void AShellProjectPlayerState::BeginPlay()
{
	Super::BeginPlay();

	if (AbilitySystemComponent)
	{
		// 世界 BeginPlay 晚于初始 Possess（PS::PawnPtr 此时已就位）——
		// 若无条件 Init(this, this) 会把 PossessedBy 绑好的 Avatar
		// 覆盖回 PlayerState，导致能力的 AvatarActor 不是角色。
		APawn* MyPawn = GetPawn();
		AbilitySystemComponent->InitAbilityActorInfo(this, MyPawn ? static_cast<AActor*>(MyPawn) : this);

		// 权限侧授予初始能力（GiveAbility 仅服务器；独立 PIE 即服务器）。
		if (GetLocalRole() == ROLE_Authority)
		{
			for (const TSubclassOf<UGameplayAbility>& AbilityClass : StartupAbilities)
			{
				if (AbilityClass)
				{
					AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(AbilityClass, /*Level=*/1, /*InputID=*/INDEX_NONE, this));
				}
			}
		}
	}
}
