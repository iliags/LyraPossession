// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Character/LyraCharacterWithAbilities.h"

#include "PossessionCharacterWithAbilities.generated.h"

class ULyraHeroComponent;
class ULyraPawnData;

UCLASS()
class POSSESSIONFEATURERUNTIME_API APossessionCharacterWithAbilities : public ALyraCharacterWithAbilities
{
	GENERATED_BODY()

public:
	explicit APossessionCharacterWithAbilities(const FObjectInitializer& ObjectInitializer);
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TObjectPtr<const ULyraPawnData> PawnData;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ULyraHeroComponent> HeroComponent;
};
