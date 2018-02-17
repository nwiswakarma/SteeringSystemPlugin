// 

#include "VSmoothDeltaActor.h"
#include "VSmoothDeltaMovementComponent.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/CollisionProfile.h"
#include "Net/UnrealNetwork.h"

FName AVSmoothDeltaActor::ShapeComponentName(TEXT("RootCollision"));
FName AVSmoothDeltaActor::MeshRootComponentName(TEXT("MeshRoot"));
FName AVSmoothDeltaActor::MovementComponentName(TEXT("SmoothDeltaMovement"));

AVSmoothDeltaActor::AVSmoothDeltaActor(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PrePhysics;

    bCanBeDamaged = true;
    
    bReplicates = true;
    bReplicateMovement = false;
    NetPriority = 3.0f;
    NetUpdateFrequency = 100.f;

    ReplicatedUpdateTimeStamp = 0.f;
    ReplicatedSimulationTime = 0.f;

    bCollideWhenPlacing = true;
    SpawnCollisionHandlingMethod = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;

    AutoReceiveInput = EAutoReceiveInput::Disabled;

    ShapeComponent = CreateDefaultSubobject<USceneComponent>(AVSmoothDeltaActor::ShapeComponentName);
    RootComponent = ShapeComponent;

    MeshRoot = CreateDefaultSubobject<USceneComponent>(AVSmoothDeltaActor::MeshRootComponentName);
    MeshRoot->SetupAttachment(RootComponent);

    if (SDMovement && RootComponent)
    {
        SDMovement->UpdatedComponent = RootComponent;
    }

#if WITH_EDITORONLY_DATA
    ArrowComponent = CreateEditorOnlyDefaultSubobject<UArrowComponent>(TEXT("Arrow"));
    if (ArrowComponent)
    {
        ArrowComponent->ArrowColor = FColor(150, 200, 255);
        ArrowComponent->bTreatAsASprite = true;
        ArrowComponent->bIsScreenSizeScaled = true;
        if (RootComponent)
        {
            ArrowComponent->SetupAttachment(RootComponent);
        }
    }
#endif // WITH_EDITORONLY_DATA
}

void AVSmoothDeltaActor::PostInitializeComponents()
{
    Super::PostInitializeComponents();

    if (! IsPendingKill())
    {
        if (! SDMovement)
        {
            SDMovement = FindComponentByClass<UVSmoothDeltaMovementComponent>();
        }
    }
}

UVSmoothDeltaMovementComponent* AVSmoothDeltaActor::GetMovementComponent() const
{
    return SDMovement;
}

void AVSmoothDeltaActor::SetMovementSource(UPrimitiveComponent* NewMovementSource)
{
    if (NewMovementSource != MovementSource)
    {
        // Set movement source
        UPrimitiveComponent* OldBase = MovementSource;
        MovementSource = NewMovementSource;

        // Notify this actor of its new movement source
        MovementSourceChange();
    }
}

void AVSmoothDeltaActor::MovementSourceChange()
{
    // Notify movement component of a new movement source
    if (SDMovement)
    {
        SDMovement->MovementSourceChange();
    }
}

void AVSmoothDeltaActor::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME_CONDITION( AVSmoothDeltaActor, ReplicatedMovementSource,  COND_SimulatedOnly );
    DOREPLIFETIME_CONDITION( AVSmoothDeltaActor, ReplicatedSimulationTime,  COND_SimulatedOnly );
    DOREPLIFETIME_CONDITION( AVSmoothDeltaActor, ReplicatedUpdateTimeStamp, COND_SimulatedOnlyNoReplay );

    // Change the condition of the replicated movement property to not replicate in replays since we handle this specifically
    // via saving this out in external replay data
    DOREPLIFETIME_CHANGE_CONDITION( AActor, ReplicatedMovement, COND_SimulatedOrPhysicsNoReplay );
}

void AVSmoothDeltaActor::PreReplication(IRepChangedPropertyTracker & ChangedPropertyTracker)
{
    Super::PreReplication(ChangedPropertyTracker);

    if (SDMovement)
    {
        ReplicatedUpdateTimeStamp = SDMovement->GetServerLastUpdateTimeStamp();
        ReplicatedSimulationTime = SDMovement->GetServerLastSimulationTime();
    }
    else
    {
        ReplicatedUpdateTimeStamp = 0.f;
        ReplicatedSimulationTime = 0.f;
    }

    ReplicatedMovementSource = MovementSource;
}

void AVSmoothDeltaActor::PostNetReceive()
{
    if (Role == ROLE_SimulatedProxy)
    {
        if (SDMovement)
        {
            const FVector OldLocation = GetActorLocation();
            const FQuat OldRotation = GetActorQuat();

            SDMovement->bNetworkSmoothingComplete = false;
            SDMovement->bNetworkUpdateReceived = true;

            SDMovement->SmoothCorrection(ReplicatedSimulationTime);

            OnUpdateSimulatedPosition(OldLocation, OldRotation);
        }
    }

    Super::PostNetReceive();
}

//void AVSmoothDeltaActor::PostNetReceiveVelocity(const FVector& NewVelocity)
//{
//    if (Role == ROLE_SimulatedProxy)
//    {
//        if (SDMovement)
//        {
//            SDMovement->Velocity = NewVelocity;
//        }
//    }
//}

//void AVSmoothDeltaActor::PostNetReceiveLocationAndRotation()
//{
//    if (Role == ROLE_SimulatedProxy)
//    {
//        //const FVector OldLocation = GetActorLocation();
//        //const FVector NewLocation = FRepMovement::RebaseOntoLocalOrigin(ReplicatedMovement.Location, this);
//        //const FQuat OldRotation = GetActorQuat();
//    
//        //SDMovement->bNetworkSmoothingComplete = false;
//        //SDMovement->SmoothCorrection(OldLocation, OldRotation, NewLocation, ReplicatedMovement.Rotation.Quaternion());
//        //OnUpdateSimulatedPosition(OldLocation, OldRotation);
//    }
//}

void AVSmoothDeltaActor::TornOff()
{
    Super::TornOff();

    if (SDMovement)
    {
        SDMovement->ResetClientSmoothingData();
    }
}

void AVSmoothDeltaActor::OnRep_ReplicatedMovementSource()
{
    if (Role == ROLE_SimulatedProxy)
    {
        if (MovementSource != ReplicatedMovementSource)
        {
            // Even though we will copy the replicated movement source, we need to use SetMovementSource() to trigger notifications.
            SetMovementSource(ReplicatedMovementSource);
        }

        MovementSource = ReplicatedMovementSource;
    }
}

void AVSmoothDeltaActor::OnUpdateSimulatedPosition(const FVector& OldLocation, const FQuat& OldRotation)
{
    if (SDMovement)
    {
        SDMovement->bJustTeleported = true;
    }
}

AVSmoothDeltaActor_BC::AVSmoothDeltaActor_BC(const FObjectInitializer& ObjectInitializer)
: Super( ObjectInitializer.SetDefaultSubobjectClass<UBoxComponent>(AVSmoothDeltaActor::ShapeComponentName) )
{
    BoxComponent = CastChecked<UBoxComponent>(GetShapeComponent());
    BoxComponent->InitBoxExtent(FVector(50.f, 50.f, 50.f));
    BoxComponent->CanCharacterStepUpOn = ECB_No;
    BoxComponent->bShouldUpdatePhysicsVolume = true;
    BoxComponent->bCheckAsyncSceneOnMove = false;
    BoxComponent->bDynamicObstacle = true;
    BoxComponent->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);
    BoxComponent->SetCanEverAffectNavigation(false);
}
