// 

#pragma once

#include "SteeringBehavior.h"

class FAlignBehavior : public FSteeringBehavior
{
protected:

    virtual bool CalculateSteeringImpl(float DeltaTime, FSteeringAcceleration& OutControlInput) override
    {
        check(HasValidData());

        const FVector SrcNormal(Steerable->GetForwardVector());
        const FVector DstNormal(SteeringTarget.GetForwardVector());

        // Check for alignment
        if (FVector::Coincident(SrcNormal, DstNormal))
        {
            return true;
        }

        OutControlInput = FSteeringAcceleration(DstNormal);

        return false;
    }

public:

    FSteeringTarget SteeringTarget;

    FAlignBehavior() = default;

    FAlignBehavior(const FSteeringTarget& InSteeringTarget) : SteeringTarget(InSteeringTarget)
    {
    }

    FORCEINLINE virtual FName GetType() const override
    {
        static const FName Type(TEXT("AlignBehavior"));
        return Type;
    }
};
