// 

#include "VPSteerableComponent.h"
#include "VPawn.h"
#include "VPMovementComponent.h"
#include "SteeringTypes.h"

#include "GameFramework/MovementComponent.h"

UVPSteerableComponent::UVPSteerableComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	PrimaryComponentTick.bCanEverTick = true;
	bAutoActivate = true;

    bAutoRegisterMovementComponent = true;
	bAutoUpdateTickRegistration = true;

    Priority = 0;
    InnerRadius = 50.f;
    OuterRadius = 150.f;
    MinControlInput = 0.1f;
}

//BEGIN Component Registration

void UVPSteerableComponent::OnRegister()
{
	Super::OnRegister();

	if (! MovementComponent && bAutoRegisterMovementComponent)
	{
		// Auto-register owner's root component if found.
		if (AActor* MyActor = GetOwner())
		{
            UVPMovementComponent* NewMovementComponent = MyActor->FindComponentByClass<UVPMovementComponent>();

			if (NewMovementComponent)
			{
				SetMovementComponent(NewMovementComponent);
			}
		}
	}
}

void UVPSteerableComponent::RegisterComponentTickFunctions(bool bRegister)
{
	Super::RegisterComponentTickFunctions(bRegister);

	// Super may start up the tick function when we don't want to.
	UpdateTickRegistration();
}

void UVPSteerableComponent::UpdateTickRegistration()
{
	if (bAutoUpdateTickRegistration)
	{
		const bool bHasMovementComponent = (MovementComponent != NULL);
		SetComponentTickEnabled(bHasMovementComponent && bAutoActivate);
	}
}

void UVPSteerableComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Clear formation
    if (Formation)
    {
        SetFormation(nullptr);
    }

    // Clear behaviors
    ClearSteeringBehaviors();

    Super::EndPlay(EndPlayReason);
}

void UVPSteerableComponent::SetMovementComponent(UVPMovementComponent* InMovementComponent)
{
    // Remove tick prerequisite from existing movement component
	if (MovementComponent && MovementComponent != InMovementComponent)
	{
		MovementComponent->PrimaryComponentTick.RemovePrerequisite(this, PrimaryComponentTick); 
	}

	MovementComponent = IsValid(InMovementComponent) && !InMovementComponent->IsPendingKill() ? InMovementComponent : NULL;

    // Add tick prerequisite to new movement component
	if (MovementComponent)
	{
		// Force ticks after steering behavior component updates
		MovementComponent->PrimaryComponentTick.AddPrerequisite(this, PrimaryComponentTick); 
	}

    PawnOwner = MovementComponent ? MovementComponent->GetPawnOwner() : nullptr;

	UpdateTickRegistration();
}

bool UVPSteerableComponent::HasValidData() const
{
    return MovementComponent && MovementComponent->UpdatedComponent && PawnOwner;
}

USceneComponent* UVPSteerableComponent::GetUpdatedComponent()
{
    check(MovementComponent);
    return MovementComponent->UpdatedComponent;
}

const USceneComponent* UVPSteerableComponent::GetUpdatedComponent() const
{
    check(MovementComponent);
    return MovementComponent->UpdatedComponent;
}

//END Component Registration

void UVPSteerableComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Don't hang on to stale references to a destroyed MovementComponent.
	if (! IsValid(MovementComponent) && MovementComponent->IsPendingKill())
	{
		SetMovementComponent(nullptr);
	}

    if (HasValidData())
    {
        UpdateBehavior(DeltaTime);
    }
}

void UVPSteerableComponent::UpdateBehavior(float DeltaTime)
{
    check(HasValidData());

    // Only execute as authority and there is any steering behavior
    if (PawnOwner->Role != ROLE_Authority || Behaviors.Num() == 0)
    {
        return;
    }

    bool bCurrentBehaviorFinished;

    // Execute current active behavior. If it finishes, immediately execute next behavior if any
    do
    {
        bCurrentBehaviorFinished = false;

        if (Behaviors.Num() > 0)
        {
            bCurrentBehaviorFinished = UpdateActiveBehavior(DeltaTime);
        }
    }
    while (bCurrentBehaviorFinished);
}

bool UVPSteerableComponent::UpdateActiveBehavior(float DeltaTime)
{
    check(Behaviors.Num() > 0);

    FBehaviorListNode* Node(Behaviors.GetTail());
    FPSSteeringBehavior& Behavior(Node->GetValue());
    bool bInProgress = Behavior.IsValid();
    bool bIsDone = false;

    if (bInProgress)
    {
        if (! Behavior->IsActive())
        {
            Behavior->Activate();
        }

        FSteeringAcceleration ControlInput;
        bIsDone = Behavior->CalculateSteering(DeltaTime, ControlInput);

        MovementComponent->AddInputVector(ControlInput.Linear);
        MovementComponent->MarkInputEnabled(ControlInput.bEnableAcceleration);
        MovementComponent->LockOrientation(ControlInput.bLockOrientation);

        if (bIsDone)
        {
            if (Behavior->IsActive())
            {
                Behavior->Deactivate();
            }

            bInProgress = false;
            BehaviorDoneEvent.Broadcast(Behavior->GetType());
        }
    }

    // Behavior is finished, remove from stack and return true
    if (! bInProgress)
    {
        if (Behavior.IsValid())
        {
            Behavior.Reset();
            Behaviors.RemoveNode(Node, true);
        }

        bIsDone = true;
    }

    return bIsDone;
}

// ~ Direct Functions

void UVPSteerableComponent::AddSteeringBehavior(FPSSteeringBehavior InBehavior)
{
    if (InBehavior.IsValid())
    {
        InBehavior->SetSteerable(this);
        Behaviors.AddTail(InBehavior);
    }
}

void UVPSteerableComponent::EnqueueSteeringBehavior(FPSSteeringBehavior InBehavior)
{
    if (InBehavior.IsValid())
    {
        InBehavior->SetSteerable(this);
        Behaviors.AddHead(InBehavior);
    }
}

void UVPSteerableComponent::RemoveSteeringBehavior(FPSSteeringBehavior InBehavior)
{
    if (InBehavior.IsValid())
    {
        FBehaviorListNode* Node(Behaviors.FindNode(InBehavior));

        if (Node)
        {
            FPSSteeringBehavior& Behavior(Node->GetValue());

            // Reset behavior
            ResetRegisteredBehavior(Behavior);

            // Remove behavior node
            Behaviors.RemoveNode(Node, true);
        }
    }
}

void UVPSteerableComponent::ClearSteeringBehaviors()
{
    for (FPSSteeringBehavior& Behavior : Behaviors)
    {
        ResetRegisteredBehavior(Behavior);
    }

    Behaviors.Empty();
}

void UVPSteerableComponent::ResetRegisteredBehavior(FPSSteeringBehavior& Behavior)
{
    if (Behavior.IsValid())
    {
        // Deactivate behavior if required
        if (Behavior->IsActive())
        {
            Behavior->Deactivate();
        }

        Behavior->SetSteerable(nullptr);
        Behavior.Reset();
    }
}

void UVPSteerableComponent::SetFormation(FSteeringFormation* InFormation, bool bSetAsPrimary)
{
    // Formation remain unmodified, abort
    if (Formation == InFormation)
    {
        return;
    }

    if (Formation)
    {
        Formation->RemoveMember(this);
    }

    Formation = InFormation;

    if (Formation)
    {
        Formation->AddMember(this, bSetAsPrimary);
    }
}

void UVPSteerableComponent::SetFormationDirect(FSteeringFormation* InFormation)
{
    Formation = InFormation;
}

FSteeringFormation* UVPSteerableComponent::GetFormation()
{
    return Formation;
}

bool UVPSteerableComponent::HasFormation() const
{
    return Formation != nullptr;
}

FTickFunction* UVPSteerableComponent::GetSteerableTickFunction()
{
    return &PrimaryComponentTick;
}

FString UVPSteerableComponent::GetSteerableName() const
{
    return GetNameSafe(GetOwner());
}

FVector UVPSteerableComponent::GetSteerableLocation() const
{
    return HasValidData() ? GetUpdatedComponent()->GetComponentLocation() : FVector::ZeroVector;
}

FQuat UVPSteerableComponent::GetSteerableOrientation() const
{
    return HasValidData() ? GetUpdatedComponent()->GetComponentQuat() : FQuat::Identity;
}

FVector UVPSteerableComponent::GetForwardVector() const
{
    return HasValidData() ? GetUpdatedComponent()->GetForwardVector() : FVector::ZeroVector;
}

FVector UVPSteerableComponent::GetLinearVelocity() const
{
    return MovementComponent ? MovementComponent->Velocity : FVector::ZeroVector;
}

float UVPSteerableComponent::GetInnerRadius() const
{
    return InnerRadius;
}

float UVPSteerableComponent::GetOuterRadius() const
{
    return OuterRadius;
}

int32 UVPSteerableComponent::GetPriority() const
{
    return Priority;
}

float UVPSteerableComponent::GetMaxLinearSpeed() const
{
    return MovementComponent ? MovementComponent->GetMaxSpeed() : 0.f;
}

float UVPSteerableComponent::GetMaxLinearAcceleration() const
{
    return 0.f;
}

float UVPSteerableComponent::GetMinControlInput() const
{
    return FMath::Max(MinControlInput, KINDA_SMALL_NUMBER);
}

// ~ Blueprint Functions

void UVPSteerableComponent::K2_SetFormation(FSteeringFormationRef& FormationRef, bool bSetAsPrimary)
{
    SetFormation(FormationRef.Formation, bSetAsPrimary);
}

void UVPSteerableComponent::K2_AddSteeringBehavior(FSteeringBehaviorRef& BehaviorRef, bool bPreserveData)
{
    if (BehaviorRef.IsValid())
    {
        AddSteeringBehavior(BehaviorRef.Behavior);

        // Reset behavior reference after register if not preserving
        if (! bPreserveData)
        {
            BehaviorRef.Reset();
        }
    }
}

void UVPSteerableComponent::K2_EnqueueSteeringBehavior(FSteeringBehaviorRef& BehaviorRef, bool bPreserveData)
{
    if (BehaviorRef.IsValid())
    {
        EnqueueSteeringBehavior(BehaviorRef.Behavior);

        // Reset behavior reference after register if not preserving
        if (! bPreserveData)
        {
            BehaviorRef.Reset();
        }
    }
}

void UVPSteerableComponent::K2_RemoveSteeringBehavior(FSteeringBehaviorRef& BehaviorRef, bool bPreserveData)
{
    if (BehaviorRef.IsValid())
    {
        RemoveSteeringBehavior(BehaviorRef.Behavior);

        // Reset behavior reference after register if not preserving
        if (! bPreserveData)
        {
            BehaviorRef.Reset();
        }
    }
}

void UVPSteerableComponent::K2_ClearSteeringBehaviors()
{
    ClearSteeringBehaviors();
}

FSteerableRef UVPSteerableComponent::K2_GetSteerable()
{
    return FSteerableRef(*this);
}

FVector UVPSteerableComponent::K2_GetSteerableLocation() const
{
    return GetSteerableLocation();
}

FVector UVPSteerableComponent::K2_GetLinearVelocity() const
{
    return GetLinearVelocity();
}

FVector UVPSteerableComponent::K2_GetForwardVector() const
{
    return GetForwardVector();
}

float UVPSteerableComponent::K2_GetInnerRadius() const
{
    return GetInnerRadius();
}

float UVPSteerableComponent::K2_GetOuterRadius() const
{
    return GetOuterRadius();
}

int32 UVPSteerableComponent::K2_GetPriority() const
{
    return GetPriority();
}

float UVPSteerableComponent::K2_GetMaxLinearSpeed() const
{
    return GetMaxLinearSpeed();
}

float UVPSteerableComponent::K2_GetMaxLinearAcceleration() const
{
    return GetMaxLinearAcceleration();
}

float UVPSteerableComponent::K2_GetMinControlInput() const
{
    return GetMinControlInput();
}
