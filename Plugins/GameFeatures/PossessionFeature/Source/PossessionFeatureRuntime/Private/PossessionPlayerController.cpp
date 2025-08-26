// Fill out your copyright notice in the Description page of Project Settings.


#include "PossessionPlayerController.h"

#include "Character/LyraCharacter.h"
#include "Character/LyraHeroComponent.h"
#include "Input/LyraInputComponent.h"

void APossessionPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	// TODO: Figure out a better way than this
	GetWorldTimerManager().SetTimerForNextTick(this, &ThisClass::ReapplyInput);

}

void APossessionPlayerController::OnUnPossess()
{
	if (const ALyraCharacter* LyraCharacter = GetPawn<ALyraCharacter>())
	{
		if (ULyraHeroComponent* HeroComponent = ULyraHeroComponent::FindHeroComponent(LyraCharacter))
		{
			if (InputComponent != nullptr)
			{
				HeroComponent->ResetInputs(this, true);
			}
		}
	}
	
	Super::OnUnPossess();
}

void APossessionPlayerController::ReapplyInput()
{
	if (const ALyraCharacter* LyraCharacter = GetPawn<ALyraCharacter>())
	{
		if (ULyraHeroComponent* HeroComponent = ULyraHeroComponent::FindHeroComponent(LyraCharacter))
		{
			if (InputComponent != nullptr)
			{
				if (HeroComponent->IsReadyToBindInputs())
				{
					return;
				}
					
				HeroComponent->InitializePlayerInput(InputComponent);
			}
		}
	}
}
