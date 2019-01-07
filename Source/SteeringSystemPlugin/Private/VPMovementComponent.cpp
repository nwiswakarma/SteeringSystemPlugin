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
