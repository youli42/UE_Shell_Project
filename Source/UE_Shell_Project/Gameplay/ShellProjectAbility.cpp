#include "ShellProjectAbility.h"

#include "AbilitySystemComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "ScalableFloat.h"
#include "Shell/Terminal/ShellSubsystem.h"

#include "ShellFireballProjectile.h"
#include "ShellProjectAttributeSet.h"

void UShellProjectAbility::PrintToShell(const FString& Line, bool bError) const
{
	const UObject* WorldContext = GetCurrentActorInfo() ? GetCurrentActorInfo()->AbilitySystemComponent.Get() : nullptr;
	const UWorld* World = GEngine && WorldContext ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	if (UShellSubsystem* Shell = GameInstance ? GameInstance->GetSubsystem<UShellSubsystem>() : nullptr)
	{
		Shell->Print(Line, bError ? EShellOutputType::Error : EShellOutputType::Success);
	}
}

UShellFireballCostGE::UShellFireballCostGE()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FGameplayModifierInfo ManaCost;
	ManaCost.Attribute = UShellProjectAttributeSet::GetManaAttribute();
	ManaCost.ModifierOp = EGameplayModOp::Additive;
	ManaCost.ModifierMagnitude = FScalableFloat(-10.f);
	Modifiers.Add(ManaCost);
}

UShellFireballCooldownGE::UShellFireballCooldownGE()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FScalableFloat(3.f);

	// UE 5.8：冷却标签必须"授予目标角色"（GE GrantedTags）——
	// 引擎 CheckCooldown 用 ASC->HasAnyMatchingGameplayTags(CooldownTags)
	// （GameplayAbility.cpp:1078），其中 CooldownTags = GE->GetGrantedTags()
	// （GameplayAbility.cpp:1216-1220）。
	// 原生 CDO 构造中不能用 AddComponent<>()（其内部 NewObject 空名会 Fatal）；
	// 正确做法：CreateDefaultSubobject 后手动塞进 protected GEComponents
	// （引擎校验错误即由此触发），再应用标签。
	const FGameplayTag CooldownTag = FGameplayTag::RequestGameplayTag(TEXT("Cooldown.Fireball"), /*ErrorIfNotFound=*/false);
	if (CooldownTag.IsValid())
	{
		UTargetTagsGameplayEffectComponent* TargetTags = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("CooldownTargetTags"));
		GEComponents.Add(TargetTags);
		FInheritedTagContainer TagChanges;
		TagChanges.AddTag(CooldownTag);
		TargetTags->SetAndApplyTargetTagChanges(TagChanges);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[Fireball] Cooldown.Fireball tag NOT registered at GE CDO construction"));
	}
}

UShellFireballAbility::UShellFireballAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	// 注意：原生 CDO 构造发生得很早 —— 若此时 GameplayTag 字典尚未加载
	// （RequestGameplayTag 返回空），标签匹配（TryActivateAbilitiesByTag）
	// 将永远失败，症状是 "activation refused"。这里显式记录以便诊断。
	const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(TEXT("Ability.Skill.Fireball"), /*ErrorIfNotFound=*/false);
	if (Tag.IsValid())
	{
		FGameplayTagContainer AssetTags;
		AssetTags.AddTag(Tag);
		SetAssetTags(AssetTags);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[Fireball] Ability.Skill.Fireball tag NOT registered at ability CDO construction"));
	}

	CostGameplayEffectClass = UShellFireballCostGE::StaticClass();
	CooldownGameplayEffectClass = UShellFireballCooldownGE::StaticClass();
}

void UShellFireballAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		PrintToShell(TEXT("Fireball: activation refused (cost/cooldown)"), true);
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/true);
		return;
	}

	// 表现：从角色视角发射一枚发光投射物。
	if (ActorInfo && ActorInfo->AvatarActor.IsValid())
	{
		if (const ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get()))
		{
			if (UWorld* World = Character->GetWorld())
			{
				const UCameraComponent* Camera = Character->FindComponentByClass<UCameraComponent>();
				const FTransform SpawnTransform = Camera
					? FTransform(Camera->GetComponentRotation(), Camera->GetComponentLocation() + Camera->GetForwardVector() * 80.f)
					: Character->GetActorTransform();
				World->SpawnActor<AShellFireballProjectile>(AShellFireballProjectile::StaticClass(), SpawnTransform);
			}
		}
	}

	PrintToShell(TEXT("Fireball: whoosh! (-10 mana)"));
	EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/false);
}
