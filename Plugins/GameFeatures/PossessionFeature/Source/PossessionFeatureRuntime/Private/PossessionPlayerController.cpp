// Fill out your copyright notice in the Description page of Project Settings.


#include "PossessionPlayerController.h"

#include "Character/LyraCharacter.h"
#include "Character/LyraHeroComponent.h"

void APossessionPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	
	if (const ALyraCharacter* LyraCharacter = GetPawn<ALyraCharacter>())
	{
		if (ULyraHeroComponent* HeroComponent = ULyraHeroComponent::FindHeroComponent(LyraCharacter))
		{
			if (InputComponent != nullptr)
			{
				if (HeroComponent->IsReadyToBindInputs())
				{
					HeroComponent->ResetInputs(this);
				}
				UE_LOG(LogTemp, Warning, TEXT("%s - %s(): Initializing player input"), *GetName(), *FString(__FUNCTION__));
				HeroComponent->InitializePlayerInput(InputComponent);
			}
		}
	}
	
}

void APossessionPlayerController::OnUnPossess()
{
	if (const ALyraCharacter* LyraCharacter = GetPawn<ALyraCharacter>())
	{
		if (ULyraHeroComponent* HeroComponent = ULyraHeroComponent::FindHeroComponent(LyraCharacter))
		{
			if (InputComponent != nullptr)
			{
				HeroComponent->ResetInputs(this);
			}
		}
	}
	
	Super::OnUnPossess();
}
