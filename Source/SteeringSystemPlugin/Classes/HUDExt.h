// 

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/HUD.h"
#include "UObject/ObjectMacros.h"
#include "HUDExt.generated.h"

UCLASS(notplaceable, transient, BlueprintType, Blueprintable)
class STEERINGSYSTEMPLUGIN_API AHUDExt : public AHUD 
{
    GENERATED_BODY()

    /**
     * Finds the array of actors inside a selection rectangle from a list of its owned components.
     */
    UFUNCTION(Category=HUD)
    void FindFilteredActorsInSelectionRectangle(const TArray<USceneComponent*>& Components, const FVector2D& FirstPoint, const FVector2D& SecondPoint, TArray<AActor*>& OutActors, bool bActorMustBeFullyEnclosed = false);

    /**
     * Finds the array of actors inside a selection rectangle from a list of its owned components.
     */
    UFUNCTION(Category=HUD)
    TArray<AActor*> GetFilteredActorsInSelectionRectangle(const TArray<USceneComponent*>& Components, const FVector2D& FirstPoint, const FVector2D& SecondPoint, bool bActorMustBeFullyEnclosed = false);
};
