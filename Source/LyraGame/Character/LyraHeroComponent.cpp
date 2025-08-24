// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraHeroComponent.h"
#include "Components/GameFrameworkComponentDelegates.h"
#include "Logging/MessageLog.h"
#include "LyraLogChannels.h"
#include "EnhancedInputSubsystems.h"
#include "Player/LyraPlayerController.h"
#include "Player/LyraPlayerState.h"
#include "Player/LyraLocalPlayer.h"
#include "Character/LyraPawnExtensionComponent.h"
#include "Character/LyraPawnData.h"
#include "Character/LyraCharacter.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "Input/LyraInputConfig.h"
#include "Input/LyraInputComponent.h"
#include "Camera/LyraCameraComponent.h"
#include "LyraGameplayTags.h"
#include "Components/GameFrameworkComponentManager.h"
#include "PlayerMappableInputConfig.h"
#include "Camera/LyraCameraMode.h"
#include "UserSettings/EnhancedInputUserSettings.h"
#include "InputMappingContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraHeroComponent)

#if WITH_EDITOR
#include "Misc/UObjectToken.h"
#endif	// WITH_EDITOR

namespace LyraHero
{
	static const float LookYawRate = 300.0f;
	static const float LookPitchRate = 165.0f;
};

const FName ULyraHeroComponent::NAME_BindInputsNow("BindInputsNow");
const FName ULyraHeroComponent::NAME_ActorFeatureName("Hero");

ULyraHeroComponent::ULyraHeroComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AbilityCameraMode = nullptr;
	bReadyToBindInputs = false;
}

void ULyraHeroComponent::OnRegister()
{
	Super::OnRegister();

	if (!GetPawn<APawn>())
	{
		UE_LOG(LogLyra, Error, TEXT("[ULyraHeroComponent::OnRegister] This component has been added to a blueprint whose base class is not a Pawn. To use this component, it MUST be placed on a Pawn Blueprint."));

#if WITH_EDITOR
		if (GIsEditor)
		{
			static const FText Message = NSLOCTEXT("LyraHeroComponent", "NotOnPawnError", "has been added to a blueprint whose base class is not a Pawn. To use this component, it MUST be placed on a Pawn Blueprint. This will cause a crash if you PIE!");
			static const FName HeroMessageLogName = TEXT("LyraHeroComponent");
			
			FMessageLog(HeroMessageLogName).Error()
				->AddToken(FUObjectToken::Create(this, FText::FromString(GetNameSafe(this))))
				->AddToken(FTextToken::Create(Message));
				
			FMessageLog(HeroMessageLogName).Open();
		}
#endif
	}
	else
	{
		//@EditBegin
		const FString PawnName = GetPawn<APawn>() != nullptr ? GetPawn<APawn>()->GetName() : FString("None");
		UE_VLOG(this, LogLyra, VeryVerbose, TEXT("%s - %s(): Registering"), *PawnName, *FString(__FUNCTION__));
		//@EditEnd
		
		// Register with the init state system early, this will only work if this is a game world
		RegisterInitStateFeature();
	}
}

bool ULyraHeroComponent::CanChangeInitState(UGameFrameworkComponentManager* Manager, FGameplayTag CurrentState, FGameplayTag DesiredState) const
{
	check(Manager);

	APawn* Pawn = GetPawn<APawn>();

	//@EditBegin-Ignore this
	const FString PawnName = GetPawn<APawn>() != nullptr ? GetPawn<APawn>()->GetName() : FString("None");
	UE_VLOG(this, LogLyra, VeryVerbose, TEXT("%s - %s(): Entry"), *PawnName, *FString(__FUNCTION__));
	//@EditEnd

	if (!CurrentState.IsValid() && DesiredState == LyraGameplayTags::InitState_Spawned)
	{
		
		UE_VLOG(this, LogLyra, VeryVerbose, TEXT("%s - %s(): Current state not valid, InitStat_Spawned"), *PawnName, *FString(__FUNCTION__));
		// As long as we have a real pawn, let us transition
		if (Pawn)
		{
			UE_VLOG(this, LogLyra, VeryVerbose, TEXT("%s - %s(): Valid pawn, returning true"), *PawnName, *FString(__FUNCTION__));
			return true;
		}
	}
	else if (CurrentState == LyraGameplayTags::InitState_Spawned && DesiredState == LyraGameplayTags::InitState_DataAvailable)
	{
		UE_VLOG(this, LogLyra, VeryVerbose, TEXT("%s - %s(): Current state is spawned, desired data is available"), *PawnName, *FString(__FUNCTION__));
		// The player state is required.
		if (!GetPlayerState<ALyraPlayerState>())
		{
			UE_VLOG(this, LogLyra, VeryVerbose, TEXT("%s - %s(): No player state, returning false"), *PawnName, *FString(__FUNCTION__));
			return false;
		}

		// If we're authority or autonomous, we need to wait for a controller with registered ownership of the player state.
		if (Pawn->GetLocalRole() != ROLE_SimulatedProxy)
		{
			UE_VLOG(this, LogLyra, VeryVerbose, TEXT("%s - %s(): Not simulated proxy"), *PawnName, *FString(__FUNCTION__));
			AController* Controller = GetController<AController>();

			const bool bHasControllerPairedWithPS = (Controller != nullptr) && \
				(Controller->PlayerState != nullptr) && \
				(Controller->PlayerState->GetOwner() == Controller);

			if (!bHasControllerPairedWithPS)
			{
				UE_VLOG(this, LogLyra, VeryVerbose, TEXT("%s - %s(): Controller has not paired with player state, returning false"), *PawnName, *FString(__FUNCTION__));
				return false;
			}
		}

		const bool bIsLocallyControlled = Pawn->IsLocallyControlled();
		const bool bIsBot = Pawn->IsBotControlled();

		if (bIsLocallyControlled && !bIsBot)
		{
			UE_VLOG(this, LogLyra, VeryVerbose, TEXT("%s - %s(): Locally controlled and not a bot"), *PawnName, *FString(__FUNCTION__));
			ALyraPlayerController* LyraPC = GetController<ALyraPlayerController>();

			// The input component and local player is required when locally controlled.
			if (!Pawn->InputComponent || !LyraPC || !LyraPC->GetLocalPlayer())
			{
				UE_VLOG(this, LogLyra, VeryVerbose, TEXT("%s - %s(): No input component found, returning false"), *PawnName, *FString(__FUNCTION__));
				return false;
			}
		}

		return true;
	}
	else if (CurrentState == LyraGameplayTags::InitState_DataAvailable && DesiredState == LyraGameplayTags::InitState_DataInitialized)
	{
		UE_VLOG(this, LogLyra, VeryVerbose, TEXT("%s - %s(): Current state is data available, desired state is data initialized"), *PawnName, *FString(__FUNCTION__));
		// Wait for player state and extension component
		ALyraPlayerState* LyraPS = GetPlayerState<ALyraPlayerState>();

		return LyraPS && Manager->HasFeatureReachedInitState(Pawn, ULyraPawnExtensionComponent::NAME_ActorFeatureName, LyraGameplayTags::InitState_DataInitialized);
	}
	else if (CurrentState == LyraGameplayTags::InitState_DataInitialized && DesiredState == LyraGameplayTags::InitState_GameplayReady)
	{
		UE_VLOG(this, LogLyra, VeryVerbose, TEXT("%s - %s(): Current state is data initialized, desired state is gameplay ready, returning true"), *PawnName, *FString(__FUNCTION__));
		// TODO add ability initialization checks?
		return true;
	}

	UE_VLOG(this, LogLyra, VeryVerbose, TEXT("%s - %s(): Fell through to default returning false"), *PawnName, *FString(__FUNCTION__));

	return false;
}

void ULyraHeroComponent::HandleChangeInitState(UGameFrameworkComponentManager* Manager, FGameplayTag CurrentState, FGameplayTag DesiredState)
{
	//@EditBegin
	const FString PawnName = GetPawn<APawn>() != nullptr ? GetPawn<APawn>()->GetName() : FString("None");
	UE_VLOG(this, LogLyra, VeryVerbose, TEXT("%s - %s(): Entry"), *PawnName, *FString(__FUNCTION__));
	//@EditEnd
	
	if (CurrentState == LyraGameplayTags::InitState_DataAvailable && DesiredState == LyraGameplayTags::InitState_DataInitialized)
	{
		UE_VLOG(this, LogLyra, VeryVerbose, TEXT("%s - %s(): InitState_DataAvailable, desired Data Initialized"), *PawnName, *FString(__FUNCTION__));
		APawn* Pawn = GetPawn<APawn>();
		ALyraPlayerState* LyraPS = GetPlayerState<ALyraPlayerState>();
		if (!ensure(Pawn && LyraPS))
		{
			return;
		}

		const ULyraPawnData* PawnData = nullptr;

		if (ULyraPawnExtensionComponent* PawnExtComp = ULyraPawnExtensionComponent::FindPawnExtensionComponent(Pawn))
		{
			UE_VLOG(this, LogLyra, VeryVerbose, TEXT("%s - %s(): Grabbing pawn data from PawnExtComp and initializing ASC"), *PawnName, *FString(__FUNCTION__));
			PawnData = PawnExtComp->GetPawnData<ULyraPawnData>();

			// The player state holds the persistent data for this player (state that persists across deaths and multiple pawns).
			// The ability system component and attribute sets live on the player state.
			PawnExtComp->InitializeAbilitySystem(LyraPS->GetLyraAbilitySystemComponent(), LyraPS);
		}

		if (ALyraPlayerController* LyraPC = GetController<ALyraPlayerController>())
		{
			if (Pawn->InputComponent != nullptr)
			{
				UE_VLOG(this, LogLyra, VeryVerbose, TEXT("%s - %s(): Initializing player input"), *PawnName, *FString(__FUNCTION__));
				InitializePlayerInput(Pawn->InputComponent);
			}
		}

		// Hook up the delegate for all pawns, in case we spectate later
		if (PawnData)
		{
			if (ULyraCameraComponent* CameraComponent = ULyraCameraComponent::FindCameraComponent(Pawn))
			{
				UE_VLOG(this, LogLyra, VeryVerbose, TEXT("%s - %s(): Camera mode delegate binding"), *PawnName, *FString(__FUNCTION__));
				CameraComponent->DetermineCameraModeDelegate.BindUObject(this, &ThisClass::DetermineCameraMode);
			}
		}
	}

	UE_VLOG(this, LogLyra, VeryVerbose, TEXT("%s - %s(): Exit"), *PawnName, *FString(__FUNCTION__));
}

void ULyraHeroComponent::OnActorInitStateChanged(const FActorInitStateChangedParams& Params)
{
	if (Params.FeatureName == ULyraPawnExtensionComponent::NAME_ActorFeatureName)
	{
		if (Params.FeatureState == LyraGameplayTags::InitState_DataInitialized)
		{
			//@EditBegin-Ignore this
			const FString PawnName = GetPawn<APawn>() != nullptr ? GetPawn<APawn>()->GetName() : FString("None");
			UE_VLOG(this, LogLyra, VeryVerbose, TEXT("%s - %s(): Calling CheckDefaultInitialization"), *PawnName, *FString(__FUNCTION__));
			//@EditEnd
			// If the extension component says all all other components are initialized, try to progress to next state
			CheckDefaultInitialization();
		}
	}
}

void ULyraHeroComponent::CheckDefaultInitialization()
{
	static const TArray<FGameplayTag> StateChain = { LyraGameplayTags::InitState_Spawned, LyraGameplayTags::InitState_DataAvailable, LyraGameplayTags::InitState_DataInitialized, LyraGameplayTags::InitState_GameplayReady };

	//@EditBegin-Ignore this
	const FString PawnName = GetPawn<APawn>() != nullptr ? GetPawn<APawn>()->GetName() : FString("None");
	UE_VLOG(this, LogLyra, VeryVerbose, TEXT("%s - %s(): Continuing init state chain"), *PawnName, *FString(__FUNCTION__));
	//@EditEnd
	
	// This will try to progress from spawned (which is only set in BeginPlay) through the data initialization stages until it gets to gameplay ready
	ContinueInitStateChain(StateChain);
}

void ULyraHeroComponent::BeginPlay()
{
	Super::BeginPlay();

	//@EditBegin-Ignore this
	const FString PawnName = GetPawn<APawn>() != nullptr ? GetPawn<APawn>()->GetName() : FString("None");
	UE_VLOG(this, LogLyra, VeryVerbose, TEXT("%s - %s(): BeginPlay"), *PawnName, *FString(__FUNCTION__));
	//@EditEnd

	// Listen for when the pawn extension component changes init state
	BindOnActorInitStateChanged(ULyraPawnExtensionComponent::NAME_ActorFeatureName, FGameplayTag(), false);

	// Notifies that we are done spawning, then try the rest of initialization
	ensure(TryToChangeInitState(LyraGameplayTags::InitState_Spawned));
	CheckDefaultInitialization();
}

void ULyraHeroComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnregisterInitStateFeature();

	//@EditBegin-Ignore this
	const FString PawnName = GetPawn<APawn>() != nullptr ? GetPawn<APawn>()->GetName() : FString("None");
	UE_VLOG(this, LogLyra, VeryVerbose, TEXT("%s - %s(): EndPlay"), *PawnName, *FString(__FUNCTION__));
	//@EditEnd

	Super::EndPlay(EndPlayReason);
}

void ULyraHeroComponent::InitializePlayerInput(UInputComponent* PlayerInputComponent)
{
	check(PlayerInputComponent);

	const APawn* Pawn = GetPawn<APawn>();
	if (!Pawn)
	{
		return;
	}

	const APlayerController* PC = GetController<APlayerController>();
	check(PC);

	const ULyraLocalPlayer* LP = Cast<ULyraLocalPlayer>(PC->GetLocalPlayer());
	check(LP);

	UEnhancedInputLocalPlayerSubsystem* Subsystem = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	check(Subsystem);

	Subsystem->ClearAllMappings();

	//@EditBegin-Ignore this
	const FString PawnName = GetPawn<APawn>() != nullptr ? GetPawn<APawn>()->GetName() : FString("None");
	UE_VLOG(this, LogLyra, VeryVerbose, TEXT("%s - %s(): Got references and cleared mappings"), *PawnName, *FString(__FUNCTION__));
	//@EditEnd

	if (const ULyraPawnExtensionComponent* PawnExtComp = ULyraPawnExtensionComponent::FindPawnExtensionComponent(Pawn))
	{
		if (const ULyraPawnData* PawnData = PawnExtComp->GetPawnData<ULyraPawnData>())
		{
			if (const ULyraInputConfig* InputConfig = PawnData->InputConfig)
			{
				for (const FInputMappingContextAndPriority& Mapping : DefaultInputMappings)
				{
					if (UInputMappingContext* IMC = Mapping.InputMapping.LoadSynchronous())
					{
						if (Mapping.bRegisterWithSettings)
						{
							if (UEnhancedInputUserSettings* Settings = Subsystem->GetUserSettings())
							{
								UE_VLOG(this, LogLyra, VeryVerbose, TEXT("%s - %s(): Registering IMC %s"), *PawnName, *FString(__FUNCTION__), *IMC->GetName());
								Settings->RegisterInputMappingContext(IMC);
							}
							
							FModifyContextOptions Options = {};
							Options.bIgnoreAllPressedKeysUntilRelease = false;
							// Actually add the config to the local player							
							Subsystem->AddMappingContext(IMC, Mapping.Priority, Options);
							UE_VLOG(this, LogLyra, VeryVerbose, TEXT("%s - %s(): Adding IMC to mapping context"), *PawnName, *FString(__FUNCTION__));
						}
					}
				}

				// The Lyra Input Component has some additional functions to map Gameplay Tags to an Input Action.
				// If you want this functionality but still want to change your input component class, make it a subclass
				// of the ULyraInputComponent or modify this component accordingly.
				ULyraInputComponent* LyraIC = Cast<ULyraInputComponent>(PlayerInputComponent);
				if (ensureMsgf(LyraIC, TEXT("Unexpected Input Component class! The Gameplay Abilities will not be bound to their inputs. Change the input component to ULyraInputComponent or a subclass of it.")))
				{
					UE_VLOG(this, LogLyra, VeryVerbose, TEXT("%s - %s(): Adding input mappings to IC"), *PawnName, *FString(__FUNCTION__));
					// Add the key mappings that may have been set by the player
					LyraIC->AddInputMappings(InputConfig, Subsystem);

					// This is where we actually bind and input action to a gameplay tag, which means that Gameplay Ability Blueprints will
					// be triggered directly by these input actions Triggered events. 
					TArray<uint32> BindHandles;
					UE_VLOG(this, LogLyra, VeryVerbose, TEXT("%s - %s(): Binding ability actions for %s"), *PawnName, *FString(__FUNCTION__), *InputConfig->GetName());
					LyraIC->BindAbilityActions(InputConfig, this, &ThisClass::Input_AbilityInputTagPressed, &ThisClass::Input_AbilityInputTagReleased, /*out*/ BindHandles);

					UE_VLOG(this, LogLyra, VeryVerbose, TEXT("%s - %s(): Binding native actions for %s"), *PawnName, *FString(__FUNCTION__), *InputConfig->GetName());
					LyraIC->BindNativeAction(InputConfig, LyraGameplayTags::InputTag_Move, ETriggerEvent::Triggered, this, &ThisClass::Input_Move, /*bLogIfNotFound=*/ true);
					LyraIC->BindNativeAction(InputConfig, LyraGameplayTags::InputTag_Look_Mouse, ETriggerEvent::Triggered, this, &ThisClass::Input_LookMouse, /*bLogIfNotFound=*/ true);
					LyraIC->BindNativeAction(InputConfig, LyraGameplayTags::InputTag_Look_Stick, ETriggerEvent::Triggered, this, &ThisClass::Input_LookStick, /*bLogIfNotFound=*/ true);
					LyraIC->BindNativeAction(InputConfig, LyraGameplayTags::InputTag_Crouch, ETriggerEvent::Triggered, this, &ThisClass::Input_Crouch, /*bLogIfNotFound=*/ true);
					LyraIC->BindNativeAction(InputConfig, LyraGameplayTags::InputTag_AutoRun, ETriggerEvent::Triggered, this, &ThisClass::Input_AutoRun, /*bLogIfNotFound=*/ true);
				}
			}
		}
	}

	if (ensure(!bReadyToBindInputs))
	{
		UE_VLOG(this, LogLyra, VeryVerbose, TEXT("%s - %s(): Setting bReadyToBindInputs to True"), *PawnName, *FString(__FUNCTION__));
		bReadyToBindInputs = true;
	}

	UE_VLOG(this, LogLyra, VeryVerbose, TEXT("%s - %s(): Sending GameFrameworkComponentExtension events"), *PawnName, *FString(__FUNCTION__));
	UGameFrameworkComponentManager::SendGameFrameworkComponentExtensionEvent(const_cast<APlayerController*>(PC), NAME_BindInputsNow);
	UGameFrameworkComponentManager::SendGameFrameworkComponentExtensionEvent(const_cast<APawn*>(Pawn), NAME_BindInputsNow);
}

void ULyraHeroComponent::AddAdditionalInputConfig(const ULyraInputConfig* InputConfig)
{
	TArray<uint32> BindHandles;

	const APawn* Pawn = GetPawn<APawn>();
	if (!Pawn)
	{
		return;
	}
	
	const APlayerController* PC = GetController<APlayerController>();
	check(PC);

	const ULocalPlayer* LP = PC->GetLocalPlayer();
	check(LP);

	UEnhancedInputLocalPlayerSubsystem* Subsystem = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	check(Subsystem);

	if (const ULyraPawnExtensionComponent* PawnExtComp = ULyraPawnExtensionComponent::FindPawnExtensionComponent(Pawn))
	{
		ULyraInputComponent* LyraIC = Pawn->FindComponentByClass<ULyraInputComponent>();
		if (ensureMsgf(LyraIC, TEXT("Unexpected Input Component class! The Gameplay Abilities will not be bound to their inputs. Change the input component to ULyraInputComponent or a subclass of it.")))
		{
			//@EditBegin-Ignore this
			const FString PawnName = GetPawn<APawn>() != nullptr ? GetPawn<APawn>()->GetName() : FString("None");
			UE_VLOG(this, LogLyra, VeryVerbose, TEXT("%s - %s(): Binding additional ability actions for %s"), *PawnName, *FString(__FUNCTION__), *InputConfig->GetName());
			//@EditEnd
			LyraIC->BindAbilityActions(InputConfig, this, &ThisClass::Input_AbilityInputTagPressed, &ThisClass::Input_AbilityInputTagReleased, /*out*/ BindHandles);
		}
	}
}

void ULyraHeroComponent::RemoveAdditionalInputConfig(const ULyraInputConfig* InputConfig)
{
	//@TODO: Implement me!
	//@EditBegin-Ignore this
	const FString PawnName = GetPawn<APawn>() != nullptr ? GetPawn<APawn>()->GetName() : FString("None");
	UE_VLOG(this, LogLyra, VeryVerbose, TEXT("%s - %s(): Removing additional config %s in unimplemented function"), *PawnName, *FString(__FUNCTION__), *InputConfig->GetName());
	//@EditEnd
}

bool ULyraHeroComponent::IsReadyToBindInputs() const
{
	return bReadyToBindInputs;
}

void ULyraHeroComponent::Input_AbilityInputTagPressed(FGameplayTag InputTag)
{
	if (const APawn* Pawn = GetPawn<APawn>())
	{
		if (const ULyraPawnExtensionComponent* PawnExtComp = ULyraPawnExtensionComponent::FindPawnExtensionComponent(Pawn))
		{
			if (ULyraAbilitySystemComponent* LyraASC = PawnExtComp->GetLyraAbilitySystemComponent())
			{
				//@EditBegin-Ignore this
				const FString PawnName = GetPawn<APawn>() != nullptr ? GetPawn<APawn>()->GetName() : FString("None");
				UE_VLOG(this, LogLyra, VeryVerbose, TEXT("%s - %s(): Ability input pressed: %s"), *PawnName, *FString(__FUNCTION__), *InputTag.ToString());
				//@EditEnd
				LyraASC->AbilityInputTagPressed(InputTag);
			}
		}	
	}
}

void ULyraHeroComponent::Input_AbilityInputTagReleased(FGameplayTag InputTag)
{
	const APawn* Pawn = GetPawn<APawn>();
	if (!Pawn)
	{
		return;
	}

	if (const ULyraPawnExtensionComponent* PawnExtComp = ULyraPawnExtensionComponent::FindPawnExtensionComponent(Pawn))
	{
		if (ULyraAbilitySystemComponent* LyraASC = PawnExtComp->GetLyraAbilitySystemComponent())
		{
			//@EditBegin-Ignore this
			const FString PawnName = GetPawn<APawn>() != nullptr ? GetPawn<APawn>()->GetName() : FString("None");
			UE_VLOG(this, LogLyra, VeryVerbose, TEXT("%s - %s(): Ability input released: %s"), *PawnName, *FString(__FUNCTION__), *InputTag.ToString());
			//@EditEnd
			LyraASC->AbilityInputTagReleased(InputTag);
		}
	}
}

void ULyraHeroComponent::Input_Move(const FInputActionValue& InputActionValue)
{
	APawn* Pawn = GetPawn<APawn>();
	AController* Controller = Pawn ? Pawn->GetController() : nullptr;

	//@EditBegin-Ignore this
	const FString PawnName = GetPawn<APawn>() != nullptr ? GetPawn<APawn>()->GetName() : FString("None");
	//@EditEnd

	// If the player has attempted to move again then cancel auto running
	if (ALyraPlayerController* LyraController = Cast<ALyraPlayerController>(Controller))
	{
		UE_VLOG(this, LogLyra, VeryVerbose, TEXT("%s - %s(): Disabling auto-run"), *PawnName, *FString(__FUNCTION__));
		LyraController->SetIsAutoRunning(false);
	}
	
	if (Controller)
	{
		const FVector2D Value = InputActionValue.Get<FVector2D>();
		const FRotator MovementRotation(0.0f, Controller->GetControlRotation().Yaw, 0.0f);

		if (Value.X != 0.0f)
		{
			UE_VLOG(this, LogLyra, VeryVerbose, TEXT("%s - %s(): Adding X input"), *PawnName, *FString(__FUNCTION__));
			const FVector MovementDirection = MovementRotation.RotateVector(FVector::RightVector);
			Pawn->AddMovementInput(MovementDirection, Value.X);
		}

		if (Value.Y != 0.0f)
		{
			UE_VLOG(this, LogLyra, VeryVerbose, TEXT("%s - %s(): Adding Y input"), *PawnName, *FString(__FUNCTION__));
			const FVector MovementDirection = MovementRotation.RotateVector(FVector::ForwardVector);
			Pawn->AddMovementInput(MovementDirection, Value.Y);
		}
	}
}

void ULyraHeroComponent::Input_LookMouse(const FInputActionValue& InputActionValue)
{
	APawn* Pawn = GetPawn<APawn>();

	if (!Pawn)
	{
		return;
	}

	//@EditBegin-Ignore this
	const FString PawnName = GetPawn<APawn>() != nullptr ? GetPawn<APawn>()->GetName() : FString("None");
	//@EditEnd
	
	const FVector2D Value = InputActionValue.Get<FVector2D>();

	if (Value.X != 0.0f)
	{
		UE_VLOG(this, LogLyra, VeryVerbose, TEXT("%s - %s(): Adding Yaw input"), *PawnName, *FString(__FUNCTION__));
		Pawn->AddControllerYawInput(Value.X);
	}

	if (Value.Y != 0.0f)
	{
		UE_VLOG(this, LogLyra, VeryVerbose, TEXT("%s - %s(): Adding Pitch input"), *PawnName, *FString(__FUNCTION__));
		Pawn->AddControllerPitchInput(Value.Y);
	}
}

void ULyraHeroComponent::Input_LookStick(const FInputActionValue& InputActionValue)
{
	APawn* Pawn = GetPawn<APawn>();

	if (!Pawn)
	{
		return;
	}
	
	const FVector2D Value = InputActionValue.Get<FVector2D>();

	const UWorld* World = GetWorld();
	check(World);

	if (Value.X != 0.0f)
	{
		Pawn->AddControllerYawInput(Value.X * LyraHero::LookYawRate * World->GetDeltaSeconds());
	}

	if (Value.Y != 0.0f)
	{
		Pawn->AddControllerPitchInput(Value.Y * LyraHero::LookPitchRate * World->GetDeltaSeconds());
	}
}

void ULyraHeroComponent::Input_Crouch(const FInputActionValue& InputActionValue)
{
	if (ALyraCharacter* Character = GetPawn<ALyraCharacter>())
	{
		//@EditBegin-Ignore this
		const FString PawnName = GetPawn<APawn>() != nullptr ? GetPawn<APawn>()->GetName() : FString("None");
		UE_VLOG(this, LogLyra, VeryVerbose, TEXT("%s - %s(): Toggle Crouch"), *PawnName, *FString(__FUNCTION__));
		//@EditEnd
		Character->ToggleCrouch();
	}
}

void ULyraHeroComponent::Input_AutoRun(const FInputActionValue& InputActionValue)
{
	if (APawn* Pawn = GetPawn<APawn>())
	{
		if (ALyraPlayerController* Controller = Cast<ALyraPlayerController>(Pawn->GetController()))
		{
			//@EditBegin-Ignore this
			const FString PawnName = GetPawn<APawn>() != nullptr ? GetPawn<APawn>()->GetName() : FString("None");
			UE_VLOG(this, LogLyra, VeryVerbose, TEXT("%s - %s(): Toggle auto running"), *PawnName, *FString(__FUNCTION__));
			//@EditEnd
			// Toggle auto running
			Controller->SetIsAutoRunning(!Controller->GetIsAutoRunning());
		}	
	}
}

TSubclassOf<ULyraCameraMode> ULyraHeroComponent::DetermineCameraMode() const
{
	if (AbilityCameraMode)
	{
		return AbilityCameraMode;
	}

	const APawn* Pawn = GetPawn<APawn>();
	if (!Pawn)
	{
		return nullptr;
	}

	if (ULyraPawnExtensionComponent* PawnExtComp = ULyraPawnExtensionComponent::FindPawnExtensionComponent(Pawn))
	{
		if (const ULyraPawnData* PawnData = PawnExtComp->GetPawnData<ULyraPawnData>())
		{
			return PawnData->DefaultCameraMode;
		}
	}

	return nullptr;
}

void ULyraHeroComponent::SetAbilityCameraMode(TSubclassOf<ULyraCameraMode> CameraMode, const FGameplayAbilitySpecHandle& OwningSpecHandle)
{
	if (CameraMode)
	{
		AbilityCameraMode = CameraMode;
		AbilityCameraModeOwningSpecHandle = OwningSpecHandle;
	}
}

void ULyraHeroComponent::ClearAbilityCameraMode(const FGameplayAbilitySpecHandle& OwningSpecHandle)
{
	if (AbilityCameraModeOwningSpecHandle == OwningSpecHandle)
	{
		AbilityCameraMode = nullptr;
		AbilityCameraModeOwningSpecHandle = FGameplayAbilitySpecHandle();
	}
}

