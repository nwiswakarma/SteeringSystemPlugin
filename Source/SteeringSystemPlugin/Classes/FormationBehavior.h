// 

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "SteeringTypes.h"
#include "SteeringFormation.h"

class FFormationBehavior : public ISteeringBehaviorBase
{
protected:

    // Whether the behavior requires formation primary for calculation
    bool bRequirePrimary = false;

    // Owning formation
    FSteeringFormation* OwningFormation = nullptr;

    virtual bool UpdateBehaviorState(float DeltaTime)
    {
        // Blank implementation, return true as finished
        return true;
    }

    virtual bool CalculateSteeringImpl(float DeltaTime) = 0;

public:

    virtual ~FFormationBehavior()
    {
        OwningFormation = nullptr;
    }

    virtual void SetFormation(FSteeringFormation* InFormation)
    {
        OwningFormation = InFormation;
    }

    FORCEINLINE FSteeringFormation& GetFormation()
    {
        return *OwningFormation;
    }

    FORCEINLINE const FSteeringFormation& GetFormation() const
    {
        return *OwningFormation;
    }

    FORCEINLINE virtual bool HasValidData() const
    {
        return OwningFormation && OwningFormation->HasValidData() && (!bRequirePrimary || OwningFormation->HasPrimary());
    }

    bool CalculateSteering(float DeltaTime)
    {
        if (IsActive() && HasValidData())
        {
            return CalculateSteeringImpl(DeltaTime);
        }

        return true;
    }
};
