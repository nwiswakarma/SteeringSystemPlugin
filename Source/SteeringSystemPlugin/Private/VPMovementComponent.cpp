// 

#include "VPMovementComponent.h"
#include "VPawn.h"

//----------------------------------------------------------------------//
// UVPMovementComponent
//----------------------------------------------------------------------//
void UVPMovementComponent::SetUpdatedComponent(USceneComponent* NewUpdatedComponent)
{
	if (NewUpdatedComponent)
	{
		if (!ensureMsgf(Cast<AVPawn>(NewUpdatedComponent->GetOwner()), TEXT("%s must update a component owned by a Pawn"), *GetName()))
		{
			return;
		}
	}

	Super::SetUpdatedComponent(NewUpdatedComponent);

	PawnOwner = NewUpdatedComponent ? CastChecked<AVPawn>(NewUpdatedComponent->GetOwner()) : NULL;
}

void UVPMovementComponent::Serialize(FArchive& Ar)
{
	AVPawn* CurrentPawnOwner = PawnOwner;
	Super::Serialize(Ar);

	if (Ar.IsLoading())
	{
		// This was marked Transient so it won't be saved out, but we need still to reject old saved values.
		PawnOwner = CurrentPawnOwner;
	}
}

AVPawn* UVPMovementComponent::GetPawnOwner() const
{
	return PawnOwner;
}

void UVPMovementComponent::AddInputVector(FVector WorldAccel, bool bForce /*=false*/)
{
	if (PawnOwner)
	{
		PawnOwner->Internal_AddMovementInput(WorldAccel, bForce);
	}
}

void UVPMovementComponent::MarkInputEnabled(bool bEnable)
{
	if (PawnOwner)
	{
		PawnOwner->Internal_MarkInputEnabled(bEnable);
	}
}

void UVPMovementComponent::LockOrientation(bool bEnableLock)
{
	if (PawnOwner)
	{
		PawnOwner->Internal_LockOrientation(bEnableLock);
	}
}

FVector UVPMovementComponent::ConsumeInputVector()
{
	return PawnOwner ? PawnOwner->Internal_ConsumeMovementInputVector() : FVector::ZeroVector;
}

bool UVPMovementComponent::IsMoveInputIgnored() const
{
    if (UpdatedComponent && PawnOwner)
    {
        return PawnOwner->IsMoveInputIgnored();
    }
	// No UpdatedComponent or Pawn, no movement
	return true;
}

bool UVPMovementComponent::IsMoveInputEnabled() const
{
	return (UpdatedComponent && PawnOwner && PawnOwner->IsMoveInputEnabled());
}

bool UVPMovementComponent::IsMoveOrientationLocked() const
{
	return (UpdatedComponent && PawnOwner && PawnOwner->IsMoveOrientationLocked());
}

FVector UVPMovementComponent::GetPendingInputVector() const
{
	return PawnOwner ? PawnOwner->Internal_GetPendingMovementInputVector() : FVector::ZeroVector;
}

FVector UVPMovementComponent::GetLastInputVector() const
{
	return PawnOwner ? PawnOwner->Internal_GetLastMovementInputVector() : FVector::ZeroVector;
}
