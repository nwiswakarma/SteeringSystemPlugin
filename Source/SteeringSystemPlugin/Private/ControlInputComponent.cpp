// 

#include "ControlInputComponent.h"

UControlInputComponent::UControlInputComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
    bMoveInputIgnored = false;
    bEnableAcceleration = true;
}

void UControlInputComponent::AddInputVector(FVector WorldAccel, bool bForce /*=false*/)
{
    AddInputVector_Direct(WorldAccel, bForce);
}

void UControlInputComponent::SetEnableAcceleration(bool bInEnableAcceleration)
{
    SetEnableAcceleration_Direct(bInEnableAcceleration);
}

FVector UControlInputComponent::GetPendingInputVector() const
{
	return GetPendingInputVector_Direct();
}

FVector UControlInputComponent::GetLastInputVector() const
{
	return GetLastInputVector_Direct();
}

FVector UControlInputComponent::ConsumeInputVector()
{
	return ConsumeInputVector_Direct();
}

bool UControlInputComponent::IsMoveInputIgnored() const
{
	return IsMoveInputIgnored_Direct();
}

bool UControlInputComponent::IsAccelerationEnabled() const
{
    return IsAccelerationEnabled_Direct();
}

void UControlInputComponent::AddInputVector_Direct(FVector WorldAccel, bool bForce /*=false*/)
{
	if (bForce || ! IsMoveInputIgnored())
	{
		ControlInputVector += WorldAccel;
	}
}

void UControlInputComponent::SetEnableAcceleration_Direct(bool bInEnableAcceleration)
{
    bEnableAcceleration = bInEnableAcceleration;
}

FVector UControlInputComponent::GetPendingInputVector_Direct() const
{
	return ControlInputVector;
}

FVector UControlInputComponent::GetLastInputVector_Direct() const
{
	return LastControlInputVector;
}

FVector UControlInputComponent::ConsumeInputVector_Direct()
{
	LastControlInputVector = ControlInputVector;
	ControlInputVector = FVector::ZeroVector;
    bEnableAcceleration = true;
	return LastControlInputVector;
}

bool UControlInputComponent::IsMoveInputIgnored_Direct() const
{
	return bMoveInputIgnored;
}

bool UControlInputComponent::IsAccelerationEnabled_Direct() const
{
    return bEnableAcceleration;
}
