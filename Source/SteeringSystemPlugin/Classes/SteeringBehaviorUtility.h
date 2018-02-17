// 

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SteeringTypes.h"
#include "SteeringBehaviorUtility.generated.h"

struct FSteeringBehaviorRef;
class ISteerable;

UCLASS()
class STEERINGSYSTEMPLUGIN_API USteeringBehaviorUtility : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

    UFUNCTION(BlueprintCallable)
    static FSteeringTarget MakeTargetLocation(const FVector& InLocation);

    UFUNCTION(BlueprintCallable)
    static FSteeringTarget MakeTargetDirection(const FVector& InDirection);

    UFUNCTION(BlueprintCallable)
    static FSteeringTarget MakeTargetRotation(const FRotator& InRotation);

    UFUNCTION(BlueprintCallable)
    static FSteeringBehaviorRef CreateArriveBehavior(const FSteeringTarget& InSteeringTarget, float InOuterRadius = 150.f, float InInnerRadius = 50.f);

    UFUNCTION(BlueprintCallable)
    static FSteeringBehaviorRef CreateAlignBehavior(const FSteeringTarget& InSteeringTarget);

    UFUNCTION(BlueprintCallable)
    static FSteeringBehaviorRef CreateRepulsionBehavior(const FVector& InRepulsionLocation, float InRepulsionDistance = 150.f);

    UFUNCTION(BlueprintCallable)
    static FSteeringBehaviorRef CreateLineRepulsionBehavior(const FVector& InLineOrigin, const FVector& InLineDirection, float InRepulsionDistance = 150.f, const FVector& InParallelRepulsionDirection = FVector::UpVector);

    UFUNCTION(BlueprintCallable)
    static FSteeringFormationData CreateFormation(const TArray<FSteerableRef>& Agents, uint8 FormationType = 0);

    UFUNCTION(BlueprintCallable)
    static void ProjectFormationAt(FSteeringFormationData& Formation, const FVector& TargetLocation);
};
