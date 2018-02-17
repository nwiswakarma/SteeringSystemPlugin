// 

#pragma once

#include "SteeringBehavior.h"

class FRepulsionBehavior : public FSteeringBehavior
{
protected:

    FVector RepulsionLocation;
    float RepulsionDistance;

    virtual bool CalculateSteeringImpl(float DeltaTime, FSteeringAcceleration& OutControlInput) override;

public:

    FRepulsionBehavior(const FVector& InRepulsionLocation, float InRepulsionDistance)
        : RepulsionLocation(InRepulsionLocation)
        , RepulsionDistance(FMath::Abs(InRepulsionDistance))
    {
    }

    FORCEINLINE virtual FName GetType() const override
    {
        static const FName Type(TEXT("RepulsionBehavior"));
        return Type;
    }
};

class FLineRepulsionBehavior : public FSteeringBehavior
{
protected:

    FVector LineOrigin;
    FVector LineDirection;
    FVector ParallelRepulsionDirection;
    float RepulsionDistance;

    virtual bool CalculateSteeringImpl(float DeltaTime, FSteeringAcceleration& OutControlInput) override;

public:

    FLineRepulsionBehavior(const FVector& InLineOrigin, const FVector& InLineDirection, float InRepulsionDistance, const FVector& InParallelRepulsionDirection)
        : LineOrigin(InLineOrigin)
        , LineDirection(InLineDirection)
        , RepulsionDistance(FMath::Abs(InRepulsionDistance))
        , ParallelRepulsionDirection(InParallelRepulsionDirection)
    {
    }

    FORCEINLINE virtual FName GetType() const override
    {
        static const FName Type(TEXT("LineRepulsionBehavior"));
        return Type;
    }
};
