// 

#pragma once

#include "SteeringBehavior.h"

class FArriveBehavior : public FSteeringBehavior
{
protected:

    bool bMoveTowardsTarget = true;

    virtual bool CalculateSteeringImpl(float DeltaTime, FSteeringAcceleration& OutControlInput) override;

public:

    FSteeringTarget SteeringTarget;
    float OuterRadius;
    float InnerRadius;

    FArriveBehavior(float InOuterRadius, float InInnerRadius)
        : OuterRadius(FMath::Max(InOuterRadius, KINDA_SMALL_NUMBER))
        , InnerRadius(FMath::Max(InInnerRadius, KINDA_SMALL_NUMBER))
    {
        ClampRadius();
    }

    FArriveBehavior(const FSteeringTarget& InSteeringTarget, float InOuterRadius, float InInnerRadius)
        : SteeringTarget(InSteeringTarget)
        , OuterRadius(FMath::Max(InOuterRadius, KINDA_SMALL_NUMBER))
        , InnerRadius(FMath::Max(InInnerRadius, KINDA_SMALL_NUMBER))
    {
        ClampRadius();
    }

    FORCEINLINE void ClampRadius()
    {
        if (InnerRadius > OuterRadius)
        {
            InnerRadius = OuterRadius;
        }
    }

    FORCEINLINE virtual FName GetType() const override
    {
        static const FName Type(TEXT("ArriveBehavior"));
        return Type;
    }
};

//class FAdjustArrivalBehavior : public FSteeringBehavior
//{
//protected:
//
//    FVector TargetLocation;
//    float TargetRadius;
//    float DirectionThreshold;
//
//    virtual bool CalculateSteeringImpl(float DeltaTime, FSteeringAcceleration& OutControlInput) override;
//
//public:
//
//    FAdjustArrivalBehavior(const FVector& InTargetLocation, float InTargetRadius, float InDirectionThreshold)
//        : TargetLocation(InTargetLocation)
//        , TargetRadius(FMath::Max(InTargetRadius, KINDA_SMALL_NUMBER))
//        , DirectionThreshold(InDirectionThreshold)
//    {
//    }
//
//    FORCEINLINE virtual FName GetType() const override
//    {
//        static const FName Type(TEXT("ArriveBehavior"));
//        return Type;
//    }
//};
