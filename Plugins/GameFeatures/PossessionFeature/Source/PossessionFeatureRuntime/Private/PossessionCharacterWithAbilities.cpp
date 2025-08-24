// Fill out your copyright notice in the Description page of Project Settings.


#include "PossessionCharacterWithAbilities.h"

#include "AbilitySystem/LyraAbilitySet.h"
#include "Character/LyraHeroComponent.h"
#include "Character/LyraPawnData.h"
#include "Character/LyraPawnExtensionComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PossessionCharacterWithAbilities)

APossessionCharacterWithAbilities::APossessionCharacterWithAbilities(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	HeroComponent = CreateDefaultSubobject<ULyraHeroComponent>(TEXT("HeroComponent"));
}

void APossessionCharacterWithAbilities::BeginPlay()
{
	Super::BeginPlay();

	if (ULyraPawnExtensionComponent* PawnExtensionComponent = ULyraPawnExtensionComponent::FindPawnExtensionComponent(this))
	{
		if (PawnData && PawnExtensionComponent->GetPawnData<ULyraPawnData>() == nullptr)
		{
			PawnExtensionComponent->SetPawnData(PawnData);
		}

		if (const ULyraPawnData* CurrentPawnData = PawnExtensionComponent->GetPawnData<ULyraPawnData>())
		{
			for (const ULyraAbilitySet* AbilitySet : CurrentPawnData->AbilitySets)
			{
				if (AbilitySet)
				{
					AbilitySet->GiveToAbilitySystem(GetLyraAbilitySystemComponent(), nullptr);
				}
			}

			ForceNetUpdate();
		}
	}
	

}
