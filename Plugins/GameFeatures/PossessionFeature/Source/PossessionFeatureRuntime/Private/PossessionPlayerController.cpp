// Fill out your copyright notice in the Description page of Project Settings.


#include "PossessionPlayerController.h"

#include "Character/LyraCharacter.h"
#include "Character/LyraHeroComponent.h"
#include "Input/LyraInputComponent.h"

void APossessionPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	TicksNeeded = 0;
	
	//GetWorldTimerManager().SetTimer(TimerHandle, this, &ThisClass::ResetApplyInput, 1.0, true, 1.0f);
	GetWorldTimerManager().SetTimerForNextTick(this, &ThisClass::ResetApplyInput);

	//ResetApplyInput();
	
}

void APossessionPlayerController::OnUnPossess()
{
	if (TimerHandle.IsValid())
	{
		GetWorldTimerManager().ClearTimer(TimerHandle);
	}
	if (const ALyraCharacter* LyraCharacter = GetPawn<ALyraCharacter>())
	{
		if (ULyraHeroComponent* HeroComponent = ULyraHeroComponent::FindHeroComponent(LyraCharacter))
		{
			if (InputComponent != nullptr)
			{
				UE_LOG(LogTemp, Warning, TEXT("%s(): Removing Inputs"), *FString(__FUNCTION__));
				//HeroComponent->ResetInputs(this, true);
				HeroComponent->ResetInputs(this, true, true);
			}
		}
	}
	
	Super::OnUnPossess();
}

void APossessionPlayerController::ResetApplyInput()
{
	if (const ALyraCharacter* LyraCharacter = GetPawn<ALyraCharacter>())
	{
		if (ULyraHeroComponent* HeroComponent = ULyraHeroComponent::FindHeroComponent(LyraCharacter))
		{
			if (InputComponent != nullptr)
			{
				if (ULyraInputComponent* LyraIC = GetPawn<APawn>()->FindComponentByClass<ULyraInputComponent>())
				{
					if (HeroComponent->IsReadyToBindInputs())
					{
						return;
					}
					
					/*if (!bOnce)
					{
						bOnce = true;
						HeroComponent->ResetInputs(this, true, false);
					}*/
					
					//if (HeroComponent->IsReadyToBindInputs())
					{
						//UE_LOG(LogTemp, Warning, TEXT("%s(): Ready to bind"), *FString(__FUNCTION__));
						//HeroComponent->ResetInputs(this);
					}

					/*if (LyraIC->GetActionEventBindings().Num() > 0)
					{
						FString Event;
						for (const auto& Binding : LyraIC->GetActionEventBindings())
						{
							Event.Append(Binding.Get()->GetAction()->GetName());
							Event.Append(", ");
						}
						UE_LOG(LogTemp, Warning, TEXT("%s(): Still have %d inputs: %s"), *FString(__FUNCTION__), LyraIC->GetActionEventBindings().Num(), *Event);
						HeroComponent->ResetInputs(this, true, true);
						
						
						//HeroComponent->ResetInputs(this, true);
						TicksNeeded++;
						//GetWorldTimerManager().SetTimerForNextTick(this, &ThisClass::ResetApplyInput);
						return;
					}

					UE_LOG(LogTemp, Warning, TEXT("%s(): %d ticks needed to reset"), *FString(__FUNCTION__), TicksNeeded);

					if (TimerHandle.IsValid())
					{
						GetWorldTimerManager().ClearTimer(TimerHandle);
					}*/

					//HeroComponent->ResetInputs(this, true, true);
					HeroComponent->InitializePlayerInput(InputComponent);

					/*if (LyraIC->GetActionEventBindings().Num() > 0)
					{
						UE_LOG(LogTemp, Warning, TEXT("%s(): Still have inputs"), *FString(__FUNCTION__));
					}*/
					
					//HeroComponent->ResetInputs(this, true);
					//HeroComponent->InitializePlayerInput(InputComponent);
					
					

					/*if (LyraIC->GetActionEventBindings().Num() > 0)
					{
						//UE_LOG(LogTemp, Warning, TEXT("%s(): Still have inputs after reset"), *FString(__FUNCTION__));
						UE_LOG(LogTemp, Warning, TEXT("%s - %s(): Initializing player input"), *GetName(), *FString(__FUNCTION__));
						HeroComponent->InitializePlayerInput(InputComponent);
					}*/
				}
			}
		}
	}
}
