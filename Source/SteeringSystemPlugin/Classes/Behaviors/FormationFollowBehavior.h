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
