// 

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SteeringTypes.h"
#include "SteeringFormation.h"
#include "SteeringFormationUtility.generated.h"

UCLASS()
class STEERINGSYSTEMPLUGIN_API USteeringFormationUtility : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

    UFUNCTION(BlueprintCallable)
    static FFormationBehaviorRef CreateRegroupBehavior();

    UFUNCTION(BlueprintCallable)
    static FFormationBehaviorRef CreateMoveBehavior(const FSteeringTarget& SteeringTarget);
};
