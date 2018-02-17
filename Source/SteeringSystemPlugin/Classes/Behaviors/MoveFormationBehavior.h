// 

#pragma once

#include "FormationBehavior.h"
#include "Templates/SharedPointer.h"

class FMoveFormationBehavior : public FFormationBehavior
{
protected:

    TSet<ISteerable*> MemberSet;
    TArray<TSharedRef<bool>> FinishedBehaviors;

    FSteeringTarget SteeringTarget;
    float TargetRadius;
    bool bTargetReached;

    float FormationLockDelay;
    float FormationLockTimer;

    float MoveSpeedUpdateTimer;
    float MoveSpeedUpdateInterval;

    float PrimaryInnerRadius;
    float PrimaryOuterRadius;

    float AnchorInterpSpeed;
    float CurrentAnchorSpeed;
    float TargetAnchorSpeed;

    float VelocityLimitInterpSpeed;
    float CurrentVelocityLimit;
    float TargetVelocityLimit;

    virtual void OnActivated() override;
    virtual void OnDeactivated() override;
    virtual bool UpdateBehaviorState(float DeltaTime) override;
    virtual bool CalculateSteeringImpl(float DeltaTime) override;

    void UpdateMoveSpeed(float DeltaTime);
    void InterpMoveSpeed(float DeltaTime);

    void InitializeAnchor();
    void UpdateAnchorMovement(float DeltaTime);

    void UpdateBehaviorAssignments();
    void UpdateFormationOrientation();

public:

    FMoveFormationBehavior(const FSteeringTarget& InSteeringTarget)
        : SteeringTarget(InSteeringTarget)
        , TargetRadius(50.f)
        , bTargetReached(false)
        , PrimaryInnerRadius(300.f)
        , PrimaryOuterRadius(600.f)
        , FormationLockTimer(0.0f)
        , FormationLockDelay(0.5f)
        , MoveSpeedUpdateTimer(0.f)
        , MoveSpeedUpdateInterval(.1f)
        , AnchorInterpSpeed(1.f)
        , CurrentAnchorSpeed(0.f)
        , TargetAnchorSpeed(0.f)
        , VelocityLimitInterpSpeed(1.f)
        , CurrentVelocityLimit(0.f)
        , TargetVelocityLimit(0.f)
    {
        bRequirePrimary = true;
    }

    FORCEINLINE virtual FName GetType() const override
    {
        static const FName Type(TEXT("MoveFormationBehavior"));
        return Type;
    }
};
