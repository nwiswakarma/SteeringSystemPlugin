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
#include "IFormationProxy.h"
#include "SteeringTypes.h"
#include "SteeringFormation.generated.h"

class FSteeringFormation;
class FSlotAssignmentStrategy;
class FPrimaryAssignmentStrategy;
class FFormationPattern;

typedef TSharedPtr<FSteeringFormation>          FPSSteeringFormation;
typedef TSharedPtr<FFormationPattern>           FPSFormationPattern;
typedef TSharedPtr<FSlotAssignmentStrategy>     FPSSlotAssignmentStrategy;
typedef TSharedPtr<FPrimaryAssignmentStrategy>  FPSPrimaryAssignmentStrategy;

typedef TMap<ISteerable*, int32>    FSlotAssignmentMap;
typedef TPair<ISteerable*, int32>   FSlotAssignmentPair;

// Utility Class Interfaces

class FFormationUtilityInterface
{
protected:
    FSteeringFormation* OwningFormation = nullptr;

public:
    virtual ~FFormationUtilityInterface() = default;

    virtual void SetOwningFormation(FSteeringFormation* InFormation)
    {
        OwningFormation = InFormation;
    }

    FORCEINLINE bool HasOwningFormation() const
    {
        return OwningFormation != nullptr;
    }

    FORCEINLINE FSteeringFormation& GetOwningFormation()
    {
        check(HasOwningFormation());
        return *OwningFormation;
    }

    FORCEINLINE const FSteeringFormation& GetOwningFormation() const
    {
        check(HasOwningFormation());
        return *OwningFormation;
    }
};

class FFormationPattern : public FFormationUtilityInterface
{
public:
    virtual bool IsSlotCountSupported(int32 SlotCount)
    {
        return true;
    }

    virtual void UpdateFormationPattern() = 0;
    virtual void CalculateSlotLocation(int32 SlotIndex, FVector& SlotLocation) = 0;
    virtual void CalculateSlotOrientation(int32 SlotIndex, FQuat& SlotOrientation) = 0;
};

class FSlotAssignmentStrategy : public FFormationUtilityInterface
{
public:
    virtual void UpdateSlotAssignments() = 0;
};

class FPrimaryAssignmentStrategy
{
public:
    virtual ISteerable* FindPrimary(const FSteeringFormation& Formation) = 0;
};

// Default Utility Classes

class FDefaultFormationPattern : public FFormationPattern
{
    int32 DimX;
    int32 DimY;
    float MaxRadius;

    FVector BoundingOrigin;
    TArray<FVector> SlotOffsets;

public:

    void CalculateDimension();
    void CalculateMaxRadius();
    void CalculateSlotOffsets();

    virtual void UpdateFormationPattern() override;
    virtual void CalculateSlotLocation(int32 SlotIndex, FVector& OutLocation) override;
    virtual void CalculateSlotOrientation(int32 SlotIndex, FQuat& SlotOrientation) override;
};

class FDefaultSlotAssignmentStrategy : public FSlotAssignmentStrategy
{
public:
    virtual void UpdateSlotAssignments() override;
};

class FDefaultPrimaryAssignmentStrategy : public FPrimaryAssignmentStrategy
{
public:
    virtual ISteerable* FindPrimary(const FSteeringFormation& Formation) override;
};

// Steering Formation Class

class FSteeringFormation
{
protected:

    IFormationProxy* FormationProxy;
    ISteerable* Primary;
    TArray<ISteerable*> Members;

    FPSPrimaryAssignmentStrategy PrimaryAssignmentStrategy;
    FPSFormationPattern FormationPattern;
    FPSSlotAssignmentStrategy SlotAssignmentStrategy;
    FSlotAssignmentMap SlotMap;
    bool bRequirePatternUpdate;

    FFormationBehaviorList Behaviors;

    float VelocityLimit;
    FQuat Orientation;

    void ResetRegisteredBehavior(FPSFormationBehavior& Behavior);

    void AddMemberDependency(ISteerable* Member);
    void RemoveMemberDependency(ISteerable* Member);

    // ~ Internal Update Functions

    void UpdatePrimaryAssignment();
    void UpdateFormationPattern();
    void UpdateSlotAssignments();
    void UpdateBehavior(float DeltaTime);
    bool UpdateActiveBehavior(float DeltaTime);

public:

    FSteeringFormation();
    ~FSteeringFormation();

    // Formation Tick Update
    void UpdateFormation(float DeltaTime);

    // ~ Formation Behavior Registration

    void AddBehavior(FPSFormationBehavior InBehavior);
    void EnqueueBehavior(FPSFormationBehavior InBehavior);
    void RemoveBehavior(FPSFormationBehavior InBehavior);
    void ClearBehaviors();

    // ~ Formation Proxy Functions

    void SetFormationProxy(IFormationProxy* InFormationProxy);

    FORCEINLINE IFormationProxy* GetFormationProxy() const
    {
        return FormationProxy;
    }

    // ~ Formation Primary Functions

    void SetPrimary(ISteerable* InPrimary);

    FORCEINLINE ISteerable* GetPrimary() const
    {
        return Primary;
    }

    FORCEINLINE bool HasPrimary() const
    {
        return Primary != nullptr;
    }

    // ~ Formation Pattern Functions

    void SetDefaultFormationPattern();
    void SetDefaultSlotAssignmentStrategy();
    void SetDefaultPrimaryAssignmentStrategy();

    void SetFormationPattern(FPSFormationPattern InFormationPattern);
    void SetSlotAssignmentStrategy(FPSSlotAssignmentStrategy InSlotAssignmentStrategy);
    void SetPrimaryAssignmentStrategy(FPSPrimaryAssignmentStrategy InPrimaryAssignmentStrategy);

    void CalculateSlotLocation(ISteerable* Member, FVector& SlotLocation);
    void CalculateSlotLocation(ISteerable* Member, FVector& SlotLocation, const FVector& SlotOffset);
    void CalculateSlotOrientation(ISteerable* Member, FQuat& SlotOrientation);

    FORCEINLINE bool IsPatternUpdateRequired() const
    {
        return bRequirePatternUpdate;
    }

    FORCEINLINE void MarkPatternUpdate()
    {
        if (! bRequirePatternUpdate)
        {
            bRequirePatternUpdate = true;
        }
    }

    FORCEINLINE FFormationPattern& GetPattern()
    {
        check(FormationPattern.IsValid());
        return *FormationPattern.Get();
    }

    FORCEINLINE FSlotAssignmentMap& GetSlotMap()
    {
        return SlotMap;
    }

    FORCEINLINE const FSlotAssignmentMap& GetSlotMap() const
    {
        return SlotMap;
    }

    // ~ Formation Member Functions

    int32 SetMembers(const TArray<ISteerable*>& InMembers);
    int32 ClearMembers();
    bool AddMember(ISteerable* Member, bool bSetAsPrimary=false);
    bool RemoveMember(ISteerable* Member);

    FORCEINLINE const TArray<ISteerable*>& GetMembers() const
    {
        return Members;
    }

    FORCEINLINE int32 GetMemberCount() const
    {
        return Members.Num();
    }

    FORCEINLINE ISteerable* GetMember(int32 MemberIndex) const
    {
        return Members.IsValidIndex(MemberIndex) ? Members[MemberIndex] : nullptr;
    }

    // ~ Formation Movement Functions

	FORCEINLINE bool HasVelocityLimit() const
    {
        return VelocityLimit > KINDA_SMALL_NUMBER;
    }

	FORCEINLINE float GetVelocityLimit() const
    {
        return VelocityLimit;
    }

	FORCEINLINE void SetVelocityLimit(float InVelocityLimit)
    {
        VelocityLimit = InVelocityLimit;
    }

	FORCEINLINE const FQuat& GetOrientation() const
    {
        return Orientation;
    }

	FORCEINLINE void SetOrientation(const FQuat& InOrientation)
    {
        Orientation = InOrientation;
    }

	FORCEINLINE FVector GetAnchorLocation() const
    {
        if (FormationProxy)
        {
            return FormationProxy->GetAnchorLocation();
        }
        return Primary ? Primary->GetSteerableLocation() : FVector();
    }

	FORCEINLINE FQuat GetAnchorOrientation() const
    {
        if (FormationProxy)
        {
            return FormationProxy->GetAnchorOrientation();
        }
        return Primary ? Primary->GetSteerableOrientation() : FQuat::Identity;
    }

	FORCEINLINE void SetAnchorLocationAndOrientation(const FVector& Location, const FQuat& Orientation)
    {
        if (FormationProxy)
        {
            FormationProxy->SetAnchorLocationAndOrientation(Location, Orientation);
        }
    }

	FORCEINLINE void SetAnchorLocation(const FVector& Location)
    {
        if (FormationProxy)
        {
            FormationProxy->SetAnchorLocation(Location);
        }
    }

	FORCEINLINE void SetAnchorOrientation(const FQuat& Orientation)
    {
        if (FormationProxy)
        {
            FormationProxy->SetAnchorOrientation(Orientation);
        }
    }

    FORCEINLINE bool HasValidData() const
    {
        return FormationProxy != nullptr;
    }
};

USTRUCT(BlueprintType)
struct STEERINGSYSTEMPLUGIN_API FSteeringFormationRef
{
	GENERATED_BODY()

    FSteeringFormation* Formation;

    FSteeringFormationRef() : Formation(nullptr)
    {
    }

    FSteeringFormationRef(FSteeringFormation& InFormation)
        : Formation(&InFormation)
    {
    }

    FORCEINLINE bool IsValid() const
    {
        return Formation != nullptr;
    }
};

USTRUCT(BlueprintType)
struct STEERINGSYSTEMPLUGIN_API FFormationBehaviorRef
{
	GENERATED_BODY()

    FPSFormationBehavior Behavior;

    FFormationBehaviorRef()
        : Behavior(nullptr)
    {
    }

    FFormationBehaviorRef(FPSFormationBehavior InBehavior)
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
