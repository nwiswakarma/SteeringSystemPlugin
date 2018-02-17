// 

#include "Behaviors/ArriveBehavior.h"
#include "UnrealMathUtility.h"

bool FArriveBehavior::CalculateSteeringImpl(float DeltaTime, FSteeringAcceleration& OutControlInput)
{
    check(HasValidData());

    const FVector SrcLocation(Steerable->GetSteerableLocation());
    const FVector SrcNormal(Steerable->GetForwardVector());

    const FVector& DstLocation(SteeringTarget.GetLocation());
    const FVector DeltaLocation(DstLocation-SrcLocation);
    const FVector DstNormal(DeltaLocation.GetSafeNormal());

    const float DistSq = DeltaLocation.SizeSquared();
    const float DstDot = SrcNormal | DstNormal;
    const float CurSpeedSq = Steerable->GetLinearVelocity().SizeSquared();

    // Check for arrival
    if (DistSq < (InnerRadius*InnerRadius))
    {
        return true;
    }

    // Whether movement should be enabled or orient towards target
    const bool bMoveEnabled = bMoveTowardsTarget ? (CurSpeedSq > 0.f && DstDot > 0.f) : (DstDot > .85f);

    // Output acceleration
    FSteeringAcceleration ControlInput;

    if (bMoveEnabled)
    {
        const float MaxSpeed = Steerable->GetMaxLinearSpeed();
        const float MinInput = Steerable->GetMinControlInput();

        const float Dist = FMath::Sqrt(DistSq);
        const float MaxSpeedInv = 1.f / MaxSpeed;
        const float ClampedDist = FMath::Min(Dist, MaxSpeed);
        const float ClampedDistRatio = ClampedDist * MaxSpeedInv;

        float ControlMagnitude = 1.f;

        // Adjust control input magnitude
        if (! FVector::Coincident(SrcNormal, DstNormal))
        {
            const FVector SrcLocationAtN(SrcLocation + SrcNormal*ClampedDist);
            const FVector DstNormalAtN((DstLocation-SrcLocationAtN).GetSafeNormal());
            const float DstDotAtN = SrcNormal | DstNormalAtN;

            // Scale bias offset, control input using dot product [-1, 1] to [0, 1]
            ControlMagnitude = (DstDotAtN+1.f) * .5f * ClampedDistRatio;

            // Clamp minimum turn control magnitude
            ControlMagnitude = FMath::Max(ControlMagnitude, MinInput);
        }

        // Clamp magnitude within outer radius
        if (Dist < OuterRadius)
        {
            ControlMagnitude = FMath::Min(ControlMagnitude, FMath::Max(MinInput, Dist/OuterRadius));
        }

        // Apply control input
        ControlInput.Linear = DstNormal * ControlMagnitude;
        ControlInput.bEnableAcceleration = true;

        // Enable movement if currently disabled
        if (! bMoveTowardsTarget)
        {
            bMoveTowardsTarget = true;
        }
    }
    else
    {
        ControlInput.Linear = DstNormal;
        ControlInput.bEnableAcceleration = false;

        // Disable movement if currently enabled
        if (bMoveTowardsTarget)
        {
            bMoveTowardsTarget = false;
        }
    }

    OutControlInput = ControlInput;

    return false;
}

//bool FAdjustArrivalBehavior::CalculateSteeringImpl(float DeltaTime, FSteeringAcceleration& OutControlInput)
//{
//    check(HasValidData());
//
//    const FVector SrcLocation(Steerable->GetSteerableLocation());
//    const FVector SrcNormal(Steerable->GetForwardVector());
//
//    const FVector& DstLocation(TargetLocation);
//    const FVector DeltaLocation(DstLocation-SrcLocation);
//    const FVector DstNormal(DeltaLocation.GetSafeNormal());
//
//    const float DistSq = DeltaLocation.SizeSquared();
//    const float DstDot = SrcNormal | DstNormal;
//
//    // Check for arrival, 
//    if (DistSq < (TargetRadius*TargetRadius) || DstDot < DirectionThreshold)
//    {
//        return true;
//    }
//
//    // Output acceleration
//    OutControlInput = FSteeringAcceleration(DstNormal, true, true);
//
//    return false;
//}
