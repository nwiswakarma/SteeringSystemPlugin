// 

#include "VPawn.h"
#include "VPMovementComponent.h"
#include "DisplayDebugHelpers.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/InputDelegateBinding.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerState.h"
#include "Interfaces/NetworkPredictionInterface.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"

DEFINE_LOG_CATEGORY_STATIC(LogVPawn, Warning, All);

AVPawn::AVPawn(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PrePhysics;

    bCanBeDamaged = true;
    
    SetRemoteRoleForBackwardsCompat(ROLE_SimulatedProxy);
    bReplicates = true;
    NetPriority = 3.0f;
    NetUpdateFrequency = 100.f;
    bReplicateMovement = true;
    bCollideWhenPlacing = true;
    SpawnCollisionHandlingMethod = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;

    bIsMoveInputEnabled = true;
    bIsMoveInputIgnored = false;
    bIsMoveOrientationLocked = false;
    bInputEnabled = false;
    AutoReceiveInput = EAutoReceiveInput::Disabled;

    ReplicatedMovement.LocationQuantizationLevel = EVectorQuantization::RoundTwoDecimals;
}

UVPMovementComponent* AVPawn::GetMovementComponent() const
{
    return FindComponentByClass<UVPMovementComponent>();
}

FVector AVPawn::GetVelocity() const
{
    if (GetRootComponent() && GetRootComponent()->IsSimulatingPhysics())
    {
        return GetRootComponent()->GetComponentVelocity();
    }

    const UMovementComponent* MovementComponent = GetMovementComponent();
    return MovementComponent ? MovementComponent->Velocity : FVector::ZeroVector;
}

bool AVPawn::ShouldTakeDamage(float Damage, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) const
{
    if ((Role < ROLE_Authority) || !bCanBeDamaged || !GetWorld()->GetAuthGameMode() || (Damage == 0.f))
    {
        return false;
    }

    return true;
}

float AVPawn::TakeDamage(float Damage, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    if (!ShouldTakeDamage(Damage, DamageEvent, EventInstigator, DamageCauser))
    {
        return 0.f;
    }

    // Do not modify damage parameters after this
    const float ActualDamage = Super::TakeDamage(Damage, DamageEvent, EventInstigator, DamageCauser);

    // Respond to the damage
    if (ActualDamage != 0.f)
    {
        if (EventInstigator)
        {
            LastHitBy = EventInstigator;
        }
    }

    return ActualDamage;
}

AController* AVPawn::GetDamageInstigator(AController* InstigatedBy, const UDamageType& DamageType) const
{
    if (InstigatedBy != NULL)
    {
        return InstigatedBy;
    }
    else if (DamageType.bCausedByWorld && (LastHitBy != NULL))
    {
        return LastHitBy;
    }
    return InstigatedBy;
}

void AVPawn::AddMovementInput(FVector WorldDirection, float ScaleValue, bool bForce /*=false*/)
{
    UVPMovementComponent* MovementComponent = GetMovementComponent();
    if (MovementComponent)
    {
        MovementComponent->AddInputVector(WorldDirection * ScaleValue, bForce);
    }
    else
    {
        Internal_AddMovementInput(WorldDirection * ScaleValue, bForce);
    }
}

void AVPawn::MarkInputEnabled(bool bEnable)
{
    UVPMovementComponent* MovementComponent = GetMovementComponent();
    if (MovementComponent)
    {
        MovementComponent->MarkInputEnabled(bEnable);
    }
    else
    {
        Internal_MarkInputEnabled(bEnable);
    }
}

void AVPawn::LockOrientation(bool bEnableLock)
{
    UVPMovementComponent* MovementComponent = GetMovementComponent();
    if (MovementComponent)
    {
        MovementComponent->LockOrientation(bEnableLock);
    }
    else
    {
        Internal_LockOrientation(bEnableLock);
    }
}

FVector AVPawn::ConsumeMovementInputVector()
{
    UVPMovementComponent* MovementComponent = GetMovementComponent();
    if (MovementComponent)
    {
        return MovementComponent->ConsumeInputVector();
    }
    else
    {
        return Internal_ConsumeMovementInputVector();
    }
}

bool AVPawn::IsMoveInputIgnored() const
{
    return bIsMoveInputIgnored;
}

bool AVPawn::IsMoveInputEnabled() const
{
    return bIsMoveInputEnabled;
}

bool AVPawn::IsMoveOrientationLocked() const
{
    return bIsMoveOrientationLocked;
}

FVector AVPawn::GetPendingMovementInputVector() const
{
    return ControlInputVector;
}

FVector AVPawn::GetLastMovementInputVector() const
{
    return LastControlInputVector;
}

void AVPawn::Internal_AddMovementInput(FVector WorldAccel, bool bForce /*=false*/)
{
    if (bForce || !IsMoveInputIgnored())
    {
        ControlInputVector += WorldAccel;
    }
}

FVector AVPawn::Internal_ConsumeMovementInputVector()
{
    LastControlInputVector = ControlInputVector;
    ControlInputVector = FVector::ZeroVector;
    bIsMoveInputEnabled = true;
    bIsMoveOrientationLocked = false;
    return LastControlInputVector;
}

void AVPawn::Internal_MarkInputEnabled(bool bEnable)
{
    bIsMoveInputEnabled = bEnable;
}

void AVPawn::Internal_LockOrientation(bool bEnableLock)
{
    bIsMoveOrientationLocked = bEnableLock;
}

void AVPawn::TeleportSucceeded(bool bIsATest)
{
    if (bIsATest == false)
    {
        UMovementComponent* MovementComponent = GetMovementComponent();
        if (MovementComponent)
        {
            MovementComponent->OnTeleported();
        }
    }

    Super::TeleportSucceeded(bIsATest);
}

// REPLICATION

void AVPawn::PostNetReceiveVelocity(const FVector& NewVelocity)
{
    if (Role == ROLE_SimulatedProxy)
    {
        if (UMovementComponent* MovementComponent = GetMovementComponent())
        {
            MovementComponent->Velocity = NewVelocity;
        }
    }
}

void AVPawn::PostNetReceiveLocationAndRotation()
{
    // Always consider Location as changed if we were spawned this tick as in that case our replicated Location was set as part of spawning, before PreNetReceive()
    if ((CreationTime != GetWorld()->TimeSeconds)
        && (FRepMovement::RebaseOntoLocalOrigin(ReplicatedMovement.Location, this) == GetActorLocation() && ReplicatedMovement.Rotation == GetActorRotation())
    ) {
        return;
    }

    if (Role == ROLE_SimulatedProxy)
    {
        const FVector OldLocation = GetActorLocation();
        const FQuat OldRotation = GetActorQuat();
        const FVector NewLocation = FRepMovement::RebaseOntoLocalOrigin(ReplicatedMovement.Location, this);

        SetActorLocationAndRotation(NewLocation, ReplicatedMovement.Rotation, /*bSweep=*/ false);

        INetworkPredictionInterface* PredictionInterface = Cast<INetworkPredictionInterface>(GetMovementComponent());
        if (PredictionInterface)
        {
            PredictionInterface->SmoothCorrection(OldLocation, OldRotation, NewLocation, ReplicatedMovement.Rotation.Quaternion());
        }
    }
}

AActor* AVPawn::GetMovementBaseActor(const AVPawn* VPawn)
{
    if (VPawn != NULL && VPawn->GetMovementBase())
    {
        return VPawn->GetMovementBase()->GetOwner();
    }

    return NULL;
}

bool AVPawn::IsBasedOnActor(const AActor* Other) const
{
    UPrimitiveComponent * MovementBase = GetMovementBase();
    AActor* MovementBaseActor = MovementBase ? MovementBase->GetOwner() : NULL;

    if (MovementBaseActor && MovementBaseActor == Other)
    {
        return true;
    }

    return Super::IsBasedOnActor(Other);
}

void AVPawn::MoveIgnoreActorAdd(AActor* ActorToIgnore)
{
    UPrimitiveComponent* RootPrimitiveComponent = Cast<UPrimitiveComponent>(GetRootComponent());
    if( RootPrimitiveComponent )
    {
        RootPrimitiveComponent->IgnoreActorWhenMoving(ActorToIgnore, true);
    }
}

void AVPawn::MoveIgnoreActorRemove(AActor* ActorToIgnore)
{
    UPrimitiveComponent* RootPrimitiveComponent = Cast<UPrimitiveComponent>(GetRootComponent());
    if( RootPrimitiveComponent )
    {
        RootPrimitiveComponent->IgnoreActorWhenMoving(ActorToIgnore, false);
    }
}
