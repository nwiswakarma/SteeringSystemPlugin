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

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Templates/SharedPointer.h"
#include "ISteerable.h"
#include "SteeringTypes.h"
#include "SteeringBehavior.generated.h"

class FSteeringBehavior : public ISteeringBehaviorBase
{
protected:

    // Owning steerable
    ISteerable* Steerable = nullptr;

    // Steering calculation implementation
    virtual bool CalculateSteeringImpl(float DeltaTime, FSteeringAcceleration& OutControlInput) = 0;

public:

    virtual ~FSteeringBehavior()
    {
        Steerable = nullptr;
    }

    virtual void SetSteerable(ISteerable* InSteerable)
    {
        Steerable = InSteerable;
    }

    FORCEINLINE ISteerable* GetSteerable()
    {
        return Steerable;
    }

    FORCEINLINE virtual bool HasValidData() const
    {
        return Steerable != nullptr;
    }

    bool CalculateSteering(float DeltaTime, FSteeringAcceleration& OutControlInput)
    {
        if (IsActive() && HasValidData())
        {
            return CalculateSteeringImpl(DeltaTime, OutControlInput);
        }

        return true;
    }
};

class FGroupSteeringBehavior : public FSteeringBehavior
{
protected:

    TArray<ISteerable*> Members;
    ISteerable* Primary = nullptr;

public:

    FORCEINLINE ISteerable* GetPrimary() const
    {
        return Primary;
    }

    FORCEINLINE bool HasPrimary() const
    {
        return Primary != nullptr;
    }

    virtual void SetPrimary(ISteerable* InSteerable)
    {
        // Primary must be a group member
        if (Members.Contains(InSteerable))
        {
            Primary = InSteerable;
        }
        else
        {
            Primary = nullptr;
        }
    }

    virtual void SetMembers(TArray<ISteerable*> InMembers)
    {
        Members = InMembers;

        if (HasPrimary() && ! Members.Contains(Primary))
        {
            SetPrimary(nullptr);
        }
    }

    virtual void AddMember(ISteerable* Member, bool bSetAsPrimary=false)
    {
        if (Member)
        {
            Members.Emplace(Member);

            if (bSetAsPrimary)
            {
                SetPrimary(Member);
            }
        }
    }

    virtual void RemoveMember(ISteerable* Member)
    {
        if (Member)
        {
            Members.RemoveSwap(Member);

            if (Member == Primary)
            {
                SetPrimary(nullptr);
            }
        }
    }

    FORCEINLINE virtual bool HasValidData() const override
    {
        return Members.Num() > 0;
    }
};

USTRUCT(BlueprintType)
struct STEERINGSYSTEMPLUGIN_API FSteerableRef
{
	GENERATED_BODY()

    ISteerable* Steerable;

    FSteerableRef()
        : Steerable(nullptr)
    {
    }

    FSteerableRef(ISteerable& InSteerable)
        : Steerable(&InSteerable)
    {
    }

    FORCEINLINE bool IsValid() const
    {
        return Steerable != nullptr;
    }
};

USTRUCT(BlueprintType)
struct STEERINGSYSTEMPLUGIN_API FSteeringBehaviorRef
{
	GENERATED_BODY()

    FPSSteeringBehavior Behavior;

    FSteeringBehaviorRef()
        : Behavior(nullptr)
    {
    }

    FSteeringBehaviorRef(FPSSteeringBehavior InBehavior)
        : Behavior(InBehavior)
    {
    }

    FORCEINLINE bool IsValid() const
    {
        return Behavior.IsValid();
    }

    FORCEINLINE void Reset()
    {
        Behavior.Reset();
    }
};

USTRUCT(BlueprintType)
struct STEERINGSYSTEMPLUGIN_API FSteeringFormationData
{
	GENERATED_BODY()

    UPROPERTY()
    TArray<FVector> ProjectedLocations;

    UPROPERTY()
    FVector ForwardVector;

    int32 DimX;
    int32 DimY;
    float MaxRadius;
    FVector BoundingOrigin;
    TArray<ISteerable*> Agents;

    FSteeringFormationData() = default;

    FSteeringFormationData(const TArray<ISteerable*>& InAgents)
        : Agents(InAgents)
        , ForwardVector(FVector::ForwardVector)
    {
        ProjectedLocations.Reserve(InAgents.Num());
        CalculateDimension();
        CalculateMaxRadius();
    }

    FSteeringFormationData(const TArray<FSteerableRef>& InSteerableRefs)
        : ForwardVector(FVector::ForwardVector)
    {
        Agents.Reserve(InSteerableRefs.Num());

        for (const FSteerableRef& SteerableRef : InSteerableRefs)
        {
            if (ISteerable* Steerable = SteerableRef.Steerable)
            {
                Agents.Emplace(Steerable);
            }
        }

        ProjectedLocations.Reserve(Agents.Num());

        CalculateDimension();
        CalculateMaxRadius();
    }

    void CalculateDimension();
    void CalculateMaxRadius();
    void CalculateOrigin();
	void ProjectLocation(const FVector& TargetLocation);
    void ClearProjectedLocations();
};
