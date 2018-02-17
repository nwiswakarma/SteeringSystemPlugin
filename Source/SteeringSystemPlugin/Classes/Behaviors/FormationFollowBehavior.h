// 

#pragma once

#include "SteeringBehavior.h"

#include "Behaviors/AlignBehavior.h"
#include "Behaviors/ArriveBehavior.h"

class FFormationFollowBehavior : public FSteeringBehavior
{
protected:

    FArriveBehavior RegroupBehavior;
    FArriveBehavior CheckpointBehavior;
    FArriveBehavior ArriveBehavior;
    FAlignBehavior AlignBehavior;

    float ReachInterval;
    float MinVelocityLimit;

    FSteeringTarget SteeringTarget;
    TSharedRef<bool> bBehaviorFinished;

    FORCEINLINE void MarkFinished()
    {
        (*bBehaviorFinished) = true;
    }

    FORCEINLINE float CalculateCheckpoint(FVector& SlotLocation)
    {
        return CalculateCheckpoint(SlotLocation, ReachInterval);
    }

    FORCEINLINE void CalculateVelocityLimit(const FVector& SlotLocation, float InReachTime, float& VelocityLimit)
    {
        CalculateVelocityLimit(SlotLocation, ReachInterval, InReachTime, VelocityLimit);
    }

    float CalculateCheckpoint(FVector& SlotLocation, float InReachInterval);
    void CalculateVelocityLimit(const FVector& SlotLocation, float InReachInterval, float InReachTime, float& VelocityLimit);

    float GetMaxSpeed() const;
    void CalculateCurrentSlotLocation(FVector& SlotLocation);
    void CalculateTargetSlotLocation(FVector& SlotLocation);
    void ClampMaxInput(FSteeringAcceleration& ControlInput, float VelocityLimit) const;

    bool CalculateRegroupSteering(float DeltaTime, FSteeringAcceleration& ControlInput);

    virtual void OnActivated() override;
    virtual void OnDeactivated() override;
    virtual bool CalculateSteeringImpl(float DeltaTime, FSteeringAcceleration& ControlInput) override;

public:

    FFormationFollowBehavior(const FSteeringTarget& InSteeringTarget)
        : SteeringTarget(InSteeringTarget)
        , RegroupBehavior(50.f, 50.f)
        , CheckpointBehavior(25.f, 25.f)
        , ArriveBehavior(25.f, 25.f)
        , AlignBehavior()
        , ReachInterval(1.f)
        , MinVelocityLimit(.5f)
        , bBehaviorFinished(new bool(false))
    {
    }

    virtual ~FFormationFollowBehavior()
    {
        if (! (*bBehaviorFinished))
        {
            MarkFinished();
        }
    }

    virtual void SetSteerable(ISteerable* InSteerable) override;

    FORCEINLINE TSharedRef<bool> GetBehaviorFinishedPtr() const
    {
        return bBehaviorFinished;
    }

    FORCEINLINE virtual bool HasValidData() const override
    {
        return Steerable && Steerable->HasFormation();
    }

    FORCEINLINE virtual FName GetType() const override
    {
        static const FName Type(TEXT("FormationFollowBehavior"));
        return Type;
    }
};
