// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraPawnExtensionComponent.h"

#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "Components/GameFrameworkComponentDelegates.h"
#include "Components/GameFrameworkComponentManager.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "LyraGameplayTags.h"
#include "LyraLogChannels.h"
#include "LyraPawnData.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraPawnExtensionComponent)

class FLifetimeProperty;
class UActorComponent;

const FName ULyraPawnExtensionComponent::NAME_ActorFeatureName("PawnExtension");

ULyraPawnExtensionComponent::ULyraPawnExtensionComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.bCanEverTick = false;

	SetIsReplicatedByDefault(true);

	PawnData = nullptr;
	AbilitySystemComponent = nullptr;
}

void ULyraPawnExtensionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ULyraPawnExtensionComponent, PawnData);
}

void ULyraPawnExtensionComponent::OnRegister()
{
	Super::OnRegister();

	//@EditBegin-Ignore this
	const FString PawnName = GetPawn<APawn>() != nullptr ? GetPawn<APawn>()->GetName() : FString("None");
	UE_VLOG(this, LogLyra, VeryVerbose, TEXT("%s - %s(): OnRegister"), *PawnName, *FString(__FUNCTION__));
	//@EditEnd

	const APawn* Pawn = GetPawn<APawn>();
	ensureAlwaysMsgf((Pawn != nullptr), TEXT("LyraPawnExtensionComponent on [%s] can only be added to Pawn actors."), *GetNameSafe(GetOwner()));

	TArray<UActorComponent*> PawnExtensionComponents;
	Pawn->GetComponents(ULyraPawnExtensionComponent::StaticClass(), PawnExtensionComponents);
	ensureAlwaysMsgf((PawnExtensionComponents.Num() == 1), TEXT("Only one LyraPawnExtensionComponent should exist on [%s]."), *GetNameSafe(GetOwner()));

	// Register with the init state system early, this will only work if this is a game world
	RegisterInitStateFeature();
}

void ULyraPawnExtensionComponent::BeginPlay()
{
	Super::BeginPlay();

	//@EditBegin-Ignore this
	const FString PawnName = GetPawn<APawn>() != nullptr ? GetPawn<APawn>()->GetName() : FString("None");
	UE_VLOG(this, LogLyra, VeryVerbose, TEXT("%s - %s(): Begin Play"), *PawnName, *FString(__FUNCTION__));
	//@EditEnd

	// Listen for changes to all features
	BindOnActorInitStateChanged(NAME_None, FGameplayTag(), false);
	
	// Notifies state manager that we have spawned, then try rest of default initialization
	ensure(TryToChangeInitState(LyraGameplayTags::InitState_Spawned));
	CheckDefaultInitialization();
}

void ULyraPawnExtensionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	//@EditBegin-Ignore this
	const FString PawnName = GetPawn<APawn>() != nullptr ? GetPawn<APawn>()->GetName() : FString("None");
	UE_VLOG(this, LogLyra, VeryVerbose, TEXT("%s - %s(): End Play"), *PawnName, *FString(__FUNCTION__));
	//@EditEnd
	
	UninitializeAbilitySystem();
	UnregisterInitStateFeature();

	

	Super::EndPlay(EndPlayReason);
}

void ULyraPawnExtensionComponent::SetPawnData(const ULyraPawnData* InPawnData)
{
	check(InPawnData);

	APawn* Pawn = GetPawnChecked<APawn>();

	if (Pawn->GetLocalRole() != ROLE_Authority)
	{
		return;
	}

	if (PawnData)
	{
		UE_LOG(LogLyra, Error, TEXT("Trying to set PawnData [%s] on pawn [%s] that already has valid PawnData [%s]."), *GetNameSafe(InPawnData), *GetNameSafe(Pawn), *GetNameSafe(PawnData));
		return;
	}

	//@EditBegin-Ignore this
	const FString PawnName = GetPawn<APawn>() != nullptr ? GetPawn<APawn>()->GetName() : FString("None");
	UE_VLOG(this, LogLyra, VeryVerbose, TEXT("%s - %s(): Setting pawn data"), *PawnName, *FString(__FUNCTION__));
	//@EditEnd

	PawnData = InPawnData;

	Pawn->ForceNetUpdate();

	CheckDefaultInitialization();
}

void ULyraPawnExtensionComponent::OnRep_PawnData()
{
	CheckDefaultInitialization();
}

void ULyraPawnExtensionComponent::InitializeAbilitySystem(ULyraAbilitySystemComponent* InASC, AActor* InOwnerActor)
{
	check(InASC);
	check(InOwnerActor);

	//@EditBegin-Ignore this
	const FString PawnName = GetPawn<APawn>() != nullptr ? GetPawn<APawn>()->GetName() : FString("None");
	//@EditEnd

	if (AbilitySystemComponent == InASC)
	{
		UE_VLOG(this, LogLyra, VeryVerbose, TEXT("%s - %s(): ASC hasn't changed, returning"), *PawnName, *FString(__FUNCTION__));
		// The ability system component hasn't changed.
		return;
	}

	if (AbilitySystemComponent)
	{
		UE_VLOG(this, LogLyra, VeryVerbose, TEXT("%s - %s(): Uninitializing ASC"), *PawnName, *FString(__FUNCTION__));
		// Clean up the old ability system component.
		UninitializeAbilitySystem();
	}

	APawn* Pawn = GetPawnChecked<APawn>();
	AActor* ExistingAvatar = InASC->GetAvatarActor();

	UE_LOG(LogLyra, Verbose, TEXT("Setting up ASC [%s] on pawn [%s] owner [%s], existing [%s] "), *GetNameSafe(InASC), *GetNameSafe(Pawn), *GetNameSafe(InOwnerActor), *GetNameSafe(ExistingAvatar));

	if ((ExistingAvatar != nullptr) && (ExistingAvatar != Pawn))
	{
		UE_LOG(LogLyra, Log, TEXT("Existing avatar (authority=%d)"), ExistingAvatar->HasAuthority() ? 1 : 0);

		// There is already a pawn acting as the ASC's avatar, so we need to kick it out
		// This can happen on clients if they're lagged: their new pawn is spawned + possessed before the dead one is removed
		ensure(!ExistingAvatar->HasAuthority());

		if (ULyraPawnExtensionComponent* OtherExtensionComponent = FindPawnExtensionComponent(ExistingAvatar))
		{
			UE_VLOG(this, LogLyra, VeryVerbose, TEXT("%s - %s(): Uninitializing the other extension components ASC"), *PawnName, *FString(__FUNCTION__));
			OtherExtensionComponent->UninitializeAbilitySystem();
		}
	}

	UE_VLOG(this, LogLyra, VeryVerbose, TEXT("%s - %s(): Setting the new ASC and calling InitAbilityActorInfo"), *PawnName, *FString(__FUNCTION__));
	AbilitySystemComponent = InASC;
	AbilitySystemComponent->InitAbilityActorInfo(InOwnerActor, Pawn);

	if (ensure(PawnData))
	{
		UE_VLOG(this, LogLyra, VeryVerbose, TEXT("%s - %s(): Setting tag relationship mappings"), *PawnName, *FString(__FUNCTION__));
		InASC->SetTagRelationshipMapping(PawnData->TagRelationshipMapping);
	}

	UE_VLOG(this, LogLyra, VeryVerbose, TEXT("%s - %s(): Broadcasting ASC initialized event"), *PawnName, *FString(__FUNCTION__));
	OnAbilitySystemInitialized.Broadcast();
}

void ULyraPawnExtensionComponent::UninitializeAbilitySystem()
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	//@EditBegin-Ignore this
	const FString PawnName = GetPawn<APawn>() != nullptr ? GetPawn<APawn>()->GetName() : FString("None");
	UE_VLOG(this, LogLyra, VeryVerbose, TEXT("%s - %s(): Entry"), *PawnName, *FString(__FUNCTION__));
	//@EditEnd

	// Uninitialize the ASC if we're still the avatar actor (otherwise another pawn already did it when they became the avatar actor)
	if (AbilitySystemComponent->GetAvatarActor() == GetOwner())
	{
		UE_VLOG(this, LogLyra, VeryVerbose, TEXT("%s - %s(): Canceling and clearing abilities"), *PawnName, *FString(__FUNCTION__));
		FGameplayTagContainer AbilityTypesToIgnore;
		AbilityTypesToIgnore.AddTag(LyraGameplayTags::Ability_Behavior_SurvivesDeath);

		AbilitySystemComponent->CancelAbilities(nullptr, &AbilityTypesToIgnore);
		AbilitySystemComponent->ClearAbilityInput();
		AbilitySystemComponent->RemoveAllGameplayCues();

		if (AbilitySystemComponent->GetOwnerActor() != nullptr)
		{
			UE_VLOG(this, LogLyra, VeryVerbose, TEXT("%s - %s(): Clearing out avatar actor"), *PawnName, *FString(__FUNCTION__));
			AbilitySystemComponent->SetAvatarActor(nullptr);
		}
		else
		{
			UE_VLOG(this, LogLyra, VeryVerbose, TEXT("%s - %s(): Clearing out ActorInfo"), *PawnName, *FString(__FUNCTION__));
			// If the ASC doesn't have a valid owner, we need to clear *all* actor info, not just the avatar pairing
			AbilitySystemComponent->ClearActorInfo();
		}

		UE_VLOG(this, LogLyra, VeryVerbose, TEXT("%s - %s(): Broadcasting OnAbilitySystemUninitialized"), *PawnName, *FString(__FUNCTION__));

		OnAbilitySystemUninitialized.Broadcast();
	}

	AbilitySystemComponent = nullptr;
}

void ULyraPawnExtensionComponent::HandleControllerChanged()
{
	//@EditBegin-Ignore this
	const FString PawnName = GetPawn<APawn>() != nullptr ? GetPawn<APawn>()->GetName() : FString("None");
	UE_VLOG(this, LogLyra, VeryVerbose, TEXT("%s - %s(): Entry"), *PawnName, *FString(__FUNCTION__));
	//@EditEnd
	if (AbilitySystemComponent && (AbilitySystemComponent->GetAvatarActor() == GetPawnChecked<APawn>()))
	{
		ensure(AbilitySystemComponent->AbilityActorInfo->OwnerActor == AbilitySystemComponent->GetOwnerActor());
		if (AbilitySystemComponent->GetOwnerActor() == nullptr)
		{
			UE_VLOG(this, LogLyra, VeryVerbose, TEXT("%s - %s(): Uninitializing ASC"), *PawnName, *FString(__FUNCTION__));
			UninitializeAbilitySystem();
		}
		else
		{
			UE_VLOG(this, LogLyra, VeryVerbose, TEXT("%s - %s(): Refresh Actor Info"), *PawnName, *FString(__FUNCTION__));
			AbilitySystemComponent->RefreshAbilityActorInfo();
		}
	}

	UE_VLOG(this, LogLyra, VeryVerbose, TEXT("%s - %s(): Check default initialization"), *PawnName, *FString(__FUNCTION__));
	CheckDefaultInitialization();
}

void ULyraPawnExtensionComponent::HandlePlayerStateReplicated()
{
	CheckDefaultInitialization();
}

void ULyraPawnExtensionComponent::SetupPlayerInputComponent()
{
	CheckDefaultInitialization();
}

void ULyraPawnExtensionComponent::CheckDefaultInitialization()
{
	//@EditBegin-Ignore this
	const FString PawnName = GetPawn<APawn>() != nullptr ? GetPawn<APawn>()->GetName() : FString("None");
	UE_VLOG(this, LogLyra, VeryVerbose, TEXT("%s - %s(): Entry"), *PawnName, *FString(__FUNCTION__));
	//@EditEnd
	
	// Before checking our progress, try progressing any other features we might depend on
	CheckDefaultInitializationForImplementers();

	static const TArray<FGameplayTag> StateChain = { LyraGameplayTags::InitState_Spawned, LyraGameplayTags::InitState_DataAvailable, LyraGameplayTags::InitState_DataInitialized, LyraGameplayTags::InitState_GameplayReady };

	// This will try to progress from spawned (which is only set in BeginPlay) through the data initialization stages until it gets to gameplay ready
	ContinueInitStateChain(StateChain);
}

bool ULyraPawnExtensionComponent::CanChangeInitState(UGameFrameworkComponentManager* Manager, FGameplayTag CurrentState, FGameplayTag DesiredState) const
{
	check(Manager);

	//@EditBegin-Ignore this
	const FString PawnName = GetPawn<APawn>() != nullptr ? GetPawn<APawn>()->GetName() : FString("None");
	UE_VLOG(this, LogLyra, VeryVerbose, TEXT("%s - %s(): Entry"), *PawnName, *FString(__FUNCTION__));
	//@EditEnd
	
	APawn* Pawn = GetPawn<APawn>();
	if (!CurrentState.IsValid() && DesiredState == LyraGameplayTags::InitState_Spawned)
	{
		// As long as we are on a valid pawn, we count as spawned
		if (Pawn)
		{
			UE_VLOG(this, LogLyra, VeryVerbose, TEXT("%s - %s(): Valid pawn, returning true"), *PawnName, *FString(__FUNCTION__));
			return true;
		}
	}
	if (CurrentState == LyraGameplayTags::InitState_Spawned && DesiredState == LyraGameplayTags::InitState_DataAvailable)
	{
		// Pawn data is required.
		if (!PawnData)
		{
			UE_VLOG(this, LogLyra, VeryVerbose, TEXT("%s - %s(): No pawn data, returning false"), *PawnName, *FString(__FUNCTION__));
			return false;
		}

		const bool bHasAuthority = Pawn->HasAuthority();
		const bool bIsLocallyControlled = Pawn->IsLocallyControlled();

		if (bHasAuthority || bIsLocallyControlled)
		{
			// Check for being possessed by a controller.
			if (!GetController<AController>())
			{
				UE_VLOG(this, LogLyra, VeryVerbose, TEXT("%s - %s(): No controller, returning false"), *PawnName, *FString(__FUNCTION__));
				return false;
			}
		}

		return true;
	}
	else if (CurrentState == LyraGameplayTags::InitState_DataAvailable && DesiredState == LyraGameplayTags::InitState_DataInitialized)
	{
		UE_VLOG(this, LogLyra, VeryVerbose, TEXT("%s - %s(): Transition to initialize if all features have their data available, returning"), *PawnName, *FString(__FUNCTION__));
		// Transition to initialize if all features have their data available
		return Manager->HaveAllFeaturesReachedInitState(Pawn, LyraGameplayTags::InitState_DataAvailable);
	}
	else if (CurrentState == LyraGameplayTags::InitState_DataInitialized && DesiredState == LyraGameplayTags::InitState_GameplayReady)
	{
		UE_VLOG(this, LogLyra, VeryVerbose, TEXT("%s - %s(): Initialized and desired state is gameplay ready, returning true"), *PawnName, *FString(__FUNCTION__));
		return true;
	}

	UE_VLOG(this, LogLyra, VeryVerbose, TEXT("%s - %s(): Fell through to default, returning false"), *PawnName, *FString(__FUNCTION__));
	return false;
}

void ULyraPawnExtensionComponent::HandleChangeInitState(UGameFrameworkComponentManager* Manager, FGameplayTag CurrentState, FGameplayTag DesiredState)
{
	if (DesiredState == LyraGameplayTags::InitState_DataInitialized)
	{
		// This is currently all handled by other components listening to this state change
	}
}

void ULyraPawnExtensionComponent::OnActorInitStateChanged(const FActorInitStateChangedParams& Params)
{
	//@EditBegin-Ignore this
	const FString PawnName = GetPawn<APawn>() != nullptr ? GetPawn<APawn>()->GetName() : FString("None");
	UE_VLOG(this, LogLyra, VeryVerbose, TEXT("%s - %s(): Entry"), *PawnName, *FString(__FUNCTION__));
	//@EditEnd
	// If another feature is now in DataAvailable, see if we should transition to DataInitialized
	if (Params.FeatureName != NAME_ActorFeatureName)
	{
		if (Params.FeatureState == LyraGameplayTags::InitState_DataAvailable)
		{
			UE_VLOG(this, LogLyra, VeryVerbose, TEXT("%s - %s(): Checking default initialization"), *PawnName, *FString(__FUNCTION__));
			CheckDefaultInitialization();
		}
	}
}

void ULyraPawnExtensionComponent::OnAbilitySystemInitialized_RegisterAndCall(FSimpleMulticastDelegate::FDelegate Delegate)
{
	//@EditBegin-Ignore this
	const FString PawnName = GetPawn<APawn>() != nullptr ? GetPawn<APawn>()->GetName() : FString("None");
	UE_VLOG(this, LogLyra, VeryVerbose, TEXT("%s - %s(): Entry"), *PawnName, *FString(__FUNCTION__));
	//@EditEnd
	if (!OnAbilitySystemInitialized.IsBoundToObject(Delegate.GetUObject()))
	{
		UE_VLOG(this, LogLyra, VeryVerbose, TEXT("%s - %s(): Adding delegate"), *PawnName, *FString(__FUNCTION__));
		OnAbilitySystemInitialized.Add(Delegate);
	}

	if (AbilitySystemComponent)
	{
		UE_VLOG(this, LogLyra, VeryVerbose, TEXT("%s - %s(): Executing delegate"), *PawnName, *FString(__FUNCTION__));
		Delegate.Execute();
	}
}

void ULyraPawnExtensionComponent::OnAbilitySystemUninitialized_Register(FSimpleMulticastDelegate::FDelegate Delegate)
{
	//@EditBegin-Ignore this
	const FString PawnName = GetPawn<APawn>() != nullptr ? GetPawn<APawn>()->GetName() : FString("None");
	UE_VLOG(this, LogLyra, VeryVerbose, TEXT("%s - %s(): Entry"), *PawnName, *FString(__FUNCTION__));
	//@EditEnd
	if (!OnAbilitySystemUninitialized.IsBoundToObject(Delegate.GetUObject()))
	{
		UE_VLOG(this, LogLyra, VeryVerbose, TEXT("%s - %s(): Adding delegate"), *PawnName, *FString(__FUNCTION__));
		OnAbilitySystemUninitialized.Add(Delegate);
	}
}

