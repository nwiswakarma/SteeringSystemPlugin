// 

#include "SteeringBehaviorUtility.h"
#include "Behaviors/AlignBehavior.h"
#include "Behaviors/ArriveBehavior.h"
#include "Behaviors/RepulsionBehavior.h"

FSteeringTarget USteeringBehaviorUtility::MakeTargetLocation(const FVector& InLocation)
{
    return FSteeringTarget(InLocation);
}

FSteeringTarget USteeringBehaviorUtility::MakeTargetRotation(const FRotator& InRotation)
{
    return FSteeringTarget(InRotation);
}

FSteeringTarget USteeringBehaviorUtility::MakeTargetDirection(const FVector& InDirection)
{
    return FSteeringTarget(InDirection.ToOrientationQuat());
}

FSteeringBehaviorRef USteeringBehaviorUtility::CreateArriveBehavior(const FSteeringTarget& InSteeringTarget, float InOuterRadius, float InInnerRadius)
{
    FPSSteeringBehavior Behavior( new FArriveBehavior(InSteeringTarget, InOuterRadius, InInnerRadius) );
    return FSteeringBehaviorRef(Behavior);
}

FSteeringBehaviorRef USteeringBehaviorUtility::CreateAlignBehavior(const FSteeringTarget& InSteeringTarget)
{
    FPSSteeringBehavior Behavior( new FAlignBehavior(InSteeringTarget) );
    return FSteeringBehaviorRef(Behavior);
}

FSteeringBehaviorRef USteeringBehaviorUtility::CreateRepulsionBehavior(const FVector& InRepulsionLocation, float InRepulsionDistance)
{
    FPSSteeringBehavior Behavior( new FRepulsionBehavior(InRepulsionLocation, InRepulsionDistance) );
    return FSteeringBehaviorRef(Behavior);
}

FSteeringBehaviorRef USteeringBehaviorUtility::CreateLineRepulsionBehavior(const FVector& InLineOrigin, const FVector& InLineDirection, float InRepulsionDistance, const FVector& InParallelRepulsionDirection)
{
    FPSSteeringBehavior Behavior( new FLineRepulsionBehavior(InLineOrigin, InLineDirection, InRepulsionDistance, InParallelRepulsionDirection) );
    return FSteeringBehaviorRef(Behavior);
}

FSteeringFormationData USteeringBehaviorUtility::CreateFormation(const TArray<FSteerableRef>& Agents, uint8 FormationType)
{
    return FSteeringFormationData(Agents);
}

void USteeringBehaviorUtility::ProjectFormationAt(FSteeringFormationData& Formation, const FVector& TargetLocation)
{
    Formation.ProjectLocation(TargetLocation);
}
