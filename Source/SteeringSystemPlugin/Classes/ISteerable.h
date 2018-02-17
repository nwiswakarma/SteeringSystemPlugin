// 

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Templates/SharedPointer.h"

class FSteeringBehavior;
class FSteeringFormation;

class ISteerable
{
public:
// ~ Steerable Interface
    // Behaviors
	virtual void AddSteeringBehavior(TSharedPtr<FSteeringBehavior> InBehavior) = 0;
	virtual void EnqueueSteeringBehavior(TSharedPtr<FSteeringBehavior> InBehavior) = 0;
	virtual void RemoveSteeringBehavior(TSharedPtr<FSteeringBehavior> InBehavior) = 0;
	virtual void ClearSteeringBehaviors() = 0;
    // Formations
	virtual void SetFormation(FSteeringFormation* InFormation, bool bSetAsPrimary=false) = 0;
	virtual void SetFormationDirect(FSteeringFormation* InFormation) = 0;
	virtual FSteeringFormation* GetFormation() = 0;
	virtual bool HasFormation() const = 0;
    // Steering Properties
	virtual FTickFunction* GetSteerableTickFunction() = 0;
	virtual FString GetSteerableName() const = 0;
	virtual FVector GetSteerableLocation() const = 0;
	virtual FQuat GetSteerableOrientation() const = 0;
	virtual FVector GetForwardVector() const = 0;
	virtual FVector GetLinearVelocity() const = 0;
	virtual float GetInnerRadius() const = 0;
	virtual float GetOuterRadius() const = 0;
	virtual int32 GetPriority() const = 0;
// ~ Limiter Interface
	virtual float GetMaxLinearSpeed() const = 0;
	virtual float GetMaxLinearAcceleration() const = 0;
	virtual float GetMinControlInput() const = 0;
};
