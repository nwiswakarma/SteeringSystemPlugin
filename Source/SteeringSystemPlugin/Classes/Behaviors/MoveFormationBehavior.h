////////////////////////////////////////////////////////////////////////////////
//
// MIT License
// 
// Copyright (c) 2018-2019 Nuraga Wiswakarma
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
////////////////////////////////////////////////////////////////////////////////
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
