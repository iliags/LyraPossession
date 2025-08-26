// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Player/LyraPlayerController.h"
#include "PossessionPlayerController.generated.h"

/**
 * 
 */
UCLASS()
class POSSESSIONFEATURERUNTIME_API APossessionPlayerController : public ALyraPlayerController
{
	GENERATED_BODY()

public:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
};
