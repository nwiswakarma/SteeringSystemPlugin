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

#include "SteeringFormationComponent.h"

USteeringFormationComponent::USteeringFormationComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	PrimaryComponentTick.bCanEverTick = true;
	bAutoActivate = true;

    bAutoRegisterUpdatedComponent = true;
	bAutoUpdateTickRegistration = true;

    UpdatedComponent = nullptr;
}

void USteeringFormationComponent::InitializeComponent()
{
	Super::InitializeComponent();

	// RootComponent is null in OnRegister for blueprint (non-native) root components.
	if (!UpdatedComponent && bAutoRegisterUpdatedComponent)
	{
		// Auto-register owner's root component if found.
		if (AActor* MyActor = GetOwner())
		{
			if (USceneComponent* NewUpdatedComponent = MyActor->GetRootComponent())
			{
				SetUpdatedComponent(NewUpdatedComponent);
			}
		}
	}
}

void USteeringFormationComponent::OnRegister()
{
	Super::OnRegister();

	const UWorld* MyWorld = GetWorld();

	if (MyWorld && MyWorld->IsGameWorld())
	{
		USceneComponent* NewUpdatedComponent = UpdatedComponent;

		if (!UpdatedComponent && bAutoRegisterUpdatedComponent)
		{
			// Auto-register owner's root component if found.
			AActor* MyActor = GetOwner();
			if (MyActor)
			{
				NewUpdatedComponent = MyActor->GetRootComponent();
			}
		}

		SetUpdatedComponent(NewUpdatedComponent);
	}
}

void USteeringFormationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    Formation.ClearMembers();
    Formation.ClearBehaviors();

    Super::EndPlay(EndPlayReason);
}

void USteeringFormationComponent::SetUpdatedComponent(USceneComponent* NewUpdatedComponent)
{
    const bool bUpdatedComponentChanged = UpdatedComponent != NewUpdatedComponent;

	if (UpdatedComponent && bUpdatedComponentChanged)
	{
		// Remove from tick prerequisite
		UpdatedComponent->PrimaryComponentTick.RemovePrerequisite(this, PrimaryComponentTick); 
	}

	// Don't assign pending kill components, but allow those to null out previous UpdatedComponent.
	UpdatedComponent = IsValid(NewUpdatedComponent) ? NewUpdatedComponent : NULL;

	// Assign delegates
	if (UpdatedComponent && !UpdatedComponent->IsPendingKill())
	{
		// Force ticks after behavior updates
		UpdatedComponent->PrimaryComponentTick.AddPrerequisite(this, PrimaryComponentTick); 
	}

    // Assign formation anchor if updated component is valid
    Formation.SetFormationProxy(UpdatedComponent ? this : nullptr);

    // Clear formation members on update
    if (bUpdatedComponentChanged)
    {
        Formation.ClearMembers();
    }

	UpdateTickRegistration();
}

void USteeringFormationComponent::UpdateTickRegistration()
{
	if (bAutoUpdateTickRegistration)
	{
		const bool bHasUpdatedComponent = (UpdatedComponent != NULL);
		SetComponentTickEnabled(bHasUpdatedComponent && bAutoActivate);
	}
}

void USteeringFormationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Don't hang on to stale references to a destroyed UpdatedComponent.
	if (UpdatedComponent != NULL && UpdatedComponent->IsPendingKill())
	{
		SetUpdatedComponent(NULL);
	}

    if (HasValidData() && (GetOwner()->Role == ROLE_Authority))
    {
        UpdateFormation(DeltaTime);
    }
}

void USteeringFormationComponent::UpdateFormation(float DeltaTime)
{
    check(HasValidData());

    Formation.UpdateFormation(DeltaTime);
}

bool USteeringFormationComponent::HasValidData() const
{
    return UpdatedComponent != nullptr && GetOwner() != nullptr;
}

// ~ IFormationProxy Interface

void USteeringFormationComponent::AddFormationBehavior(FPSFormationBehavior InBehavior)
{
    if (HasValidData() && InBehavior.IsValid())
    {
        Formation.AddBehavior(InBehavior);
    }
}

void USteeringFormationComponent::EnqueueFormationBehavior(FPSFormationBehavior InBehavior)
{
    if (HasValidData() && InBehavior.IsValid())
    {
        Formation.EnqueueBehavior(InBehavior);
    }
}

void USteeringFormationComponent::RemoveFormationBehavior(FPSFormationBehavior InBehavior)
{
    if (HasValidData() && InBehavior.IsValid())
    {
        Formation.RemoveBehavior(InBehavior);
    }
}

void USteeringFormationComponent::ClearFormationBehaviors()
{
    Formation.ClearBehaviors();
}

void USteeringFormationComponent::AddMemberDependency(ISteerable* Member)
{
    if (Member)
    {
        if (FTickFunction* MemberTickFunction = Member->GetSteerableTickFunction())
        {
            MemberTickFunction->AddPrerequisite(this, PrimaryComponentTick);
        }
    }
}

void USteeringFormationComponent::RemoveMemberDependency(ISteerable* Member)
{
    if (Member)
    {
        if (FTickFunction* MemberTickFunction = Member->GetSteerableTickFunction())
        {
            MemberTickFunction->RemovePrerequisite(this, PrimaryComponentTick);
        }
    }
}

// ~ Blueprint Functions

void USteeringFormationComponent::K2_AddFormationBehavior(FFormationBehaviorRef& BehaviorRef, bool bPreserveData)
{
    if (BehaviorRef.IsValid())
    {
        AddFormationBehavior(BehaviorRef.Behavior);

        // Reset behavior reference after register if not preserving
        if (! bPreserveData)
        {
            BehaviorRef.Reset();
        }
    }
}

void USteeringFormationComponent::K2_EnqueueFormationBehavior(FFormationBehaviorRef& BehaviorRef, bool bPreserveData)
{
    if (BehaviorRef.IsValid())
    {
        EnqueueFormationBehavior(BehaviorRef.Behavior);

        // Reset behavior reference after register if not preserving
        if (! bPreserveData)
        {
            BehaviorRef.Reset();
        }
    }
}

void USteeringFormationComponent::K2_RemoveFormationBehavior(FFormationBehaviorRef& BehaviorRef, bool bPreserveData)
{
    if (BehaviorRef.IsValid())
    {
        RemoveFormationBehavior(BehaviorRef.Behavior);

        // Reset behavior reference after register if not preserving
        if (! bPreserveData)
        {
            BehaviorRef.Reset();
        }
    }
}

void USteeringFormationComponent::K2_ClearFormationBehaviors()
{
    ClearFormationBehaviors();
}

FSteeringFormationRef USteeringFormationComponent::K2_GetFormation()
{
    return FSteeringFormationRef(Formation);
}
