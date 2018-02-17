// 

#pragma once

#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"

class ISteerable;

class IFormationProxy
{
public:
	virtual void AddFormationBehavior(FPSFormationBehavior InBehavior) = 0;
	virtual void EnqueueFormationBehavior(FPSFormationBehavior InBehavior) = 0;
	virtual void RemoveFormationBehavior(FPSFormationBehavior InBehavior) = 0;
	virtual void ClearFormationBehaviors() = 0;
	virtual void AddMemberDependency(ISteerable* Member) = 0;
	virtual void RemoveMemberDependency(ISteerable* Member) = 0;
	virtual FVector GetAnchorLocation() const = 0;
	virtual FQuat GetAnchorOrientation() const = 0;
	virtual void SetAnchorLocationAndOrientation(const FVector& Location, const FQuat& Orientation) = 0;
	virtual void SetAnchorLocation(const FVector& Location) = 0;
	virtual void SetAnchorOrientation(const FQuat& Orientation) = 0;
};
