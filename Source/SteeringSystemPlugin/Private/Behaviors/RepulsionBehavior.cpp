// 

#include "Behaviors/RepulsionBehavior.h"

#include "UnrealMathUtility.h"
#include "Kismet/KismetMathLibrary.h"

bool FRepulsionBehavior::CalculateSteeringImpl(float DeltaTime, FSteeringAcceleration& OutControlInput)
{
    check(HasValidData());

    const FVector SrcLocation(Steerable->GetSteerableLocation());
    const FVector SrcNormal(Steerable->GetForwardVector());
    const FVector DeltaLocation(SrcLocation-RepulsionLocation);

    // Check for arrival
    if (DeltaLocation.SizeSquared() > (RepulsionDistance*RepulsionDistance))
    {
        return true;
    }

    OutControlInput = FSteeringAcceleration(DeltaLocation.GetSafeNormal(), true, true);

    return false;
}

bool FLineRepulsionBehavior::CalculateSteeringImpl(float DeltaTime, FSteeringAcceleration& OutControlInput)
{
    check(HasValidData());

    const FVector SrcLocation(Steerable->GetSteerableLocation());
    const FVector SrcNormal(Steerable->GetForwardVector());

    const FVector DstLocation(UKismetMathLibrary::FindClosestPointOnLine(SrcLocation, LineOrigin, LineDirection));
    const FVector DeltaLocation(SrcLocation-DstLocation);
    const float DeltaDistSq(DeltaLocation.SizeSquared());

    // Check for arrival
    if (DeltaDistSq > (RepulsionDistance*RepulsionDistance))
    {
        return true;
    }

    FVector RepulsionDirection = (DeltaDistSq > SMALL_NUMBER) ? DeltaLocation.GetSafeNormal() : ParallelRepulsionDirection;
    OutControlInput = FSteeringAcceleration(RepulsionDirection, true, true);

    return false;
}
