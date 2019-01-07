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

/*=============================================================================
    Movement.cpp: Character movement implementation
=============================================================================*/

#include "VPCMovementComponent.h"
#include "VPawnChar.h"
#include "RVO3DAgentComponent.h"

#include "DrawDebugHelpers.h"
#include "EngineStats.h"
#include "Components/BrushComponent.h"
#include "Components/DestructibleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Canvas.h"
#include "Engine/DemoNetDriver.h"
#include "Engine/NetDriver.h"
#include "Engine/NetworkObjectList.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameNetworkManager.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PhysicsVolume.h"
#include "UObject/Package.h"


DEFINE_LOG_CATEGORY_STATIC(LogVPCMovement, Log, All);
DEFINE_LOG_CATEGORY_STATIC(LogVPCNetSmoothing, Log, All);

/**
 * Character stats
 */
DECLARE_CYCLE_STAT(TEXT("VPC Tick"), STAT_VPCMovementTick, STATGROUP_Steering);
DECLARE_CYCLE_STAT(TEXT("VPC NonSimulated Time"), STAT_VPCMovementNonSimulated, STATGROUP_Steering);
DECLARE_CYCLE_STAT(TEXT("VPC Simulated Time"), STAT_VPCMovementSimulated, STATGROUP_Steering);
DECLARE_CYCLE_STAT(TEXT("VPC PerformMovement"), STAT_VPCMovementPerformMovement, STATGROUP_Steering);
DECLARE_CYCLE_STAT(TEXT("VPC NetSmoothCorrection"), STAT_VPCMovementSmoothCorrection, STATGROUP_Steering);
DECLARE_CYCLE_STAT(TEXT("VPC SmoothClientPosition"), STAT_VPCMovementSmoothClientPosition, STATGROUP_Steering);
DECLARE_CYCLE_STAT(TEXT("VPC SmoothClientPosition_Interp"), STAT_VPCMovementSmoothClientPosition_Interp, STATGROUP_Steering);
DECLARE_CYCLE_STAT(TEXT("VPC SmoothClientPosition_Visual"), STAT_VPCMovementSmoothClientPosition_Visual, STATGROUP_Steering);
DECLARE_CYCLE_STAT(TEXT("VPC Physics Interaction"), STAT_VPCPhysicsInteraction, STATGROUP_Steering);
DECLARE_CYCLE_STAT(TEXT("VPC Update Acceleration"), STAT_VPCUpdateAcceleration, STATGROUP_Steering);
DECLARE_CYCLE_STAT(TEXT("VPC MoveUpdateDelegate"), STAT_VPCMoveUpdateDelegate, STATGROUP_Steering);

const float UVPCMovementComponent::MIN_TICK_TIME = 1e-6f;
const float UVPCMovementComponent::BRAKE_TO_STOP_VELOCITY = 10.f;

// CVars
namespace VPCMovementCVars
{
    // Listen server smoothing
    static int32 NetEnableListenServerSmoothing = 1;
    FAutoConsoleVariableRef CVarNetEnableListenServerSmoothing(
        TEXT("p.VPCNetEnableListenServerSmoothing"),
        NetEnableListenServerSmoothing,
        TEXT("Whether to enable mesh smoothing on listen servers for the local view of remote clients.\n")
        TEXT("0: Disable, 1: Enable"),
        ECVF_Default);

    // Logging when character is stuck. Off by default in shipping.
#if UE_BUILD_SHIPPING
    static float StuckWarningPeriod = -1.f;
#else
    static float StuckWarningPeriod = 1.f;
#endif

    FAutoConsoleVariableRef CVarStuckWarningPeriod(
        TEXT("p.VPCStuckWarningPeriod"),
        StuckWarningPeriod,
        TEXT("How often (in seconds) we are allowed to log a message about being stuck in geometry.\n")
        TEXT("<0: Disable, >=0: Enable and log this often, in seconds."),
        ECVF_Default);

    static int32 NetEnableMoveCombining = 1;
    FAutoConsoleVariableRef CVarNetEnableMoveCombining(
        TEXT("p.VPCNetEnableMoveCombining"),
        NetEnableMoveCombining,
        TEXT("Whether to enable move combining on the client to reduce bandwidth by combining similar moves.\n")
        TEXT("0: Disable, 1: Enable"),
        ECVF_Default);

    static int32 ReplayUseInterpolation = 1;
    FAutoConsoleVariableRef CVarReplayUseInterpolation(
        TEXT( "p.VPCReplayUseInterpolation" ),
        ReplayUseInterpolation,
        TEXT( "" ),
        ECVF_Default);

    static int32 FixReplayOverSampling = 1;
    FAutoConsoleVariableRef CVarFixReplayOverSampling(
        TEXT( "p.VPCFixReplayOverSampling" ),
        FixReplayOverSampling,
        TEXT( "If 1, remove invalid replay samples that can occur due to oversampling (sampling at higher rate than physics is being ticked)" ),
        ECVF_Default);

#if !UE_BUILD_SHIPPING

    static int32 NetShowCorrections = 0;
    FAutoConsoleVariableRef CVarNetShowCorrections(
        TEXT("p.VPCNetShowCorrections"),
        NetShowCorrections,
        TEXT("Whether to draw client position corrections (red is incorrect, green is corrected).\n")
        TEXT("0: Disable, 1: Enable"),
        ECVF_Cheat);

    static float NetCorrectionLifetime = 4.f;
    FAutoConsoleVariableRef CVarNetCorrectionLifetime(
        TEXT("p.VPCNetCorrectionLifetime"),
        NetCorrectionLifetime,
        TEXT("How long a visualized network correction persists.\n")
        TEXT("Time in seconds each visualized network correction persists."),
        ECVF_Cheat);

#endif // !UI_BUILD_SHIPPING


#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

    static float NetForceClientAdjustmentPercent = 0.f;
    FAutoConsoleVariableRef CVarNetForceClientAdjustmentPercent(
        TEXT("p.VPCNetForceClientAdjustmentPercent"),
        NetForceClientAdjustmentPercent,
        TEXT("Percent of ServerCheckClientError checks to return true regardless of actual error.\n")
        TEXT("Useful for testing client correction code.\n")
        TEXT("<=0: Disable, 0.05: 5% of checks will return failed, 1.0: Always send client adjustments"),
        ECVF_Cheat);

    static int32 VisualizeMovement = 0;
    FAutoConsoleVariableRef CVarVisualizeMovement(
        TEXT("p.VPCVisualizeMovement"),
        VisualizeMovement,
        TEXT("Whether to draw in-world debug information for character movement.\n")
        TEXT("0: Disable, 1: Enable"),
        ECVF_Cheat);

    static int32 NetVisualizeSimulatedCorrections = 0;
    FAutoConsoleVariableRef CVarNetVisualizeSimulatedCorrections(
        TEXT("p.VPCNetVisualizeSimulatedCorrections"),
        NetVisualizeSimulatedCorrections,
        TEXT("")
        TEXT("0: Disable, 1: Enable"),
        ECVF_Cheat);

    static int32 DebugTimeDiscrepancy = 0;
    FAutoConsoleVariableRef CVarDebugTimeDiscrepancy(
        TEXT("p.VPCDebugTimeDiscrepancy"),
        DebugTimeDiscrepancy,
        TEXT("Whether to log detailed Movement Time Discrepancy values for testing")
        TEXT("0: Disable, 1: Enable Detection logging, 2: Enable Detection and Resolution logging"),
        ECVF_Cheat);
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}



//BEGIN FVPCMovementComponentPostPhysicsTickFunction

void FVPCMovementComponentPostPhysicsTickFunction::ExecuteTick(float DeltaTime, enum ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
    FActorComponentTickFunction::ExecuteTickHelper(Target, /*bTickInEditor=*/ false, DeltaTime, TickType, [this](float DilatedTime)
    {
        Target->PostPhysicsTickComponent(DilatedTime, *this);
    });
}

FString FVPCMovementComponentPostPhysicsTickFunction::DiagnosticMessage()
{
    return Target->GetFullName() + TEXT("[UVPCMovementComponent::PreClothTick]");
}

//END FVPCMovementComponentPostPhysicsTickFunction

UVPCMovementComponent::UVPCMovementComponent(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    PostPhysicsTickFunction.bCanEverTick = true;
    PostPhysicsTickFunction.bStartWithTickEnabled = false;
    PostPhysicsTickFunction.TickGroup = TG_PostPhysics;

    bEnableScopedMovementUpdates = true;

    OldBaseQuat = FQuat::Identity;
    OldBaseLocation = FVector::ZeroVector;

    // Speed & Acceleration
    MaxAcceleration = 2048.f;
    MaxDeceleration = 2048.f;
    MaxZeroInputDeceleration = 2048.f;
    MaxSpeed = 600.0f;
    MaxLockedOrientationSpeed = 50.0f;
    MaxCustomMovementSpeed = MaxSpeed;

    // Braking Friction
    bEnableVolumeFriction = false;
    bUseSeparateBrakingFriction = true;
    BrakingFriction = 0.f;
    BrakingFrictionFactor = 2.f;

    // Orientation
    bLockedOrientation = false;
    bUseTurnRange = false;
    TurnRate = 45.f;
    MinTurnRate = -1.f;
    TurnAcceleration = 45.f;
    CurrentTurnRate = 0.f;

    RotationRate = FRotator(0.f, 360.0f, 0.0f);

    // Depenetration
    MaxDepenetrationWithGeometry = 500.f;
    MaxDepenetrationWithGeometryAsProxy = 100.f;
    MaxDepenetrationWithPawn = 100.f;
    MaxDepenetrationWithPawnAsProxy = 2.f;

    // Network
    bNetworkSmoothingComplete = true; // Initially true until we get a net update, so we don't try to smooth to an uninitialized value.
    NetworkSimulatedSmoothLocationTime = 0.100f;
    NetworkSimulatedSmoothRotationTime = 0.033f;
    ListenServerNetworkSimulatedSmoothLocationTime = 0.040f;
    ListenServerNetworkSimulatedSmoothRotationTime = 0.033f;
    NetworkMaxSmoothUpdateDistance = 256.f;
    NetworkNoSmoothUpdateDistance = 384.f;
    NetworkSmoothingMode = ENetworkSmoothingMode::Exponential;
    bCancelAdaptiveReplicationEnabled = true;

    ClientPredictionData = NULL;
    ServerPredictionData = NULL;
    
    // Physics Interaction
    Mass = 100.0f;
    bJustTeleported = true;
    LastUpdateRotation = FQuat::Identity;
    LastUpdateVelocity = FVector::ZeroVector;
    PendingImpulseToApply = FVector::ZeroVector;

    bEnablePhysicsInteraction = true;
    InitialPushForceFactor = 500.0f;
    PushForceFactor = 750000.0f;
    bPushForceScaledToMass = false;
    bScalePushForceToVelocity = true;
    
    TouchForceFactor = 1.0f;
    bTouchForceScaledToMass = true;
    MinTouchForce = -1.0f;
    MaxTouchForce = 250.0f;
    RepulsionForce = 2.5f;
    bFastAttachedMove = false;

    // Avoidance
    bAutoRegisterRVOAgentComponent = true;
    bUseRVOAvoidance = false;
    bUseRVOPostProcess = false;
    bZeroVelocityRVO = false;
    ZeroVelocityRVOThreshold = 20.f;
    ZeroVelocityRVOLockDuration = 1.f;
    ZeroVelocityRVOMaxSpeed = 50.f;
    MinAvoidanceMagnitude = 0.5f;
}

void UVPCMovementComponent::PostLoad()
{
    Super::PostLoad();

    CharacterOwner = Cast<AVPawnChar>(PawnOwner);
}

void UVPCMovementComponent::OnRegister()
{
    const ENetMode NetMode = GetNetMode();

    // Never enable avoidance on client
    if (bUseRVOAvoidance && NetMode == NM_Client)
    {
        bUseRVOAvoidance = false;
    }

    Super::OnRegister();

    // Force linear smoothing for replays.
    const UWorld* MyWorld = GetWorld();
    const bool IsReplay = (MyWorld && MyWorld->DemoNetDriver && MyWorld->DemoNetDriver->ServerConnection);

    if (IsReplay)
    {
        if (VPCMovementCVars::ReplayUseInterpolation == 1)
        {
            NetworkSmoothingMode = ENetworkSmoothingMode::Replay;
        }
        else
        {
            NetworkSmoothingMode = ENetworkSmoothingMode::Linear;
        }
    }
    else if (NetMode == NM_ListenServer)
    {
        // Linear smoothing works on listen servers, but makes a lot less sense under the typical high update rate.
        if (NetworkSmoothingMode == ENetworkSmoothingMode::Linear)
        {
            NetworkSmoothingMode = ENetworkSmoothingMode::Exponential;
        }
    }

    // Auto register rvo agent component if enabled
    if (! RVOAgentComponent && bUseRVOAvoidance && bAutoRegisterRVOAgentComponent)
    {
        // Auto-register owner's rvo agent component if found.
        if (AActor* MyActor = GetOwner())
        {
            URVO3DAgentComponent* NewRVOAgentComponent = MyActor->FindComponentByClass<URVO3DAgentComponent>();

            if (NewRVOAgentComponent)
            {
                SetRVOAgentComponent(NewRVOAgentComponent);
            }
        }
    }

    RefreshTurnRange();
}

void UVPCMovementComponent::BeginDestroy()
{
    if (ClientPredictionData)
    {
        delete ClientPredictionData;
        ClientPredictionData = NULL;
    }
    
    if (ServerPredictionData)
    {
        delete ServerPredictionData;
        ServerPredictionData = NULL;
    }
    
    Super::BeginDestroy();
}

void UVPCMovementComponent::Deactivate()
{
    Super::Deactivate();

    if (! IsActive())
    {
        ClearAccumulatedForces();
    }
}

void UVPCMovementComponent::SetUpdatedComponent(USceneComponent* NewUpdatedComponent)
{
    if (NewUpdatedComponent)
    {
        const AVPawnChar* NewCharacterOwner = Cast<AVPawnChar>(NewUpdatedComponent->GetOwner());

        if (NewCharacterOwner == NULL)
        {
            UE_LOG(LogVPCMovement, Error, TEXT("%s owned by %s must update a component owned by a Character"), *GetName(), *GetNameSafe(NewUpdatedComponent->GetOwner()));
            return;
        }

        // Check that UpdatedComponent is a PrimitiveComponent
        if (Cast<UPrimitiveComponent>(NewUpdatedComponent) == NULL)
        {
            UE_LOG(LogVPCMovement, Error, TEXT("%s owned by %s must update a primitive component"), *GetName(), *GetNameSafe(NewUpdatedComponent->GetOwner()));
            return;
        }
    }

    if (bMovementInProgress)
    {
        // failsafe to avoid crashes in CharacterMovement. 
        bDeferUpdateMoveComponent = true;
        DeferredUpdatedMoveComponent = NewUpdatedComponent;
        return;
    }

    bDeferUpdateMoveComponent = false;
    DeferredUpdatedMoveComponent = NULL;

    USceneComponent* OldUpdatedComponent = UpdatedComponent;
    UPrimitiveComponent* OldPrimitive = Cast<UPrimitiveComponent>(UpdatedComponent);
    if (IsValid(OldPrimitive) && OldPrimitive->OnComponentBeginOverlap.IsBound())
    {
        OldPrimitive->OnComponentBeginOverlap.RemoveDynamic(this, &UVPCMovementComponent::PrimitiveTouched);
    }
    
    Super::SetUpdatedComponent(NewUpdatedComponent);

    CharacterOwner = Cast<AVPawnChar>(PawnOwner);

    if (UpdatedComponent != OldUpdatedComponent)
    {
        ClearAccumulatedForces();
        RefreshTurnRange();
    }

    if (UpdatedComponent == NULL)
    {
        StopMovementImmediately();
    }

    const bool bValidUpdatedPrimitive = IsValid(UpdatedPrimitive);

    if (bValidUpdatedPrimitive && bEnablePhysicsInteraction)
    {
        UpdatedPrimitive->OnComponentBeginOverlap.AddUniqueDynamic(this, &UVPCMovementComponent::PrimitiveTouched);
    }
}

bool UVPCMovementComponent::HasValidData() const
{
    bool bIsValid = UpdatedComponent && IsValid(CharacterOwner);
#if ENABLE_NAN_DIAGNOSTIC
    if (bIsValid)
    {
        // NaN-checking updates
        if (Velocity.ContainsNaN())
        {
            logOrEnsureNanError(TEXT("UVPCMovementComponent::HasValidData() detected NaN/INF for (%s) in Velocity:\n%s"), *GetPathNameSafe(this), *Velocity.ToString());
            UVPCMovementComponent* MutableThis = const_cast<UVPCMovementComponent*>(this);
            MutableThis->Velocity = FVector::ZeroVector;
        }
        if (!UpdatedComponent->GetComponentTransform().IsValid())
        {
            logOrEnsureNanError(TEXT("UVPCMovementComponent::HasValidData() detected NaN/INF for (%s) in UpdatedComponent->ComponentTransform:\n%s"), *GetPathNameSafe(this), *UpdatedComponent->GetComponentTransform().ToHumanReadableString());
        }
        if (UpdatedComponent->GetComponentRotation().ContainsNaN())
        {
            logOrEnsureNanError(TEXT("UVPCMovementComponent::HasValidData() detected NaN/INF for (%s) in UpdatedComponent->GetComponentRotation():\n%s"), *GetPathNameSafe(this), *UpdatedComponent->GetComponentRotation().ToString());
        }
    }
#endif
    return bIsValid;
}

FVector UVPCMovementComponent::GetBestDirectionOffActor(AActor* BaseActor) const
{
    // By default, just pick a random direction.  Derived character classes can choose to do more complex calculations,
    // such as finding the shortest distance to move in based on the BaseActor's Bounding Volume.
    const float RandAngle = FMath::DegreesToRadians(GetNetworkSafeRandomAngleDegrees());
    return FVector(FMath::Cos(RandAngle), FMath::Sin(RandAngle), 0.5f).GetSafeNormal();
}

float UVPCMovementComponent::GetNetworkSafeRandomAngleDegrees() const
{
    float Angle = FMath::SRand() * 360.f;

    if (! IsNetMode(NM_Standalone))
    {
        // Networked game
        // Get a timestamp that is relatively close between client and server (within ping).
        FNetworkPredictionData_Client_VPC const* ClientData = (HasPredictionData_Client() ? GetPredictionData_Client_VPC() : NULL);

        float TimeStamp = Angle;

        if (ClientData)
        {
            TimeStamp = ClientData->CurrentTimeStamp;
        }
        
        // Convert to degrees with a faster period.
        const float PeriodMult = 8.0f;
        Angle = TimeStamp * PeriodMult;
        Angle = FMath::Fmod(Angle, 360.f);
    }

    return Angle;
}

void UVPCMovementComponent::SetDefaultMovementMode()
{
    SetMovementMode(MOVE_Flying);
}

void UVPCMovementComponent::SetMovementMode(EMovementMode NewMovementMode, uint8 NewCustomMode)
{
    if (NewMovementMode != MOVE_Custom)
    {
        NewCustomMode = 0;
    }

    // Do nothing if nothing is changing.
    if (MovementMode == NewMovementMode)
    {
        // Allow changes in custom sub-mode.
        if ((NewMovementMode != MOVE_Custom) || (NewCustomMode == CustomMovementMode))
        {
            return;
        }
    }

    const EMovementMode PrevMovementMode = MovementMode;
    const uint8 PrevCustomMode = CustomMovementMode;

    MovementMode = NewMovementMode;
    CustomMovementMode = NewCustomMode;

    // We allow setting movement mode before we have a component to update, in case this happens at startup.
    if (! HasValidData())
    {
        return;
    }
    
    // Handle change in movement mode
    OnMovementModeChanged(PrevMovementMode, PrevCustomMode);

    // @todo UE4 do we need to disable ragdoll physics here? Should this function do nothing if in ragdoll?
}

void UVPCMovementComponent::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
    if (! HasValidData())
    {
        return;
    }

    // React to changes in the movement mode.
    {
        SetBase(NULL);

        if (MovementMode == MOVE_None)
        {
            // Kill velocity and clear queued up events
            ClearAccumulatedForces();
        }
    }

    CharacterOwner->OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);
}

uint8 UVPCMovementComponent::PackNetworkMovementMode() const
{
    if (MovementMode != MOVE_Custom)
    {
        return uint8(MovementMode.GetValue());
    }
    else
    {
        return CustomMovementMode + VPCPackedMovementModeConstants::CustomModeThr;
    }
}

void UVPCMovementComponent::UnpackNetworkMovementMode(const uint8 ReceivedMode, TEnumAsByte<EMovementMode>& OutMode, uint8& OutCustomMode) const
{
    if (ReceivedMode < VPCPackedMovementModeConstants::CustomModeThr)
    {
        OutMode = TEnumAsByte<EMovementMode>(ReceivedMode & VPCPackedMovementModeConstants::GroundMask);
        OutCustomMode = 0;
    }
    else
    {
        OutMode = MOVE_Custom;
        OutCustomMode = ReceivedMode - VPCPackedMovementModeConstants::CustomModeThr;
    }
}

void UVPCMovementComponent::ApplyNetworkMovementMode(const uint8 ReceivedMode)
{
    TEnumAsByte<EMovementMode> NetMovementMode(MOVE_None);
    uint8 NetCustomMode(0);
    UnpackNetworkMovementMode(ReceivedMode, NetMovementMode, NetCustomMode);

    SetMovementMode(NetMovementMode, NetCustomMode);
}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
static void DrawCircle( const UWorld* InWorld, const FVector& Base, const FVector& X, const FVector& Y, const FColor& Color, float Radius, int32 NumSides, bool bPersistentLines, float LifeTime, uint8 DepthPriority=0, float Thickness=0.0f )
{
    const float AngleDelta = 2.0f * PI / NumSides;
    FVector LastVertex = Base + X * Radius;

    for ( int32 SideIndex = 0; SideIndex < NumSides; SideIndex++ )
    {
        const FVector Vertex = Base + ( X * FMath::Cos( AngleDelta * ( SideIndex + 1 ) ) + Y * FMath::Sin( AngleDelta * ( SideIndex + 1 ) ) ) * Radius;
        DrawDebugLine( InWorld, LastVertex, Vertex, Color, bPersistentLines, LifeTime, DepthPriority, Thickness );
        LastVertex = Vertex;
    }
}
#endif

void UVPCMovementComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
    SCOPED_NAMED_EVENT(UVPCMovementComponent_TickComponent, FColor::Yellow);
    SCOPE_CYCLE_COUNTER(STAT_VPCMovementTick);

    bLockedOrientation = IsMoveOrientationLocked();

    const bool bIsMoveInputEnabled = IsMoveInputEnabled();
    const FVector InputVector = ConsumeInputVector();

    if (! HasValidData() || ShouldSkipUpdate(DeltaTime))
    {
        return;
    }

    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // Super tick may destroy/invalidate CharacterOwner or UpdatedComponent, so we need to re-check.
    if (! HasValidData())
    {
        return;
    }

    // See if we fell out of the world.
    const bool bIsSimulatingPhysics = UpdatedComponent->IsSimulatingPhysics();

    if (CharacterOwner->Role == ROLE_Authority && !CharacterOwner->CheckStillInWorld())
    {
        return;
    }

    // We don't update if simulating physics (eg ragdolls).
    if (bIsSimulatingPhysics)
    {
        ClearAccumulatedForces();
        return;
    }

    // Authority (server) movement
    if (CharacterOwner->Role == ROLE_Authority)
    {
        SCOPE_CYCLE_COUNTER(STAT_VPCMovementNonSimulated);

        if (bUseRVOAvoidance)
        {
            CalcAvoidanceVelocity();
        }

        {
            SCOPE_CYCLE_COUNTER(STAT_VPCUpdateAcceleration);

            // Apply input to acceleration
            Acceleration = ScaleInputAcceleration(ConstrainInputAcceleration(InputVector));
            AnalogInputModifier = bIsMoveInputEnabled ? ComputeAnalogInputModifier() : 0.f;
        }

        PerformMovement(DeltaTime);
    }
    // Client (replicated predictive) movement
    else if (CharacterOwner->Role == ROLE_SimulatedProxy)
    {
        SimulatedTick(DeltaTime);
    }

    if (bEnablePhysicsInteraction)
    {
        SCOPE_CYCLE_COUNTER(STAT_VPCPhysicsInteraction);
        ApplyRepulsionForce(DeltaTime);
    }

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
    const bool bVisualizeMovement = VPCMovementCVars::VisualizeMovement > 0;
    if (bVisualizeMovement)
    {
        VisualizeMovement();
    }
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

}

void UVPCMovementComponent::PostPhysicsTickComponent(float DeltaTime, FVPCMovementComponentPostPhysicsTickFunction& ThisTickFunction)
{
    if (bDeferUpdateBasedMovement)
    {
        FScopedMovementUpdate ScopedMovementUpdate(UpdatedComponent, bEnableScopedMovementUpdates ? EScopedUpdate::DeferredUpdates : EScopedUpdate::ImmediateUpdates);
        UpdateBasedMovement(DeltaTime);
        SaveBaseLocation();
        bDeferUpdateBasedMovement = false;
    }
}

void UVPCMovementComponent::SimulatedTick(float DeltaTime)
{
    SCOPE_CYCLE_COUNTER(STAT_VPCMovementSimulated);
    checkSlow(CharacterOwner != nullptr);

    if (NetworkSmoothingMode == ENetworkSmoothingMode::Replay)
    {
        const FVector OldLocation = UpdatedComponent ? UpdatedComponent->GetComponentLocation() : FVector::ZeroVector;
        const FVector OldVelocity = Velocity;

        // Interpolate between appropriate samples
        {
            SCOPE_CYCLE_COUNTER(STAT_VPCMovementSmoothClientPosition);
            SmoothClientPosition(DeltaTime);
        }

        // Update replicated movement mode
        ApplyNetworkMovementMode(CharacterOwner->GetReplicatedMovementMode());

        UpdateComponentVelocity();
        bJustTeleported = false;

        // Note: we do not call the Super implementation, that runs prediction.
        // We do still need to call these though
        OnMovementUpdated(DeltaTime, OldLocation, OldVelocity);
        CallMovementUpdateDelegate(DeltaTime, OldLocation, OldVelocity);

        LastUpdateLocation = UpdatedComponent ? UpdatedComponent->GetComponentLocation() : FVector::ZeroVector;
        LastUpdateRotation = UpdatedComponent ? UpdatedComponent->GetComponentQuat() : FQuat::Identity;
        LastUpdateVelocity = Velocity;

        //TickCharacterPose( DeltaTime );
        return;
    }

    {
        // Avoid moving the mesh during movement if SmoothClientPosition will take care of it.
        const FScopedPreventAttachedComponentMove PreventMeshMovement(bNetworkSmoothingComplete ? nullptr : CharacterOwner->GetMeshRoot());

        if (CharacterOwner->bReplicateMovement)
        {
            if (CharacterOwner->IsMatineeControlled())
            {
                PerformMovement(DeltaTime);
            }
            else
            {
                SimulateMovement(DeltaTime);
            }
        }
    }

    // Smooth mesh location after moving the capsule above.
    if (!bNetworkSmoothingComplete)
    {
        SCOPE_CYCLE_COUNTER(STAT_VPCMovementSmoothClientPosition);
        SmoothClientPosition(DeltaTime);
    }
    else
    {
        UE_LOG(LogVPCNetSmoothing, Verbose, TEXT("Skipping network smoothing for %s."), *GetNameSafe(CharacterOwner));
    }
}

void UVPCMovementComponent::SimulateMovement(float DeltaTime)
{
    if (!HasValidData() || UpdatedComponent->Mobility != EComponentMobility::Movable || UpdatedComponent->IsSimulatingPhysics())
    {
        return;
    }

    const bool bIsSimulatedProxy = (CharacterOwner->Role == ROLE_SimulatedProxy);

    // Workaround for replication not being updated initially
    if (bIsSimulatedProxy &&
        CharacterOwner->ReplicatedMovement.Location.IsZero() &&
        CharacterOwner->ReplicatedMovement.Rotation.IsZero() &&
        CharacterOwner->ReplicatedMovement.LinearVelocity.IsZero())
    {
        return;
    }

    // If base is not resolved on the client, we should not try to simulate at all
    if (CharacterOwner->GetReplicatedBasedMovement().IsBaseUnresolved())
    {
        UE_LOG(LogVPCMovement, Verbose,
            TEXT("Base for simulated character '%s' is not resolved on client, skipping SimulateMovement"),
            *CharacterOwner->GetName());
        return;
    }

    FVector OldVelocity;
    FVector OldLocation;

    // Scoped updates can improve performance of multiple MoveComponent calls.
    {
        FScopedMovementUpdate ScopedMovementUpdate(UpdatedComponent, bEnableScopedMovementUpdates ? EScopedUpdate::DeferredUpdates : EScopedUpdate::ImmediateUpdates);

        if (bIsSimulatedProxy)
        {
            // Handle network changes
            if (bNetworkUpdateReceived)
            {
                bNetworkUpdateReceived = false;
                if (bNetworkMovementModeChanged)
                {
                    bNetworkMovementModeChanged = false;
                    ApplyNetworkMovementMode(CharacterOwner->GetReplicatedMovementMode());
                }
                else if (bJustTeleported)
                {
                    bJustTeleported = false;
                }
            }
        }

        if (MovementMode == MOVE_None)
        {
            ClearAccumulatedForces();
            return;
        }

        //TODO: Also ApplyAccumulatedForces()?
        ClearAccumulatedForces();

        Acceleration = Velocity.GetSafeNormal();    // Not currently used for simulated movement
        AnalogInputModifier = 1.0f;                 // Not currently used for simulated movement

        MaybeUpdateBasedMovement(DeltaTime);

        // Simulated pawns predict location
        OldVelocity = Velocity;
        OldLocation = UpdatedComponent->GetComponentLocation();
        MoveSmooth(Velocity, DeltaTime);

        OnMovementUpdated(DeltaTime, OldLocation, OldVelocity);
    } // End scoped movement update

    // Call custom post-movement events. These happen after the scoped movement completes in case the events want to use the current state of overlaps etc.
    CallMovementUpdateDelegate(DeltaTime, OldLocation, OldVelocity);

    MaybeSaveBaseLocation();
    UpdateComponentVelocity();
    bJustTeleported = false;

    LastUpdateLocation = UpdatedComponent ? UpdatedComponent->GetComponentLocation() : FVector::ZeroVector;
    LastUpdateRotation = UpdatedComponent ? UpdatedComponent->GetComponentQuat() : FQuat::Identity;
    LastUpdateVelocity = Velocity;
}

UPrimitiveComponent* UVPCMovementComponent::GetMovementBase() const
{
    return CharacterOwner ? CharacterOwner->GetMovementBase() : NULL;
}

void UVPCMovementComponent::SetBase(UPrimitiveComponent* NewBase, FName BoneName, bool bNotifyActor)
{
    // prevent from changing Base while server is NavWalking (no Base in that mode), so both sides are in sync
    // otherwise it will cause problems with position smoothing

    if (CharacterOwner)
    {
        CharacterOwner->SetBase(NewBase, NewBase ? BoneName : NAME_None, bNotifyActor);
    }
}

void UVPCMovementComponent::MaybeUpdateBasedMovement(float DeltaTime)
{
    bDeferUpdateBasedMovement = false;

    UPrimitiveComponent* MovementBase = CharacterOwner->GetMovementBase();

    if (MovementBaseUtility::UseRelativeLocation(MovementBase))
    {
        const bool bBaseIsSimulatingPhysics = MovementBase->IsSimulatingPhysics();
        
        // Temporarily disabling deferred tick on skeletal mesh components that sim physics.
        // We need to be consistent on when we read the bone locations for those, and while this reads
        // the wrong location, the relative changes (which is what we care about) will be accurate.
        const bool bAllowDefer = (bBaseIsSimulatingPhysics && !Cast<USkeletalMeshComponent>(MovementBase));
        
        if (! bBaseIsSimulatingPhysics || ! bAllowDefer)
        {
            bDeferUpdateBasedMovement = false;
            UpdateBasedMovement(DeltaTime);
            // If previously simulated, go back to using normal tick dependencies.
            if (PostPhysicsTickFunction.IsTickFunctionEnabled())
            {
                PostPhysicsTickFunction.SetTickFunctionEnable(false);
                MovementBaseUtility::AddTickDependency(PrimaryComponentTick, MovementBase);
            }
        }
        else
        {
            // defer movement base update until after physics
            bDeferUpdateBasedMovement = true;
            // If previously not simulating, remove tick dependencies and use post physics tick function.
            if (!PostPhysicsTickFunction.IsTickFunctionEnabled())
            {
                PostPhysicsTickFunction.SetTickFunctionEnable(true);
                MovementBaseUtility::RemoveTickDependency(PrimaryComponentTick, MovementBase);
            }
        }
    }
}

void UVPCMovementComponent::MaybeSaveBaseLocation()
{
    if (!bDeferUpdateBasedMovement)
    {
        SaveBaseLocation();
    }
}

void UVPCMovementComponent::UpdateBasedMovement(float DeltaTime)
{
    if (!HasValidData())
    {
        return;
    }

    const UPrimitiveComponent* MovementBase = CharacterOwner->GetMovementBase();

    if (! MovementBaseUtility::UseRelativeLocation(MovementBase))
    {
        return;
    }

    if (! IsValid(MovementBase) || ! IsValid(MovementBase->GetOwner()))
    {
        SetBase(NULL);
        return;
    }

    // Ignore collision with bases during these movements.
    TGuardValue<EMoveComponentFlags> ScopedFlagRestore(MoveComponentFlags, MoveComponentFlags | MOVECOMP_IgnoreBases);

    FQuat DeltaQuat = FQuat::Identity;
    FVector DeltaPosition = FVector::ZeroVector;

    FQuat NewBaseQuat;
    FVector NewBaseLocation;

    if (! MovementBaseUtility::GetMovementBaseTransform(MovementBase, CharacterOwner->GetBasedMovement().BoneName, NewBaseLocation, NewBaseQuat))
    {
        return;
    }

    // Find change in rotation
    const bool bRotationChanged = ! OldBaseQuat.Equals(NewBaseQuat, 1e-8f);

    if (bRotationChanged)
    {
        DeltaQuat = NewBaseQuat * OldBaseQuat.Inverse();
    }

    // Only if base moved
    if (bRotationChanged || (OldBaseLocation != NewBaseLocation))
    {
        // Calculate new transform matrix of base actor (ignoring scale).
        const FQuatRotationTranslationMatrix OldLocalToWorld(OldBaseQuat, OldBaseLocation);
        const FQuatRotationTranslationMatrix NewLocalToWorld(NewBaseQuat, NewBaseLocation);

        if (CharacterOwner->IsMatineeControlled())
        {
            FRotationTranslationMatrix HardRelMatrix(CharacterOwner->GetBasedMovement().Rotation, CharacterOwner->GetBasedMovement().Location);
            const FMatrix NewWorldTM = HardRelMatrix * NewLocalToWorld;
            const FQuat NewWorldRot = bIgnoreBaseRotation ? UpdatedComponent->GetComponentQuat() : NewWorldTM.ToQuat();
            MoveUpdatedComponent(NewWorldTM.GetOrigin() - UpdatedComponent->GetComponentLocation(), NewWorldRot, true);
        }
        else
        {
            FQuat FinalQuat = UpdatedComponent->GetComponentQuat();
            
            if (bRotationChanged && ! bIgnoreBaseRotation)
            {
                // Apply change in rotation
                const FQuat TargetQuat = DeltaQuat * FinalQuat;
                MoveUpdatedComponent(FVector::ZeroVector, TargetQuat, false);
                FinalQuat = UpdatedComponent->GetComponentQuat();
            }

            FVector const LocalBasePos = OldLocalToWorld.InverseTransformPosition(UpdatedComponent->GetComponentLocation());
            FVector const NewWorldPos = ConstrainLocationToPlane(NewLocalToWorld.TransformPosition(LocalBasePos));
            DeltaPosition = ConstrainDirectionToPlane(NewWorldPos - UpdatedComponent->GetComponentLocation());

            // Move attached actor
            if (bFastAttachedMove)
            {
                // We're trusting no other obstacle can prevent the move here
                UpdatedComponent->SetWorldLocationAndRotation(NewWorldPos, FinalQuat, false);
            }
            else
            {
                // hack - transforms between local and world space introducing slight error
                // FIXMESTEVE - discuss with engine team: just skip the transforms if no rotation?
                FVector BaseMoveDelta = NewBaseLocation - OldBaseLocation;

                if (! bRotationChanged && (BaseMoveDelta.X == 0.f) && (BaseMoveDelta.Y == 0.f) && (BaseMoveDelta.Z == 0.f))
                {
                    DeltaPosition.X = 0.f;
                    DeltaPosition.Y = 0.f;
                    DeltaPosition.Z = 0.f;
                }

                FHitResult MoveOnBaseHit(1.f);
                const FVector OldLocation = UpdatedComponent->GetComponentLocation();
                MoveUpdatedComponent(DeltaPosition, FinalQuat, true, &MoveOnBaseHit);

                if ((UpdatedComponent->GetComponentLocation() - (OldLocation + DeltaPosition)).IsNearlyZero() == false)
                {
                    OnUnableToFollowBaseMove(DeltaPosition, OldLocation, MoveOnBaseHit);
                }
            }
        }

        if (MovementBase->IsSimulatingPhysics() && CharacterOwner->GetMesh())
        {
            CharacterOwner->GetMesh()->ApplyDeltaToAllPhysicsTransforms(DeltaPosition, DeltaQuat);
        }
    }
}

void UVPCMovementComponent::OnUnableToFollowBaseMove(const FVector& DeltaPosition, const FVector& OldLocation, const FHitResult& MoveOnBaseHit)
{
    // no default implementation, left for subclasses to override.
}

void UVPCMovementComponent::DisableMovement()
{
    if (CharacterOwner)
    {
        SetMovementMode(MOVE_None);
    }
    else
    {
        MovementMode = MOVE_None;
        CustomMovementMode = 0;
    }
}

void UVPCMovementComponent::PerformMovement(float DeltaTime)
{
    SCOPE_CYCLE_COUNTER(STAT_VPCMovementPerformMovement);

    if (! HasValidData())
    {
        return;
    }
    
    // No movement if we can't move, or if currently doing physical simulation on UpdatedComponent
    if (MovementMode == MOVE_None || UpdatedComponent->Mobility != EComponentMobility::Movable || UpdatedComponent->IsSimulatingPhysics())
    {
        // Clear pending physics forces
        ClearAccumulatedForces();
        return;
    }

    FVector OldVelocity;
    FVector OldLocation;

    // Scoped updates can improve performance of multiple MoveComponent calls.
    {
        FScopedMovementUpdate ScopedMovementUpdate(UpdatedComponent, bEnableScopedMovementUpdates ? EScopedUpdate::DeferredUpdates : EScopedUpdate::ImmediateUpdates);

        MaybeUpdateBasedMovement(DeltaTime);

        OldVelocity = Velocity;
        OldLocation = UpdatedComponent->GetComponentLocation();

        ApplyAccumulatedForces(DeltaTime);

        // Update the character state before we do our movement
        UpdateCharacterStateBeforeMovement();

        ClearAccumulatedForces();

        // NaN tracking
        checkCode(ensureMsgf(!Velocity.ContainsNaN(),
            TEXT("UVPCMovementComponent::PerformMovement: Velocity contains NaN (%s)\n%s"),
            *GetPathNameSafe(this), *Velocity.ToString()));

        // Update orientation
        if (!CharacterOwner->IsMatineeControlled() && !bLockedOrientation)
        {
            PhysicsOrientation(DeltaTime);
        }

        // Update movement
        StartNewPhysics(DeltaTime);

        if (!HasValidData())
        {
            return;
        }

        // Update character state based on change from movement
        UpdateCharacterStateAfterMovement();

        if (!CharacterOwner->IsMatineeControlled())
        {
            PhysicsRotation(DeltaTime);
        }

        OnMovementUpdated(DeltaTime, OldLocation, OldVelocity);
    } // End scoped movement update

    // Call external post-movement events. These happen after the scoped movement completes in case the events want to use the current state of overlaps etc.
    CallMovementUpdateDelegate(DeltaTime, OldLocation, OldVelocity);

    MaybeSaveBaseLocation();
    UpdateComponentVelocity();

    const bool bHasAuthority = CharacterOwner && CharacterOwner->HasAuthority();

    // If we move we want to avoid a long delay before replication catches up to notice this change, especially if it's throttling our rate.
    if (bHasAuthority && UNetDriver::IsAdaptiveNetUpdateFrequencyEnabled() && IsCancelAdaptiveReplicationEnabled() && UpdatedComponent)
    {
        const UWorld* MyWorld = GetWorld();
        if (MyWorld)
        {
            UNetDriver* NetDriver = MyWorld->GetNetDriver();
            if (NetDriver && NetDriver->IsServer())
            {
                FNetworkObjectInfo* NetActor = NetDriver->GetNetworkObjectInfo(CharacterOwner);
                if (NetActor && MyWorld->GetTimeSeconds() <= NetActor->NextUpdateTime && NetDriver->IsNetworkActorUpdateFrequencyThrottled(*NetActor))
                {
                    if (ShouldCancelAdaptiveReplication())
                    {
                        NetDriver->CancelAdaptiveReplication(*NetActor);
                    }
                }
            }
        }
    }

    const FVector NewLocation = UpdatedComponent ? UpdatedComponent->GetComponentLocation() : FVector::ZeroVector;
    const FQuat NewRotation = UpdatedComponent ? UpdatedComponent->GetComponentQuat() : FQuat::Identity;

    if (bHasAuthority && UpdatedComponent && !IsNetMode(NM_Client))
    {
        const bool bLocationChanged = (NewLocation != LastUpdateLocation);
        const bool bRotationChanged = (NewRotation != LastUpdateRotation);

        if (bLocationChanged || bRotationChanged)
        {
            const UWorld* MyWorld = GetWorld();
            ServerLastTransformUpdateTimeStamp = MyWorld ? MyWorld->GetTimeSeconds() : 0.f;
        }
    }

    LastUpdateLocation = NewLocation;
    LastUpdateRotation = NewRotation;
    LastUpdateVelocity = Velocity;
}

bool UVPCMovementComponent::ShouldCancelAdaptiveReplication() const
{
    // Update sooner if important properties changed.
    const bool bVelocityChanged = (Velocity != LastUpdateVelocity);
    const bool bLocationChanged = (UpdatedComponent->GetComponentLocation() != LastUpdateLocation);
    const bool bRotationChanged = (UpdatedComponent->GetComponentQuat() != LastUpdateRotation);

    return (bVelocityChanged || bLocationChanged || bRotationChanged);
}

bool UVPCMovementComponent::IsCancelAdaptiveReplicationEnabled() const
{
    return bCancelAdaptiveReplicationEnabled;
}

void UVPCMovementComponent::CallMovementUpdateDelegate(float DeltaTime, const FVector& OldLocation, const FVector& OldVelocity)
{
    SCOPE_CYCLE_COUNTER(STAT_VPCMoveUpdateDelegate);

    // Update component velocity in case events want to read it
    UpdateComponentVelocity();

    // Delegate (for blueprints)
    if (CharacterOwner)
    {
        CharacterOwner->OnCharacterMovementUpdated.Broadcast(DeltaTime, OldLocation, OldVelocity);
    }
}

void UVPCMovementComponent::OnMovementUpdated(float DeltaTime, const FVector& OldLocation, const FVector& OldVelocity)
{
    // empty base implementation, intended for derived classes to override.
}

void UVPCMovementComponent::SaveBaseLocation()
{
    if (!HasValidData())
    {
        return;
    }

    const UPrimitiveComponent* MovementBase = CharacterOwner->GetMovementBase();

    if (MovementBaseUtility::UseRelativeLocation(MovementBase) && !CharacterOwner->IsMatineeControlled())
    {
        // Read transforms into OldBaseLocation, OldBaseQuat
        MovementBaseUtility::GetMovementBaseTransform(MovementBase, CharacterOwner->GetBasedMovement().BoneName, OldBaseLocation, OldBaseQuat);

        // Location
        const FVector RelativeLocation = UpdatedComponent->GetComponentLocation() - OldBaseLocation;

        // Rotation
        if (bIgnoreBaseRotation)
        {
            // Absolute rotation
            CharacterOwner->SaveRelativeBasedMovement(RelativeLocation, UpdatedComponent->GetComponentRotation(), false);
        }
        else
        {
            // Relative rotation
            const FRotator RelativeRotation = (FQuatRotationMatrix(UpdatedComponent->GetComponentQuat()) * FQuatRotationMatrix(OldBaseQuat).GetTransposed()).Rotator();
            CharacterOwner->SaveRelativeBasedMovement(RelativeLocation, RelativeRotation, true);
        }
    }
}

void UVPCMovementComponent::UpdateCharacterStateBeforeMovement()
{
    // Blank implementation
}

void UVPCMovementComponent::UpdateCharacterStateAfterMovement()
{
    // Blank implementation
}

void UVPCMovementComponent::StartNewPhysics(float DeltaTime)
{
    if ((DeltaTime < MIN_TICK_TIME) || !HasValidData())
    {
        return;
    }

    if (UpdatedComponent->IsSimulatingPhysics())
    {
        UE_LOG(LogVPCMovement, Log,
            TEXT("UVPCMovementComponent::StartNewPhysics: UpdateComponent (%s) is simulating physics - aborting."),
            *UpdatedComponent->GetPathName());
        return;
    }

    const bool bSavedMovementInProgress = bMovementInProgress;
    bMovementInProgress = true;

    switch (MovementMode)
    {
    case MOVE_None:
        break;
    case MOVE_Flying:
        PhysFlying(DeltaTime);
        break;
    case MOVE_Custom:
        PhysCustom(DeltaTime);
        break;
    default:
        UE_LOG(LogVPCMovement, Warning, TEXT("%s has unsupported movement mode %d"), *CharacterOwner->GetName(), int32(MovementMode));
        SetMovementMode(MOVE_None);
        break;
    }

    bMovementInProgress = bSavedMovementInProgress;

    if (bDeferUpdateMoveComponent)
    {
        SetUpdatedComponent(DeferredUpdatedMoveComponent);
    }
}

float UVPCMovementComponent::GetGravityZ() const
{
    return 0.f;
}

float UVPCMovementComponent::GetMaxSpeed() const
{
    switch (MovementMode)
    {
    case MOVE_Flying:
        return !bLockedOrientation ? MaxSpeed : MaxLockedOrientationSpeed;
    case MOVE_Custom:
        return MaxCustomMovementSpeed;
    case MOVE_None:
    default:
        return 0.f;
    }
}

float UVPCMovementComponent::GetMinAnalogSpeed() const
{
    return 0.f;
}

FVector UVPCMovementComponent::GetPenetrationAdjustment(const FHitResult& Hit) const
{
    FVector Result = Super::GetPenetrationAdjustment(Hit);

    if (CharacterOwner)
    {
        const bool bIsProxy = (CharacterOwner->Role == ROLE_SimulatedProxy);
        float MaxDistance = bIsProxy ? MaxDepenetrationWithGeometryAsProxy : MaxDepenetrationWithGeometry;
        const AActor* HitActor = Hit.GetActor();

        if (Cast<APawn>(HitActor) || Cast<AVPawn>(HitActor))
        {
            MaxDistance = bIsProxy ? MaxDepenetrationWithPawnAsProxy : MaxDepenetrationWithPawn;
        }

        Result = Result.GetClampedToMaxSize(MaxDistance);
    }

    return Result;
}

bool UVPCMovementComponent::ResolvePenetrationImpl(const FVector& Adjustment, const FHitResult& Hit, const FQuat& NewRotation)
{
    // If movement occurs, mark that we teleported, so we don't incorrectly adjust velocity based on a potentially very different movement than our movement direction.
    bJustTeleported |= Super::ResolvePenetrationImpl(Adjustment, Hit, NewRotation);
    return bJustTeleported;
}

float UVPCMovementComponent::SlideAlongSurface(const FVector& Delta, float Time, const FVector& InNormal, FHitResult& Hit, bool bHandleImpact)
{
    if (! Hit.bBlockingHit)
    {
        return 0.f;
    }

    return Super::SlideAlongSurface(Delta, Time, InNormal, Hit, bHandleImpact);
}

void UVPCMovementComponent::CalcVelocity(float DeltaTime, float Friction, bool bVolumeFrictionEnabled, float BrakingDeceleration)
{
    // Do not update velocity when using root motion or when SimulatedProxy - SimulatedProxy are repped their Velocity
    if (!HasValidData() || DeltaTime < MIN_TICK_TIME || (CharacterOwner && CharacterOwner->Role == ROLE_SimulatedProxy))
    {
        return;
    }

    const float MaxAccel = GetMaxAcceleration();
    float MaxSpeed = GetMaxSpeed();

    // Use velocity orientation unless orientation lock is enabled in which acceleration direction is used instead
    FVector VelDir = (!bLockedOrientation || Acceleration.IsZero()) ? VelocityOrientation.GetForwardVector() : Acceleration.GetSafeNormal();

    // Override velocity duration if performing zero velocity rvo
    if (bUseRVOAvoidance && bZeroVelocityRVO && RVOAgentComponent)
    {
        VelDir = RVOAgentComponent->GetAvoidanceVelocity().GetSafeNormal();
    }

    // If zero friction unless fluid (volume) friction is enabled
    Friction = bVolumeFrictionEnabled ? FMath::Max(0.f, Friction) : 0.f;

    // Force maximum acceleration if required
    if (bForceMaxAccel)
    {
        Acceleration = VelDir * MaxAccel;
        AnalogInputModifier = 1.f;
    }

    // Use max of requested speed and max speed if we modified the speed.
    MaxSpeed = FMath::Max(MaxSpeed*AnalogInputModifier, GetMinAnalogSpeed());

    // Velocity limit flags
    const bool bZeroAcceleration = Acceleration.IsZero() || FMath::IsNearlyZero(AnalogInputModifier);
    const bool bVelocityOverMax = IsExceedingMaxSpeed(MaxSpeed);
    
    // Only apply braking if there is no acceleration, or we are over our max speed and need to slow down to it.
    if (bZeroAcceleration || bVelocityOverMax)
    {
        const float DecelerationMagnitude = bZeroAcceleration ? GetMaxZeroInputDeceleration() : BrakingDeceleration;
        const float ActualBrakingFriction = bUseSeparateBrakingFriction ? BrakingFriction : Friction;

        ApplyVelocityBraking(DeltaTime, ActualBrakingFriction, DecelerationMagnitude);
    
        // Don't allow braking to lower us below max speed if we started above it.
        if (bVelocityOverMax && Velocity.SizeSquared() < FMath::Square(MaxSpeed))
        {
            Velocity = VelDir * MaxSpeed;
        }
    }
    // Apply velocity direction
    else if (!bZeroAcceleration)
    {
        Velocity = VelDir * Velocity.Size();
    }

    // Apply volume friction if enabled
    if (bVolumeFrictionEnabled)
    {
        Velocity = Velocity * (1.f - FMath::Min(Friction * DeltaTime, 1.f));
    }

    // Apply acceleration
    const float NewMaxSpeed = IsExceedingMaxSpeed(MaxSpeed) ? Velocity.Size() : MaxSpeed;
    Velocity += Acceleration * DeltaTime;
    Velocity = Velocity.GetClampedToMaxSize(NewMaxSpeed);

    // Clamp to zero velocity rvo max speed if required
    if (bUseRVOAvoidance && bZeroVelocityRVO)
    {
        Velocity = Velocity.GetClampedToMaxSize(ZeroVelocityRVOMaxSpeed);
    }
}

void UVPCMovementComponent::SetRVOAgentComponent(URVO3DAgentComponent* InRVOAgentComponent)
{
    // Don't assign pending kill components, but allow those to null out previous value
    RVOAgentComponent = IsValid(InRVOAgentComponent) ? InRVOAgentComponent : nullptr;
}

void UVPCMovementComponent::CalcAvoidanceVelocity()
{
    check(HasValidData());

    // Make sure avoidance is enabled with rvo agent component available and run on authoritative role
    if (! bUseRVOAvoidance || ! RVOAgentComponent || CharacterOwner->Role != ROLE_Authority)
    {
        return;
    }

    const FVector InputControl = GetLastInputVector();

    // Disable zero velocity rvo if rvo velocity override has been unlocked
    if (bZeroVelocityRVO && ! RVOAgentComponent->HasLockedPreferredVelocity())
    {
        bZeroVelocityRVO = false;
    }

    // Perform RVO calculation if currently moving
    if (InputControl.SizeSquared() > KINDA_SMALL_NUMBER && Velocity.SizeSquared() > KINDA_SMALL_NUMBER)
    {
        // Calculate RVO if not already performing locked avoidance
        if (! RVOAgentComponent->HasLockedPreferredVelocity())
        {
            if (RVOAgentComponent->HasAvoidanceVelocity())
            {
                // Whether current RVO performed with minimal velocity
                bZeroVelocityRVO = Velocity.SizeSquared() < FMath::Square(ZeroVelocityRVOThreshold);

                // Had to divert course, lock this avoidance move in for a short time.
                if (bZeroVelocityRVO)
                {
                    RVOAgentComponent->LockPreferredVelocity(InputControl * GetMaxSpeed(), ZeroVelocityRVOLockDuration);
                }
                else
                {
                    RVOAgentComponent->LockPreferredVelocity(InputControl * GetMaxSpeed());
                }
            }
        }
    }
}

void UVPCMovementComponent::PostProcessAvoidanceVelocity(FVector& NewVelocity)
{
    // Blank implementation
}

void UVPCMovementComponent::NotifyBumpedPawn(APawn* BumpedPawn)
{
    Super::NotifyBumpedPawn(BumpedPawn);

    // Blank implementation
}

void UVPCMovementComponent::NotifyBumpedPawn(AVPawn* BumpedPawn)
{
    Super::NotifyBumpedPawn(BumpedPawn);

    // Blank implementation
}

float UVPCMovementComponent::GetMaxAcceleration() const
{
    return MaxAcceleration;
}

float UVPCMovementComponent::GetMaxDeceleration() const
{
    switch (MovementMode)
    {
        case MOVE_Flying:
            return MaxDeceleration;
        case MOVE_None:
        default:
            return 0.f;
    }
}

float UVPCMovementComponent::GetMaxZeroInputDeceleration() const
{
    switch (MovementMode)
    {
        case MOVE_Flying:
            return MaxZeroInputDeceleration;
        case MOVE_None:
        default:
            return 0.f;
    }
}

FVector UVPCMovementComponent::GetCurrentAcceleration() const
{
    return Acceleration;
}

void UVPCMovementComponent::ApplyVelocityBraking(float DeltaTime, float Friction, float BrakingDeceleration)
{
    if (DeltaTime < MIN_TICK_TIME || Velocity.IsZero() || !HasValidData())
    {
        return;
    }

    const float FrictionFactor = FMath::Max(0.f, BrakingFrictionFactor);
    Friction = FMath::Max(0.f, Friction * FrictionFactor);
    BrakingDeceleration = FMath::Max(0.f, BrakingDeceleration);

    const bool bZeroFriction = (Friction == 0.f);
    const bool bZeroBraking = (BrakingDeceleration == 0.f);

    if (bZeroFriction && bZeroBraking)
    {
        return;
    }

    const FVector OldVel = Velocity;

    // Subdivide braking to get reasonably consistent results at lower frame rates
    // (important for packet loss situations w/ networking)
    float RemainingTime = DeltaTime;
    const float MaxTimeStep = .03f; // (1.0f / 33.0f);

    // Calculate braking deceleration
    const FVector RevAccel = (bZeroBraking ? FVector::ZeroVector : (-BrakingDeceleration * Velocity.GetSafeNormal()));

    // Decelerate iteration
    while (RemainingTime >= MIN_TICK_TIME)
    {
        // Zero friction uses constant deceleration, so no need for iteration.
        const float dt = ((RemainingTime > MaxTimeStep && !bZeroFriction) ? FMath::Min(MaxTimeStep, RemainingTime * 0.5f) : RemainingTime);
        RemainingTime -= dt;

        // Apply friction and braking
        Velocity = Velocity + ((-Friction) * Velocity + RevAccel) * dt;
        
        // Don't reverse direction
        if ((Velocity | OldVel) <= 0.f)
        {
            Velocity = FVector::ZeroVector;
            return;
        }
    }

    // Clamp to zero if nearly zero, or if below min threshold and braking.
    const float VSizeSq = Velocity.SizeSquared();
    if (VSizeSq <= KINDA_SMALL_NUMBER || (!bZeroBraking && VSizeSq <= FMath::Square(BRAKE_TO_STOP_VELOCITY)))
    {
        Velocity = FVector::ZeroVector;
    }
}

void UVPCMovementComponent::PhysFlying(float DeltaTime)
{
    if (DeltaTime < MIN_TICK_TIME)
    {
        return;
    }

    // Calculate current velocity
    {
        const float Friction = 0.5f * GetPhysicsVolume()->FluidFriction;
        CalcVelocity(DeltaTime, Friction, bEnableVolumeFriction, GetMaxDeceleration());
    }

    bJustTeleported = false;

    FVector OldLocation = UpdatedComponent->GetComponentLocation();
    const FVector Adjusted = Velocity * DeltaTime;
    FHitResult Hit(1.f);
    SafeMoveUpdatedComponent(Adjusted, UpdatedComponent->GetComponentQuat(), true, Hit);

    if (Hit.Time < 1.f)
    {
        // Adjust and try again
        HandleImpact(Hit, DeltaTime, Adjusted);
        SlideAlongSurface(Adjusted, (1.f - Hit.Time), Hit.Normal, Hit, true);
    }

    if (!bJustTeleported)
    {
        Velocity = (UpdatedComponent->GetComponentLocation() - OldLocation) / DeltaTime;
    }
}

void UVPCMovementComponent::OnCharacterStuckInGeometry(const FHitResult* Hit)
{
    if (VPCMovementCVars::StuckWarningPeriod >= 0)
    {
        UWorld* MyWorld = GetWorld();
        const float RealTimeSeconds = MyWorld->GetRealTimeSeconds();
        if ((RealTimeSeconds - LastStuckWarningTime) >= VPCMovementCVars::StuckWarningPeriod)
        {
            LastStuckWarningTime = RealTimeSeconds;
            if (Hit == nullptr)
            {
                UE_LOG(LogVPCMovement, Log, TEXT("%s is stuck and failed to move! (%d other events since notify)"), *CharacterOwner->GetName(), StuckWarningCountSinceNotify);
            }
            else
            {
                UE_LOG(LogVPCMovement, Log, TEXT("%s is stuck and failed to move! Velocity: X=%3.2f Y=%3.2f Z=%3.2f Location: X=%3.2f Y=%3.2f Z=%3.2f Normal: X=%3.2f Y=%3.2f Z=%3.2f PenetrationDepth:%.3f Actor:%s Component:%s BoneName:%s (%d other events since notify)"),
                       *GetNameSafe(CharacterOwner),
                       Velocity.X, Velocity.Y, Velocity.Z,
                       Hit->Location.X, Hit->Location.Y, Hit->Location.Z,
                       Hit->Normal.X, Hit->Normal.Y, Hit->Normal.Z,
                       Hit->PenetrationDepth,
                       *GetNameSafe(Hit->GetActor()),
                       *GetNameSafe(Hit->GetComponent()),
                       Hit->BoneName.IsValid() ? *Hit->BoneName.ToString() : TEXT("None"),
                       StuckWarningCountSinceNotify
                       );
            }
            StuckWarningCountSinceNotify = 0;
        }
        else
        {
            StuckWarningCountSinceNotify += 1;
        }
    }

    // Don't update velocity based on our (failed) change in position this update since we're stuck.
    bJustTeleported = true;
}

void UVPCMovementComponent::PhysCustom(float DeltaTime)
{
    if (CharacterOwner)
    {
        CharacterOwner->K2_UpdateCustomMovement(DeltaTime);
    }
}

void UVPCMovementComponent::OnTeleported()
{
    if (!HasValidData())
    {
        return;
    }

    bJustTeleported = true;
    MaybeSaveBaseLocation();
}

FRotator UVPCMovementComponent::GetDeltaRotation(float DeltaTime) const
{
    return FRotator(
        GetAxisDeltaRotation(RotationRate.Pitch, DeltaTime),
        GetAxisDeltaRotation(RotationRate.Yaw, DeltaTime),
        GetAxisDeltaRotation(RotationRate.Roll, DeltaTime)
    );
}

FRotator UVPCMovementComponent::ComputeOrientToMovementRotation(const FRotator& CurrentRotation, float DeltaTime, FRotator& DeltaRotation) const
{
    if (Acceleration.SizeSquared() < KINDA_SMALL_NUMBER)
    {
        // Don't change rotation if there is no acceleration.
        return CurrentRotation;
    }

    // Rotate toward direction of acceleration.
    return Acceleration.GetSafeNormal().Rotation();
}

bool UVPCMovementComponent::HasUnlockedRotation() const
{
    return true;
}

void UVPCMovementComponent::PhysicsRotation(float DeltaTime)
{
    if (! bOrientRotationToMovement)
    {
        return;
    }

    if (!HasValidData())
    {
        return;
    }

    MoveUpdatedComponent(FVector::ZeroVector, VelocityOrientation, true);
    return;

    FRotator CurrentRotation = UpdatedComponent->GetComponentRotation(); // Normalized
    CurrentRotation.DiagnosticCheckNaN(TEXT("CharacterMovementComponent::PhysicsRotation(): CurrentRotation"));

    FRotator DeltaRot = GetDeltaRotation(DeltaTime);
    DeltaRot.DiagnosticCheckNaN(TEXT("CharacterMovementComponent::PhysicsRotation(): GetDeltaRotation"));

    FRotator DesiredRotation = ComputeOrientToMovementRotation(CurrentRotation, DeltaTime, DeltaRot);

    if (HasUnlockedRotation())
    {
        DesiredRotation.Normalize();
    }
    else
    {
        DesiredRotation.Pitch = 0.f;
        DesiredRotation.Yaw = FRotator::NormalizeAxis(DesiredRotation.Yaw);
        DesiredRotation.Roll = 0.f;
    }
    
    // Accumulate a desired new rotation.
    const float AngleTolerance = 1e-3f;

    if (!CurrentRotation.Equals(DesiredRotation, AngleTolerance))
    {
        // PITCH
        if (!FMath::IsNearlyEqual(CurrentRotation.Pitch, DesiredRotation.Pitch, AngleTolerance))
        {
            DesiredRotation.Pitch = FMath::FixedTurn(CurrentRotation.Pitch, DesiredRotation.Pitch, DeltaRot.Pitch);
        }

        // YAW
        if (!FMath::IsNearlyEqual(CurrentRotation.Yaw, DesiredRotation.Yaw, AngleTolerance))
        {
            DesiredRotation.Yaw = FMath::FixedTurn(CurrentRotation.Yaw, DesiredRotation.Yaw, DeltaRot.Yaw);
        }

        // ROLL
        if (!FMath::IsNearlyEqual(CurrentRotation.Roll, DesiredRotation.Roll, AngleTolerance))
        {
            DesiredRotation.Roll = FMath::FixedTurn(CurrentRotation.Roll, DesiredRotation.Roll, DeltaRot.Roll);
        }

        // Set the new rotation.
        DesiredRotation.DiagnosticCheckNaN(TEXT("CharacterMovementComponent::PhysicsRotation(): DesiredRotation"));
        MoveUpdatedComponent(FVector::ZeroVector, DesiredRotation, true);
    }
}

void UVPCMovementComponent::AddImpulse(FVector Impulse, bool bVelocityChange)
{
    if (!Impulse.IsZero() && (MovementMode != MOVE_None) && IsActive() && HasValidData())
    {
        // Handle scaling by mass
        FVector FinalImpulse = Impulse;
        if (!bVelocityChange)
        {
            if (Mass > SMALL_NUMBER)
            {
                FinalImpulse = FinalImpulse / Mass;
            }
            else
            {
                UE_LOG(LogVPCMovement, Warning, TEXT("Attempt to apply impulse to zero or negative Mass in CharacterMovement"));
            }
        }

        PendingImpulseToApply += FinalImpulse;
    }
}

void UVPCMovementComponent::AddForce(FVector Force)
{
    if (!Force.IsZero() && (MovementMode != MOVE_None) && IsActive() && HasValidData())
    {
        if (Mass > SMALL_NUMBER)
        {
            PendingForceToApply += Force / Mass;
        }
        else
        {
            UE_LOG(LogVPCMovement, Warning, TEXT("Attempt to apply force to zero or negative Mass in CharacterMovement"));
        }
    }
}

void UVPCMovementComponent::MoveSmooth(const FVector& InVelocity, const float DeltaTime)
{
    if (!HasValidData())
    {
        return;
    }

    // Custom movement mode.
    // Custom movement may need an update even if there is zero velocity.
    if (MovementMode == MOVE_Custom)
    {
        FScopedMovementUpdate ScopedMovementUpdate(UpdatedComponent, bEnableScopedMovementUpdates ? EScopedUpdate::DeferredUpdates : EScopedUpdate::ImmediateUpdates);
        PhysCustom(DeltaTime);
        return;
    }

    FVector Delta = InVelocity * DeltaTime;
    if (Delta.IsZero())
    {
        return;
    }

    FScopedMovementUpdate ScopedMovementUpdate(UpdatedComponent, bEnableScopedMovementUpdates ? EScopedUpdate::DeferredUpdates : EScopedUpdate::ImmediateUpdates);

    FHitResult Hit(1.f);
    SafeMoveUpdatedComponent(Delta, UpdatedComponent->GetComponentQuat(), true, Hit);

    if (Hit.IsValidBlockingHit())
    {
        SlideAlongSurface(Delta, 1.f - Hit.Time, Hit.Normal, Hit, false);
    }
}

void UVPCMovementComponent::HandleImpact(const FHitResult& Impact, float TimeSlice, const FVector& MoveDelta)
{
    if (CharacterOwner)
    {
        CharacterOwner->MoveBlockedBy(Impact);
    }

    AActor* ImpactActor = Impact.GetActor();

    if (APawn* OtherPawn = Cast<APawn>(ImpactActor))
    {
        NotifyBumpedPawn(OtherPawn);
    }
    else if (AVPawn* OtherPawn = Cast<AVPawn>(ImpactActor))
    {
        NotifyBumpedPawn(OtherPawn);
    }

    if (bEnablePhysicsInteraction)
    {
        const FVector ForceAccel = Acceleration;
        ApplyImpactPhysicsForces(Impact, ForceAccel, Velocity);
    }
}

void UVPCMovementComponent::ApplyImpactPhysicsForces(const FHitResult& Impact, const FVector& ImpactAcceleration, const FVector& ImpactVelocity)
{
    if (bEnablePhysicsInteraction && Impact.bBlockingHit)
    {
        if (UPrimitiveComponent* ImpactComponent = Impact.GetComponent())
        {
            FBodyInstance* BI = ImpactComponent->GetBodyInstance(Impact.BoneName);
            if (BI != nullptr && BI->IsInstanceSimulatingPhysics())
            {
                FVector ForcePoint = Impact.ImpactPoint;

                const float BodyMass = FMath::Max(BI->GetBodyMass(), 1.0f);

                FVector Force = Impact.ImpactNormal * -1.0f;

                float PushForceModificator = 1.0f;

                const FVector ComponentVelocity = ImpactComponent->GetPhysicsLinearVelocity();
                const FVector VirtualVelocity = ImpactAcceleration.IsZero() ? ImpactVelocity : ImpactAcceleration.GetSafeNormal() * GetMaxSpeed();

                float Dot = 0.0f;

                if (bScalePushForceToVelocity && !ComponentVelocity.IsNearlyZero())
                {           
                    Dot = ComponentVelocity | VirtualVelocity;

                    if (Dot > 0.0f && Dot < 1.0f)
                    {
                        PushForceModificator *= Dot;
                    }
                }

                if (bPushForceScaledToMass)
                {
                    PushForceModificator *= BodyMass;
                }

                Force *= PushForceModificator;

                if (ComponentVelocity.IsNearlyZero())
                {
                    Force *= InitialPushForceFactor;
                    ImpactComponent->AddImpulseAtLocation(Force, ForcePoint, Impact.BoneName);
                }
                else
                {
                    Force *= PushForceFactor;
                    ImpactComponent->AddForceAtLocation(Force, ForcePoint, Impact.BoneName);
                }
            }
        }
    }
}

FString UVPCMovementComponent::GetMovementName() const
{
    if( CharacterOwner )
    {
        if (CharacterOwner->GetRootComponent() && CharacterOwner->GetRootComponent()->IsSimulatingPhysics())
        {
            return TEXT("Rigid Body");
        }
        else if (CharacterOwner->IsMatineeControlled())
        {
            return TEXT("Matinee");
        }
    }

    // Using character movement
    switch( MovementMode )
    {
        case MOVE_None:             return TEXT("NULL"); break;
        case MOVE_Flying:           return TEXT("Flying"); break;
        case MOVE_Custom:           return TEXT("Custom"); break;
        default:                    break;
    }
    return TEXT("Unknown");
}

void UVPCMovementComponent::DisplayDebug(UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos)
{
    if (CharacterOwner == NULL)
    {
        return;
    }

    FDisplayDebugManager& DisplayDebugManager = Canvas->DisplayDebugManager;
    DisplayDebugManager.SetDrawColor(FColor::White);
    FString T;

    T = FString::Printf(TEXT("Updated Component: %s"), *UpdatedComponent->GetName());
    DisplayDebugManager.DrawString(T);

    T = FString::Printf(TEXT("Acceleration: %s"), *Acceleration.ToCompactString());
    DisplayDebugManager.DrawString(T);

    T = FString::Printf(TEXT("bForceMaxAccel: %i"), bForceMaxAccel);
    DisplayDebugManager.DrawString(T);

    APhysicsVolume * PhysicsVolume = GetPhysicsVolume();

    const UPrimitiveComponent* BaseComponent = CharacterOwner->GetMovementBase();
    const AActor* BaseActor = BaseComponent ? BaseComponent->GetOwner() : NULL;

    T = FString::Printf(TEXT("%s In physicsvolume %s on base %s component %s gravity %f"), *GetMovementName(), (PhysicsVolume ? *PhysicsVolume->GetName() : TEXT("None")),
        (BaseActor ? *BaseActor->GetName() : TEXT("None")), (BaseComponent ? *BaseComponent->GetName() : TEXT("None")), GetGravityZ());
    DisplayDebugManager.DrawString(T);
}

void UVPCMovementComponent::VisualizeMovement() const
{
    if (CharacterOwner == nullptr)
    {
        return;
    }

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
    FVector ActorLocation = UpdatedComponent ? UpdatedComponent->GetComponentLocation() : FVector::ZeroVector;
    const FVector TopOfCapsule = ActorLocation + FVector(0.f, 0.f, CharacterOwner->GetSimpleCollisionHalfHeight());
    float HeightOffset = 0.f;

    // Position
    {
        const FColor DebugColor = FColor::White;
        const FVector DebugLocation = TopOfCapsule + FVector(0.f,0.f,HeightOffset);
        FString DebugText = FString::Printf(TEXT("Position: %s"), *ActorLocation.ToCompactString());
        DrawDebugString(GetWorld(), DebugLocation, DebugText, nullptr, DebugColor, 0.f, true);
    }

    // Velocity
    {
        const FColor DebugColor = FColor::Green;
        HeightOffset += 15.f;
        const FVector DebugLocation = TopOfCapsule + FVector(0.f,0.f,HeightOffset);
        DrawDebugDirectionalArrow(GetWorld(), DebugLocation, DebugLocation + Velocity, 
            100.f, DebugColor, false, -1.f, (uint8)'\000', 10.f);

        FString DebugText = FString::Printf(TEXT("Velocity: %s (Speed: %.2f)"), *Velocity.ToCompactString(), Velocity.Size());
        DrawDebugString(GetWorld(), DebugLocation + FVector(0.f,0.f,5.f), DebugText, nullptr, DebugColor, 0.f, true);
    }

    // Acceleration
    {
        const FColor DebugColor = FColor::Yellow;
        HeightOffset += 15.f;
        const float MaxAccelerationLineLength = 200.f;
        const float CurrentMaxAccel = GetMaxAcceleration();
        const float CurrentAccelAsPercentOfMaxAccel = CurrentMaxAccel > 0.f ? Acceleration.Size() / CurrentMaxAccel : 1.f;
        const FVector DebugLocation = TopOfCapsule + FVector(0.f,0.f,HeightOffset);
        DrawDebugDirectionalArrow(GetWorld(), DebugLocation, 
            DebugLocation + Acceleration.GetSafeNormal(SMALL_NUMBER) * CurrentAccelAsPercentOfMaxAccel * MaxAccelerationLineLength, 
            25.f, DebugColor, false, -1.f, (uint8)'\000', 8.f);

        FString DebugText = FString::Printf(TEXT("Acceleration: %s"), *Acceleration.ToCompactString());
        DrawDebugString(GetWorld(), DebugLocation + FVector(0.f,0.f,5.f), DebugText, nullptr, DebugColor, 0.f, true);
    }

    // Movement Mode
    {
        const FColor DebugColor = FColor::Blue;
        HeightOffset += 20.f;
        const FVector DebugLocation = TopOfCapsule + FVector(0.f,0.f,HeightOffset);
        FString DebugText = FString::Printf(TEXT("MovementMode: %s"), *GetMovementName());
        DrawDebugString(GetWorld(), DebugLocation, DebugText, nullptr, DebugColor, 0.f, true);
    }
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

// ~ ACCELERATION

void UVPCMovementComponent::ForceReplicationUpdate()
{
    if (HasPredictionData_Server())
    {
        GetPredictionData_Server_VPC()->LastUpdateTime = GetWorld()->TimeSeconds - 10.f;
    }
}

FVector UVPCMovementComponent::ConstrainInputAcceleration(const FVector& InputAcceleration) const
{
    // Override input with RVO if required
    if (bUseRVOAvoidance && RVOAgentComponent && RVOAgentComponent->HasLockedPreferredVelocity())
    {
        const FVector AvoidanceVelocity( RVOAgentComponent->GetAvoidanceVelocity() );
        const FVector AvoidanceDirection( AvoidanceVelocity.GetSafeNormal() );
        const float CompMaxSpeed = GetMaxSpeed();
        const float AvoidanceMagnitude = CompMaxSpeed>0.f ? (AvoidanceVelocity.Size() / CompMaxSpeed) : 0.f;

        return AvoidanceDirection * FMath::Max(AvoidanceMagnitude, MinAvoidanceMagnitude);
    }

    return InputAcceleration;
}

FVector UVPCMovementComponent::ScaleInputAcceleration(const FVector& InputAcceleration) const
{
    return GetMaxAcceleration() * InputAcceleration.GetClampedToMaxSize(1.0f);
}

float UVPCMovementComponent::ComputeAnalogInputModifier() const
{
    const float MaxAccel = GetMaxAcceleration();

    if (Acceleration.SizeSquared() > 0.f && MaxAccel > SMALL_NUMBER)
    {
        return FMath::Clamp(Acceleration.Size() / MaxAccel, 0.f, 1.f);
    }

    return 0.f;
}

float UVPCMovementComponent::GetAnalogInputModifier() const
{
    return AnalogInputModifier;
}

// ~ ORIENTATION

float UVPCMovementComponent::GetCurrentTurnRate() const
{
    return CurrentTurnRate;
}

float UVPCMovementComponent::GetTurnRate() const
{
    return TurnRate;
}

float UVPCMovementComponent::GetMinTurnRate() const
{
    return MinTurnRate;
}

float UVPCMovementComponent::GetTurnAcceleration() const
{
    return TurnAcceleration;
}

void UVPCMovementComponent::SetTurnRate(float InTurnRate)
{
    TurnRate = FMath::Max(InTurnRate, 0.f);
    RefreshTurnRange();
}

void UVPCMovementComponent::SetMinTurnRate(float InMinTurnRate)
{
    MinTurnRate = InMinTurnRate;
    RefreshTurnRange();
}

void UVPCMovementComponent::SetTurnAcceleration(float InTurnAcceleration)
{
    TurnAcceleration = InTurnAcceleration;
    RefreshTurnRange();
}

void UVPCMovementComponent::RefreshTurnRange()
{
    bUseTurnRange = (MinTurnRate > 0.f) && (MinTurnRate < TurnRate) && (TurnAcceleration > 0.f);

    if (bUseTurnRange)
    {
        TurnInterpRangeInv = 1.f / (TurnRate-MinTurnRate);
        ClearTurnRange();
    }
    else
    {
        CurrentTurnRate = TurnRate;
    }
}

void UVPCMovementComponent::K2_RefreshTurnRange()
{
    RefreshTurnRange();
}

void UVPCMovementComponent::PrepareTurnRange(float DeltaTime, const FVector& VelocityNormal, const FVector& InputNormal)
{
    // turn range is not enabled, directly set current turn rate and return
    if (! bUseTurnRange)
    {
        if (CurrentTurnRate < TurnRate)
        {
            CurrentTurnRate = TurnRate;
        }
        return;
    }

    if (LastTurnDelta.SizeSquared() > 0.f)
    {
        FVector TurnDelta = InputNormal - VelocityNormal;

        // Turning towards the same direction since last update, accelerate turn rate
        if ((LastTurnDelta|TurnDelta) > 0.f)
        {
            // Update turn rate if not yet reached maximum turn rate
            if (CurrentTurnRate < TurnRate)
            {
                const float TurnInterpAlpha = ((CurrentTurnRate-MinTurnRate) + TurnAcceleration*DeltaTime) * TurnInterpRangeInv;
                const float TurnInterpSpeed = FMath::Lerp(MinTurnRate, TurnRate, FMath::Clamp(TurnInterpAlpha, 0.f, 1.f));
                CurrentTurnRate = FMath::Min(TurnInterpSpeed, TurnRate);
            }
        }
        // Turning towards the opposite direction from last update, reset turn rate
        else
        {
            ClearTurnRange();
        }

        LastTurnDelta = TurnDelta;
    }
    else
    {
        ClearTurnRange();
        LastTurnDelta = (InputNormal-VelocityNormal).GetSafeNormal();
    }
}

void UVPCMovementComponent::ClearTurnRange()
{
    if (bUseTurnRange && CurrentTurnRate > MinTurnRate)
    {
        CurrentTurnRate = MinTurnRate;
        LastTurnDelta = FVector::ZeroVector;
    }
}

void UVPCMovementComponent::InterpOrientation(FQuat& Orientation, const FVector& Current, const FVector& Target, float DeltaTime, float RotationSpeedDegrees)
{
    // Find delta rotation between both normals.
    FQuat DeltaQuat = FQuat::FindBetweenNormals(Current, Target);

    // Decompose into an axis and angle for rotation
    FVector DeltaAxis(0.f);
    float DeltaAngle = 0.f;
    DeltaQuat.ToAxisAndAngle(DeltaAxis, DeltaAngle);

    // Find rotation step for this frame
    const float RotationStepRadians = RotationSpeedDegrees * (PI / 180) * DeltaTime;

    if (FMath::Abs(DeltaAngle) > RotationStepRadians)
    {
        DeltaAngle = FMath::Clamp(DeltaAngle, -RotationStepRadians, RotationStepRadians);
        DeltaQuat = FQuat(DeltaAxis, DeltaAngle);
    }

    Orientation = DeltaQuat * Orientation;
}

void UVPCMovementComponent::PhysicsOrientation(float DeltaTime)
{
    // Not enough acceleration magnitude, abort
    if (Acceleration.SizeSquared() < SMALL_NUMBER || !HasValidData())
    {
        return;
    }

    VelocityOrientation = UpdatedComponent->GetComponentQuat();

    // Orient velocity towards acceleration
    {
        const FVector VelocityNormal = VelocityOrientation.GetForwardVector();
        const FVector AccelerationNormal = Acceleration.GetUnsafeNormal();

        // Interpolate velocity direction
        if (! FVector::Coincident(VelocityNormal, AccelerationNormal))
        {
            PrepareTurnRange(DeltaTime, VelocityNormal, AccelerationNormal);
            InterpOrientation(VelocityOrientation, VelocityNormal, AccelerationNormal, DeltaTime, CurrentTurnRate);
        }
        // Set direction directly if above dot threshold
        else
        {
            ClearTurnRange();
            InterpOrientation(VelocityOrientation, VelocityNormal, AccelerationNormal, DeltaTime, CurrentTurnRate);
        }
    }
}

// ~ REPLICATION

void UVPCMovementComponent::SmoothCorrection(const FVector& OldLocation, const FQuat& OldRotation, const FVector& NewLocation, const FQuat& NewRotation)
{
    SCOPE_CYCLE_COUNTER(STAT_VPCMovementSmoothCorrection);

    if (! HasValidData())
    {
        return;
    }

    // We shouldn't be running this on a standalone or dedicated server net mode
    checkSlow(GetNetMode() != NM_DedicatedServer);
    checkSlow(GetNetMode() != NM_Standalone);

    // Only client proxies or remote clients on a listen server should run this code.
    const bool bIsSimulatedProxy = (CharacterOwner->Role == ROLE_SimulatedProxy);
    const bool bIsRemoteAutoProxy = (CharacterOwner->GetRemoteRole() == ROLE_AutonomousProxy);
    ensure(bIsSimulatedProxy || bIsRemoteAutoProxy);

    // Getting a correction means new data, so smoothing needs to run.
    bNetworkSmoothingComplete = false;

    // Handle selected smoothing mode.
    if (NetworkSmoothingMode == ENetworkSmoothingMode::Replay)
    {
        // Replays use pure interpolation in this mode, all of the work is done in SmoothClientPosition_Interpolate
        return;
    }
    else if (NetworkSmoothingMode == ENetworkSmoothingMode::Disabled)
    {
        UpdatedComponent->SetWorldLocationAndRotation(NewLocation, NewRotation);
        bNetworkSmoothingComplete = true;
    }
    else if (FNetworkPredictionData_Client_VPC* ClientData = GetPredictionData_Client_VPC())
    {
        const UWorld* MyWorld = GetWorld();
        if (!ensure(MyWorld != nullptr))
        {
            return;
        }

        // The mesh doesn't move, but the capsule does so we have a new offset.
        FVector NewToOldVector = (OldLocation - NewLocation);
        const float DistSq = NewToOldVector.SizeSquared();

        if (DistSq > FMath::Square(ClientData->MaxSmoothNetUpdateDist))
        {
            ClientData->MeshTranslationOffset = (DistSq > FMath::Square(ClientData->NoSmoothNetUpdateDist)) 
                ? FVector::ZeroVector 
                : ClientData->MeshTranslationOffset + ClientData->MaxSmoothNetUpdateDist * NewToOldVector.GetSafeNormal();
        }
        else
        {
            ClientData->MeshTranslationOffset = ClientData->MeshTranslationOffset + NewToOldVector; 
        }

        if (NetworkSmoothingMode == ENetworkSmoothingMode::Linear)
        {
            ClientData->OriginalMeshTranslationOffset   = ClientData->MeshTranslationOffset;

            // Remember the current and target rotation, we're going to lerp between them
            ClientData->OriginalMeshRotationOffset      = OldRotation;
            ClientData->MeshRotationOffset              = OldRotation;
            ClientData->MeshRotationTarget              = NewRotation;

            // Move the capsule, but not the mesh.
            // Note: we don't change rotation, we lerp towards it in SmoothClientPosition.
            const FScopedPreventAttachedComponentMove PreventMeshMove(CharacterOwner->GetMeshRoot());
            UpdatedComponent->SetWorldLocation(NewLocation);
        }
        else
        {
            // Calc rotation needed to keep current world rotation after UpdatedComponent moves.
            // Take difference between where we were rotated before, and where we're going
            ClientData->MeshRotationOffset = (NewRotation.Inverse() * OldRotation) * ClientData->MeshRotationOffset;
            ClientData->MeshRotationTarget = FQuat::Identity;

            const FScopedPreventAttachedComponentMove PreventMeshMove(CharacterOwner->GetMeshRoot());
            UpdatedComponent->SetWorldLocationAndRotation(NewLocation, NewRotation);
        }

        //////////////////////////////////////////////////////////////////////////
        // Update smoothing timestamps

        // If running ahead, pull back slightly. This will cause the next delta to seem slightly longer, and cause us to lerp to it slightly slower.
        if (ClientData->SmoothingClientTimeStamp > ClientData->SmoothingServerTimeStamp)
        {
            const double OldClientTimeStamp = ClientData->SmoothingClientTimeStamp;
            ClientData->SmoothingClientTimeStamp = FMath::LerpStable(ClientData->SmoothingServerTimeStamp, OldClientTimeStamp, 0.5);

            UE_LOG(LogVPCNetSmoothing, VeryVerbose,
                TEXT("SmoothCorrection: Pull back client from ClientTimeStamp: %.6f to %.6f, ServerTimeStamp: %.6f for %s"),
                OldClientTimeStamp, ClientData->SmoothingClientTimeStamp, ClientData->SmoothingServerTimeStamp, *GetNameSafe(CharacterOwner));
        }

        // Using server timestamp lets us know how much time actually elapsed, regardless of packet lag variance.
        double OldServerTimeStamp = ClientData->SmoothingServerTimeStamp;

        ClientData->SmoothingServerTimeStamp = bIsSimulatedProxy
            ? CharacterOwner->GetReplicatedServerLastTransformUpdateTimeStamp()
            : ServerLastTransformUpdateTimeStamp;

        // Initial update has no delta.
        if (ClientData->LastCorrectionTime == 0)
        {
            ClientData->SmoothingClientTimeStamp = ClientData->SmoothingServerTimeStamp;
            OldServerTimeStamp = ClientData->SmoothingServerTimeStamp;
        }

        // Don't let the client fall too far behind or run ahead of new server time.
        const double ServerDeltaTime = ClientData->SmoothingServerTimeStamp - OldServerTimeStamp;
        const double MaxDelta = FMath::Clamp(ServerDeltaTime * 1.25, 0.0, ClientData->MaxMoveDeltaTime * 2.0);

        ClientData->SmoothingClientTimeStamp = FMath::Clamp(
            ClientData->SmoothingClientTimeStamp,
            ClientData->SmoothingServerTimeStamp - MaxDelta,
            ClientData->SmoothingServerTimeStamp);

        // Compute actual delta between new server timestamp and client simulation.
        ClientData->LastCorrectionDelta = ClientData->SmoothingServerTimeStamp - ClientData->SmoothingClientTimeStamp;
        ClientData->LastCorrectionTime = MyWorld->GetTimeSeconds();

        UE_LOG(LogVPCNetSmoothing, VeryVerbose,
            TEXT("SmoothCorrection: WorldTime: %.6f, ServerTimeStamp: %.6f, ClientTimeStamp: %.6f, Delta: %.6f for %s"),
            MyWorld->GetTimeSeconds(),
            ClientData->SmoothingServerTimeStamp,
            ClientData->SmoothingClientTimeStamp,
            ClientData->LastCorrectionDelta,
            *GetNameSafe(CharacterOwner));

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
        if (VPCMovementCVars::NetVisualizeSimulatedCorrections >= 2)
        {
            const float Radius      = 4.0f;
            const bool  bPersist    = false;
            const float Lifetime    = 10.0f;
            const int32 Sides       = 8;
            const float ArrowSize   = 4.0f;

            const FVector SimulatedLocation = OldLocation;
            const FVector ServerLocation    = NewLocation + FVector( 0, 0, 0.5f );

            const FVector SmoothLocation    = CharacterOwner->GetMeshRoot()->GetComponentLocation() - CharacterOwner->GetBaseTranslationOffset() + FVector(0, 0, 1.0f);

            //DrawDebugCoordinateSystem( GetWorld(), ServerLocation + FVector( 0, 0, 300.0f ), UpdatedComponent->GetComponentRotation(), 45.0f, bPersist, Lifetime );

            // Draw simulated location
            DrawCircle(GetWorld(), SimulatedLocation, FVector(1, 0, 0), FVector(0, 1, 0), FColor(255, 0, 0, 255), Radius, Sides, bPersist, Lifetime);

            // Draw server (corrected location)
            DrawCircle(GetWorld(), ServerLocation, FVector(1, 0, 0), FVector(0, 1, 0), FColor(0, 255, 0, 255), Radius, Sides, bPersist, Lifetime);

            // Draw smooth simulated location
            FRotationMatrix SmoothMatrix(CharacterOwner->GetMeshRoot()->GetComponentRotation());
            DrawDebugDirectionalArrow(GetWorld(), SmoothLocation, SmoothLocation + SmoothMatrix.GetScaledAxis(EAxis::Y) * 5, ArrowSize, FColor(255, 255, 0, 255), bPersist, Lifetime);
            DrawCircle(GetWorld(), SmoothLocation, FVector(1, 0, 0), FVector(0, 1, 0), FColor(0, 0, 255, 255), Radius, Sides, bPersist, Lifetime);

            if (ClientData->LastServerLocation_Debug != FVector::ZeroVector)
            {
                // Arrow showing simulated line
                DrawDebugDirectionalArrow(GetWorld(), ClientData->LastServerLocation_Debug, SimulatedLocation, ArrowSize, FColor(255, 0, 0, 255), bPersist, Lifetime);
                
                // Arrow showing server line
                DrawDebugDirectionalArrow(GetWorld(), ClientData->LastServerLocation_Debug, ServerLocation, ArrowSize, FColor(0, 255, 0, 255), bPersist, Lifetime);
                
                // Arrow showing smooth location plot
                DrawDebugDirectionalArrow(GetWorld(), ClientData->LastSmoothLocation_Debug, SmoothLocation, ArrowSize, FColor(0, 0, 255, 255), bPersist, Lifetime);

                // Line showing correction
                DrawDebugDirectionalArrow(GetWorld(), SimulatedLocation, ServerLocation, ArrowSize, FColor(128, 0, 0, 255), bPersist, Lifetime);
    
                // Line showing smooth vector
                DrawDebugDirectionalArrow(GetWorld(), ServerLocation, SmoothLocation, ArrowSize, FColor(0, 0, 128, 255), bPersist, Lifetime);
            }

            ClientData->LastServerLocation_Debug = ServerLocation;
            ClientData->LastSmoothLocation_Debug = SmoothLocation;
        }
#endif
    }
}

// TODO SERIALIZATION
FArchive& operator<<( FArchive& Ar, FVPCReplaySample& V )
{
    SerializePackedVector<10, 24>( V.Location, Ar );
    SerializePackedVector<10, 24>( V.Velocity, Ar );
    SerializePackedVector<10, 24>( V.Acceleration, Ar );
    V.Rotation.SerializeCompressed( Ar );

    return Ar;
}

void UVPCMovementComponent::SmoothClientPosition(float DeltaTime)
{
    if (!HasValidData() || NetworkSmoothingMode == ENetworkSmoothingMode::Disabled)
    {
        return;
    }

    // We shouldn't be running this on a server that is not a listen server.
    checkSlow(GetNetMode() != NM_DedicatedServer);
    checkSlow(GetNetMode() != NM_Standalone);

    // Only client proxies or remote clients on a listen server should run this code.
    const bool bIsSimulatedProxy = (CharacterOwner->Role == ROLE_SimulatedProxy);
    const bool bIsRemoteAutoProxy = (CharacterOwner->GetRemoteRole() == ROLE_AutonomousProxy);

    if (!ensure(bIsSimulatedProxy || bIsRemoteAutoProxy))
    {
        return;
    }

    // Update mesh offsets
    SmoothClientPosition_Interpolate(DeltaTime);
    SmoothClientPosition_UpdateVisuals();
}

void UVPCMovementComponent::SmoothClientPosition_Interpolate(float DeltaTime)
{
    SCOPE_CYCLE_COUNTER(STAT_VPCMovementSmoothClientPosition_Interp);
    FNetworkPredictionData_Client_VPC* ClientData = GetPredictionData_Client_VPC();
    if (ClientData)
    {
        if (NetworkSmoothingMode == ENetworkSmoothingMode::Linear)
        {
            const UWorld* MyWorld = GetWorld();

            // Increment client position.
            ClientData->SmoothingClientTimeStamp += DeltaTime;

            float LerpPercent = 0.f;
            const float LerpLimit = 1.15f;
            const float TargetDelta = ClientData->LastCorrectionDelta;

            if (TargetDelta > SMALL_NUMBER)
            {
                // Don't let the client get too far ahead (happens on spikes). But we do want a buffer for variable network conditions.
                const float MaxClientTimeAheadPercent = 0.15f;
                const float MaxTimeAhead = TargetDelta * MaxClientTimeAheadPercent;
                ClientData->SmoothingClientTimeStamp = FMath::Min<float>(ClientData->SmoothingClientTimeStamp, ClientData->SmoothingServerTimeStamp + MaxTimeAhead);

                // Compute interpolation alpha based on our client position within the server delta. We should take TargetDelta seconds to reach alpha of 1.
                const float RemainingTime = ClientData->SmoothingServerTimeStamp - ClientData->SmoothingClientTimeStamp;
                const float CurrentSmoothTime = TargetDelta - RemainingTime;
                LerpPercent = FMath::Clamp(CurrentSmoothTime / TargetDelta, 0.0f, LerpLimit);

                UE_LOG(LogVPCNetSmoothing, VeryVerbose,
                    TEXT("Interpolate: WorldTime: %.6f, ServerTimeStamp: %.6f, ClientTimeStamp: %.6f, Elapsed: %.6f, Alpha: %.6f for %s"),
                    MyWorld->GetTimeSeconds(), ClientData->SmoothingServerTimeStamp, ClientData->SmoothingClientTimeStamp, CurrentSmoothTime, LerpPercent, *GetNameSafe(CharacterOwner));
            }
            else
            {
                LerpPercent = 1.0f;
            }

            if (LerpPercent >= 1.0f - KINDA_SMALL_NUMBER)
            {
                if (Velocity.IsNearlyZero())
                {
                    ClientData->MeshTranslationOffset = FVector::ZeroVector;
                    ClientData->SmoothingClientTimeStamp = ClientData->SmoothingServerTimeStamp;
                    bNetworkSmoothingComplete = true;
                }
                else
                {
                    // Allow limited forward prediction.
                    ClientData->MeshTranslationOffset = FMath::LerpStable(ClientData->OriginalMeshTranslationOffset, FVector::ZeroVector, LerpPercent);
                    bNetworkSmoothingComplete = (LerpPercent >= LerpLimit);
                }

                ClientData->MeshRotationOffset = ClientData->MeshRotationTarget;
            }
            else
            {
                ClientData->MeshTranslationOffset = FMath::LerpStable(ClientData->OriginalMeshTranslationOffset, FVector::ZeroVector, LerpPercent);
                ClientData->MeshRotationOffset = FQuat::FastLerp(ClientData->OriginalMeshRotationOffset, ClientData->MeshRotationTarget, LerpPercent).GetNormalized();
            }

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
            // Show lerp percent
            if (VPCMovementCVars::NetVisualizeSimulatedCorrections >= 1)
            {
                const FColor DebugColor = FColor::White;
                const FVector DebugLocation = CharacterOwner->GetMeshRoot()->GetComponentLocation()+FVector(0.f, 0.f, 300.0f) - CharacterOwner->GetBaseTranslationOffset();
                FString DebugText = FString::Printf(TEXT("Lerp: %2.2f"), LerpPercent);
                DrawDebugString(GetWorld(), DebugLocation, DebugText, nullptr, DebugColor, 0.f, true);
            }
#endif
        }
        else if (NetworkSmoothingMode == ENetworkSmoothingMode::Exponential)
        {
            // Smooth interpolation of mesh translation to avoid popping of other client pawns unless under a low tick rate.
            // Faster interpolation if stopped.
            const float SmoothLocationTime = Velocity.IsZero() ? 0.5f*ClientData->SmoothNetUpdateTime : ClientData->SmoothNetUpdateTime;
            if (DeltaTime < SmoothLocationTime)
            {
                // Slowly decay translation offset
                ClientData->MeshTranslationOffset = (ClientData->MeshTranslationOffset * (1.f - DeltaTime / SmoothLocationTime));
            }
            else
            {
                ClientData->MeshTranslationOffset = FVector::ZeroVector;
            }

            // Smooth rotation
            const FQuat MeshRotationTarget = ClientData->MeshRotationTarget;
            if (DeltaTime < ClientData->SmoothNetUpdateRotationTime)
            {
                // Slowly decay rotation offset
                ClientData->MeshRotationOffset = FQuat::FastLerp(ClientData->MeshRotationOffset, MeshRotationTarget, DeltaTime / ClientData->SmoothNetUpdateRotationTime).GetNormalized();
            }
            else
            {
                ClientData->MeshRotationOffset = MeshRotationTarget;
            }

            // Check if lerp is complete
            if (ClientData->MeshTranslationOffset.IsNearlyZero(1e-2f) && ClientData->MeshRotationOffset.Equals(MeshRotationTarget, 1e-5f))
            {
                bNetworkSmoothingComplete = true;
                // Make sure to snap exactly to target values.
                ClientData->MeshTranslationOffset = FVector::ZeroVector;
                ClientData->MeshRotationOffset = MeshRotationTarget;
            }
        }
        else if (NetworkSmoothingMode == ENetworkSmoothingMode::Replay)
        {
            const UWorld* MyWorld = GetWorld();

            if (!MyWorld || !MyWorld->DemoNetDriver)
            {
                return;
            }

            const float CurrentTime = MyWorld->DemoNetDriver->DemoCurrentTime;

            // Remove old samples
            while (ClientData->ReplaySamples.Num() > 0)
            {
                if (ClientData->ReplaySamples[0].Time > CurrentTime - 1.0f)
                {
                    break;
                }

                ClientData->ReplaySamples.RemoveAt(0);
            }

            FReplayExternalDataArray* ExternalReplayData = MyWorld->DemoNetDriver->GetExternalDataArrayForObject(CharacterOwner);

            // Grab any samples available, deserialize them, then clear originals
            if (ExternalReplayData && ExternalReplayData->Num() > 0)
            {
                for (int i = 0; i < ExternalReplayData->Num(); i++)
                {
                    FVPCReplaySample ReplaySample;

                    (*ExternalReplayData)[i].Reader << ReplaySample;

                    ReplaySample.Time = (*ExternalReplayData)[i].TimeSeconds;

                    ClientData->ReplaySamples.Add(ReplaySample);
                }

                if (VPCMovementCVars::FixReplayOverSampling > 0)
                {
                    // Remove invalid replay samples that can occur due to oversampling (sampling at higher rate than physics is being ticked)
                    // We detect this by finding samples that have the same location but have a velocity that says the character should be moving
                    // If we don't do this, then characters will look like they are skipping around, which looks really bad
                    for (int i = 1; i < ClientData->ReplaySamples.Num(); i++)
                    {
                        if (ClientData->ReplaySamples[i].Location.Equals(ClientData->ReplaySamples[i-1].Location, KINDA_SMALL_NUMBER))
                        {
                            if (ClientData->ReplaySamples[i-1].Velocity.SizeSquared() > FMath::Square(KINDA_SMALL_NUMBER) && ClientData->ReplaySamples[i].Velocity.SizeSquared() > FMath::Square(KINDA_SMALL_NUMBER))
                            {
                                ClientData->ReplaySamples.RemoveAt(i);
                                i--;
                            }
                        }
                    }
                }

                ExternalReplayData->Empty();
            }

            bool bFoundSample = false;

            for (int i=0; i < ClientData->ReplaySamples.Num()-1; i++)
            {
                if (CurrentTime >= ClientData->ReplaySamples[i].Time && CurrentTime <= ClientData->ReplaySamples[i+1].Time)
                {
                    const float EPSILON     = SMALL_NUMBER;
                    const float Delta       = (ClientData->ReplaySamples[i+1].Time - ClientData->ReplaySamples[i].Time);
                    const float LerpPercent = Delta > EPSILON ? FMath::Clamp<float>((float)(CurrentTime-ClientData->ReplaySamples[i].Time) / Delta, 0.0f, 1.0f) : 1.0f;

                    const FVPCReplaySample& ReplaySample1 = ClientData->ReplaySamples[i];
                    const FVPCReplaySample& ReplaySample2 = ClientData->ReplaySamples[i+1];

                    const FVector Location = FMath::Lerp(ReplaySample1.Location, ReplaySample2.Location, LerpPercent);
                    const FQuat Rotation = FQuat::FastLerp(FQuat(ReplaySample1.Rotation), FQuat(ReplaySample2.Rotation), LerpPercent).GetNormalized();
                    Velocity = FMath::Lerp(ReplaySample1.Velocity, ReplaySample2.Velocity, LerpPercent);
                    //Acceleration = FMath::Lerp(ClientData->ReplaySamples[i].Acceleration, ClientData->ReplaySamples[i+1].Acceleration, LerpPercent);
                    Acceleration = ClientData->ReplaySamples[i+1].Acceleration;

                    UpdateComponentVelocity();

                    USceneComponent* Mesh = CharacterOwner->GetMeshRoot();

                    if (Mesh)
                    {
                        Mesh->RelativeLocation = CharacterOwner->GetBaseTranslationOffset();
                        Mesh->RelativeRotation = CharacterOwner->GetBaseRotationOffset().Rotator();
                    }

                    ClientData->MeshTranslationOffset = Location;
                    ClientData->MeshRotationOffset = Rotation;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
                    if (VPCMovementCVars::NetVisualizeSimulatedCorrections >= 1)
                    {
                        const float Radius      = 4.0f;
                        const int32 Sides       = 8;
                        const float ArrowSize   = 4.0f;
                        const FColor DebugColor = FColor::White;

                        const FVector DebugLocation = CharacterOwner->GetMeshRoot()->GetComponentLocation() + FVector(0.f, 0.f, 300.0f) - CharacterOwner->GetBaseTranslationOffset();

                        FString DebugText = FString::Printf(TEXT("Lerp: %2.2f"), LerpPercent);
                        DrawDebugString(GetWorld(), DebugLocation, DebugText, nullptr, DebugColor, 0.f, true);
                        DrawDebugBox(GetWorld(), DebugLocation, FVector(45, 45, 45), CharacterOwner->GetMeshRoot()->GetComponentQuat(), FColor(0, 255, 0));

                        DrawDebugDirectionalArrow(GetWorld(), DebugLocation, DebugLocation + Velocity, 20.0f, FColor(255, 0, 0, 255));
                    }
#endif
                    bFoundSample = true;
                    break;
                }
            }

            if (!bFoundSample)
            {
                int32 BestSample = -1;
                float BestTime = 0.0f;

                for (int i = 0; i < ClientData->ReplaySamples.Num(); i++)
                {
                    const FVPCReplaySample& ReplaySample = ClientData->ReplaySamples[i];

                    if (BestSample == -1 || FMath::Abs(BestTime - ReplaySample.Time) < BestTime)
                    {
                        BestTime = ReplaySample.Time;
                        BestSample = i;
                    }
                }

                if (BestSample != -1)
                {
                    const FVPCReplaySample& ReplaySample = ClientData->ReplaySamples[BestSample];

                    Velocity        = ReplaySample.Velocity;
                    Acceleration    = ReplaySample.Acceleration;

                    UpdateComponentVelocity();

                    USceneComponent* Mesh = CharacterOwner->GetMeshRoot();

                    if (Mesh)
                    {
                        Mesh->RelativeLocation = CharacterOwner->GetBaseTranslationOffset();
                        Mesh->RelativeRotation = CharacterOwner->GetBaseRotationOffset().Rotator();
                    }

                    ClientData->MeshTranslationOffset   = ReplaySample.Location;
                    ClientData->MeshRotationOffset      = ReplaySample.Rotation.Quaternion();
                }
            }

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
            // Show future samples
            if (VPCMovementCVars::NetVisualizeSimulatedCorrections >= 1)
            {
                const float Radius      = 4.0f;
                const int32 Sides       = 8;
                const float ArrowSize   = 4.0f;
                const FColor DebugColor = FColor::White;

                // Draw points ahead up to a few seconds
                for (int i = 0; i < ClientData->ReplaySamples.Num(); i++)
                {
                    const bool bHasMorePoints = i < ClientData->ReplaySamples.Num() - 1;
                    const bool bActiveSamples = (bHasMorePoints && CurrentTime >= ClientData->ReplaySamples[i].Time && CurrentTime <= ClientData->ReplaySamples[i+1].Time);

                    if (ClientData->ReplaySamples[i].Time >= CurrentTime || bActiveSamples)
                    {
                        //const FVector Adjust = FVector( 0.f, 0.f, 300.0f + i * 15.0f );
                        const FVector Adjust = FVector(0.f, 0.f, 300.0f);
                        const FVector Location = ClientData->ReplaySamples[i].Location + Adjust;

                        if (bHasMorePoints)
                        {
                            const FVector NextLocation = ClientData->ReplaySamples[i+1].Location + Adjust;
                            DrawDebugDirectionalArrow(GetWorld(), Location, NextLocation, 4.0f, FColor(0, 255, 0, 255));
                        }

                        DrawCircle(GetWorld(), Location, FVector(1, 0, 0), FVector(0, 1, 0), FColor(255, 0, 0, 255), Radius, Sides, false, 0.0f);

                        if (VPCMovementCVars::NetVisualizeSimulatedCorrections >= 2)
                        {
                            DrawDebugDirectionalArrow(GetWorld(), Location, Location + ClientData->ReplaySamples[i].Velocity, 20.0f, FColor(255, 0, 0, 255));
                        }

                        if (VPCMovementCVars::NetVisualizeSimulatedCorrections >= 3)
                        {
                            DrawDebugDirectionalArrow(GetWorld(), Location, Location + ClientData->ReplaySamples[i].Acceleration, 20.0f, FColor(255, 255, 255, 255));
                        }
                    }
                    
                    if (ClientData->ReplaySamples[i].Time - CurrentTime > 2.0f)
                    {
                        break;
                    }
                }
            }
#endif

            bNetworkSmoothingComplete = false;
        }
        else
        {
            // Unhandled mode
        }

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
        //UE_LOG(LogVPCNetSmoothing, VeryVerbose, TEXT("SmoothClientPosition_Interpolate %s: Translation: %s Rotation: %s"),
        //  *GetNameSafe(CharacterOwner), *ClientData->MeshTranslationOffset.ToString(), *ClientData->MeshRotationOffset.ToString());

        if (VPCMovementCVars::NetVisualizeSimulatedCorrections >= 1 && NetworkSmoothingMode != ENetworkSmoothingMode::Replay)
        {
            const FVector DebugLocation = CharacterOwner->GetMeshRoot()->GetComponentLocation() + FVector(0.f, 0.f, 300.0f) - CharacterOwner->GetBaseTranslationOffset();
            DrawDebugBox(GetWorld(), DebugLocation, FVector(45, 45, 45), CharacterOwner->GetMeshRoot()->GetComponentQuat(), FColor(0, 255, 0));

            //DrawDebugCoordinateSystem(GetWorld(), UpdatedComponent->GetComponentLocation() + FVector(0, 0, 300.0f), UpdatedComponent->GetComponentRotation(), 45.0f);
            //DrawDebugBox(GetWorld(), UpdatedComponent->GetComponentLocation() + FVector(0, 0, 300.0f), FVector(45, 45, 45), UpdatedComponent->GetComponentQuat(), FColor(0, 255, 0));

            if (VPCMovementCVars::NetVisualizeSimulatedCorrections >= 3)
            {
                ClientData->SimulatedDebugDrawTime_Debug += DeltaTime;

                if (ClientData->SimulatedDebugDrawTime_Debug >= 1.0f / 60.0f)
                {
                    const float Radius      = 2.0f;
                    const bool  bPersist    = false;
                    const float Lifetime    = 10.0f;
                    const int32 Sides       = 8;

                    const FVector SmoothLocation    = CharacterOwner->GetMeshRoot()->GetComponentLocation() - CharacterOwner->GetBaseTranslationOffset();
                    const FVector SimulatedLocation = UpdatedComponent->GetComponentLocation();

                    DrawCircle(GetWorld(), SmoothLocation + FVector(0, 0, 1.5f), FVector(1, 0, 0), FVector(0, 1, 0), FColor(0, 0, 255, 255), Radius, Sides, bPersist, Lifetime);
                    DrawCircle(GetWorld(), SimulatedLocation + FVector(0, 0, 2.0f), FVector(1, 0, 0), FVector(0, 1, 0), FColor(255, 0, 0, 255), Radius, Sides, bPersist, Lifetime);

                    ClientData->SimulatedDebugDrawTime_Debug = 0.0f;
                }
            }
        }
#endif
    }
}

void UVPCMovementComponent::SmoothClientPosition_UpdateVisuals()
{
    SCOPE_CYCLE_COUNTER(STAT_VPCMovementSmoothClientPosition_Visual);
    FNetworkPredictionData_Client_VPC* ClientData = GetPredictionData_Client_VPC();

    USceneComponent* Mesh = CharacterOwner->GetMeshRoot();

    //if (ClientData && Mesh && !Mesh->IsSimulatingPhysics())
    if (ClientData && Mesh)
    {
        if (NetworkSmoothingMode == ENetworkSmoothingMode::Linear)
        {
            // Adjust capsule rotation and mesh location. Optimized to trigger only one transform chain update.
            // If we know the rotation is changing that will update children, so it's sufficient to set RelativeLocation directly on the mesh.
            const FVector NewRelLocation = ClientData->MeshRotationOffset.UnrotateVector(ClientData->MeshTranslationOffset) + CharacterOwner->GetBaseTranslationOffset();
            if (!UpdatedComponent->GetComponentQuat().Equals(ClientData->MeshRotationOffset, SCENECOMPONENT_QUAT_TOLERANCE))
            {
                const FVector OldLocation = Mesh->RelativeLocation;
                const FRotator OldRotation = UpdatedComponent->RelativeRotation;
                Mesh->RelativeLocation = NewRelLocation;
                UpdatedComponent->SetWorldRotation(ClientData->MeshRotationOffset);

                // If we did not move from SetWorldRotation, we need to at least call SetRelativeLocation
                // since we were relying on the UpdatedComponent to update the transform of the mesh
                if (UpdatedComponent->RelativeRotation == OldRotation)
                {
                    Mesh->RelativeLocation = OldLocation;
                    Mesh->SetRelativeLocation(NewRelLocation);
                }
            }
            else
            {
                Mesh->SetRelativeLocation(NewRelLocation);
            }
        }
        else if (NetworkSmoothingMode == ENetworkSmoothingMode::Exponential)
        {
            // Adjust mesh location and rotation
            const FVector NewRelTranslation = UpdatedComponent->GetComponentToWorld().InverseTransformVectorNoScale(ClientData->MeshTranslationOffset) + CharacterOwner->GetBaseTranslationOffset();
            const FQuat NewRelRotation = ClientData->MeshRotationOffset * CharacterOwner->GetBaseRotationOffset();
            Mesh->SetRelativeLocationAndRotation(NewRelTranslation, NewRelRotation);
        }
        else if (NetworkSmoothingMode == ENetworkSmoothingMode::Replay)
        {
            if (!UpdatedComponent->GetComponentQuat().Equals(ClientData->MeshRotationOffset, SCENECOMPONENT_QUAT_TOLERANCE) || !UpdatedComponent->GetComponentLocation().Equals(ClientData->MeshTranslationOffset, KINDA_SMALL_NUMBER))
            {
                UpdatedComponent->SetWorldLocationAndRotation(ClientData->MeshTranslationOffset, ClientData->MeshRotationOffset);
            }
        }
        else
        {
            // Unhandled mode
        }
    }
}

//BEGIN INetworkPredictionInterface interface

void UVPCMovementComponent::SendClientAdjustment()
{
    if (!HasValidData())
    {
        return;
    }

    // Blank implementation, autonomous (client) update disabled, will never receive client adjustment
}

void UVPCMovementComponent::ForcePositionUpdate(float DeltaTime)
{
    if (!HasValidData() || MovementMode == MOVE_None || UpdatedComponent->Mobility != EComponentMobility::Movable)
    {
        return;
    }

    check(CharacterOwner->Role == ROLE_Authority);
    check(CharacterOwner->GetRemoteRole() == ROLE_AutonomousProxy);

    // TODO: smooth correction on listen server?
    PerformMovement(DeltaTime);
}

FNetworkPredictionData_Client* UVPCMovementComponent::GetPredictionData_Client() const
{
    if (ClientPredictionData == nullptr)
    {
        UVPCMovementComponent* MutableThis = const_cast<UVPCMovementComponent*>(this);
        MutableThis->ClientPredictionData = new FNetworkPredictionData_Client_VPC(*this);
    }

    return ClientPredictionData;
}

FNetworkPredictionData_Server* UVPCMovementComponent::GetPredictionData_Server() const
{
    if (ServerPredictionData == nullptr)
    {
        UVPCMovementComponent* MutableThis = const_cast<UVPCMovementComponent*>(this);
        MutableThis->ServerPredictionData = new FNetworkPredictionData_Server_VPC(*this);
    }

    return ServerPredictionData;
}

FNetworkPredictionData_Client_VPC* UVPCMovementComponent::GetPredictionData_Client_VPC() const
{
    // Should only be called on client or listen server (for remote clients) in network games
    checkSlow(CharacterOwner != NULL);
    checkSlow(CharacterOwner->Role < ROLE_Authority || (CharacterOwner->GetRemoteRole() == ROLE_AutonomousProxy && GetNetMode() == NM_ListenServer));
    checkSlow(GetNetMode() == NM_Client || GetNetMode() == NM_ListenServer);

    if (ClientPredictionData == nullptr)
    {
        UVPCMovementComponent* MutableThis = const_cast<UVPCMovementComponent*>(this);
        MutableThis->ClientPredictionData = static_cast<FNetworkPredictionData_Client_VPC*>(GetPredictionData_Client());
    }

    return ClientPredictionData;
}

FNetworkPredictionData_Server_VPC* UVPCMovementComponent::GetPredictionData_Server_VPC() const
{
    // Should only be called on server in network games
    checkSlow(CharacterOwner != NULL);
    checkSlow(CharacterOwner->Role == ROLE_Authority);
    checkSlow(GetNetMode() < NM_Client);

    if (ServerPredictionData == nullptr)
    {
        UVPCMovementComponent* MutableThis = const_cast<UVPCMovementComponent*>(this);
        MutableThis->ServerPredictionData = static_cast<FNetworkPredictionData_Server_VPC*>(GetPredictionData_Server());
    }

    return ServerPredictionData;
}

bool UVPCMovementComponent::HasPredictionData_Client() const
{
    return (ClientPredictionData != nullptr) && HasValidData();
}

bool UVPCMovementComponent::HasPredictionData_Server() const
{
    return (ServerPredictionData != nullptr) && HasValidData();
}

void UVPCMovementComponent::ResetPredictionData_Client()
{
    if (ClientPredictionData)
    {
        delete ClientPredictionData;
        ClientPredictionData = nullptr;
    }
}

void UVPCMovementComponent::ResetPredictionData_Server()
{
    if (ServerPredictionData)
    {
        delete ServerPredictionData;
        ServerPredictionData = nullptr;
    }   
}

//END INetworkPredictionInterface interface

void UVPCMovementComponent::PrimitiveTouched(UPrimitiveComponent* OverlappedComp, AActor* Other, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult )
{
    if (!bEnablePhysicsInteraction)
    {
        return;
    }

    if (OtherComp != NULL && OtherComp->IsAnySimulatingPhysics())
    {
        const FVector OtherLoc = OtherComp->GetComponentLocation();
        const FVector Loc = UpdatedComponent->GetComponentLocation();
        FVector ImpulseDir = FVector(OtherLoc.X - Loc.X, OtherLoc.Y - Loc.Y, 0.25f).GetSafeNormal();
        ImpulseDir = (ImpulseDir + Velocity.GetSafeNormal2D()) * 0.5f;
        ImpulseDir.Normalize();

        FName BoneName = NAME_None;
        if (OtherBodyIndex != INDEX_NONE)
        {
            BoneName = ((USkinnedMeshComponent*)OtherComp)->GetBoneName(OtherBodyIndex);
        }

        float TouchForceFactorModified = TouchForceFactor;

        if ( bTouchForceScaledToMass )
        {
            FBodyInstance* BI = OtherComp->GetBodyInstance(BoneName);
            TouchForceFactorModified *= BI ? BI->GetBodyMass() : 1.0f;
        }

        float ImpulseStrength = FMath::Clamp(Velocity.Size2D() * TouchForceFactorModified, 
            MinTouchForce > 0.0f ? MinTouchForce : -FLT_MAX, 
            MaxTouchForce > 0.0f ? MaxTouchForce : FLT_MAX);

        FVector Impulse = ImpulseDir * ImpulseStrength;

        OtherComp->AddImpulse(Impulse, BoneName);
    }
}

void UVPCMovementComponent::SetAvoidanceEnabled(bool bEnable)
{
    if (bUseRVOAvoidance != bEnable)
    {
        bUseRVOAvoidance = bEnable;
    }
}

void UVPCMovementComponent::ApplyRepulsionForce(float DeltaTime)
{
    if (UpdatedPrimitive && RepulsionForce > 0.0f && CharacterOwner!=nullptr)
    {
        const TArray<FOverlapInfo>& Overlaps = UpdatedPrimitive->GetOverlapInfos();
        if (Overlaps.Num() > 0)
        {
            FCollisionQueryParams QueryParams (SCENE_QUERY_STAT(CMC_ApplyRepulsionForce));
            QueryParams.bReturnFaceIndex = false;
            QueryParams.bReturnPhysicalMaterial = false;

            const float CollisionRadius = CharacterOwner->GetSimpleCollisionRadius();
            const float RepulsionForceRadius = CollisionRadius * 1.1f;
            const float StopBodyDistance = 2.5f;
            const FVector MyLocation = UpdatedPrimitive->GetComponentLocation();

            for (int32 i=0; i < Overlaps.Num(); i++)
            {
                const FOverlapInfo& Overlap = Overlaps[i];
                UPrimitiveComponent* OverlapComp = Overlap.OverlapInfo.Component.Get();

                if (!OverlapComp || OverlapComp->Mobility < EComponentMobility::Movable)
                {
                    continue;
                }

                // Use the body instead of the component for cases where we have multi-body overlaps enabled
                FBodyInstance* OverlapBody = nullptr;
                const int32 OverlapBodyIndex = Overlap.GetBodyIndex();
                const USkeletalMeshComponent* SkelMeshForBody = (OverlapBodyIndex != INDEX_NONE) ? Cast<USkeletalMeshComponent>(OverlapComp) : nullptr;

                if (SkelMeshForBody != nullptr)
                {
                    OverlapBody = SkelMeshForBody->Bodies.IsValidIndex(OverlapBodyIndex) ? SkelMeshForBody->Bodies[OverlapBodyIndex] : nullptr;
                }
                else
                {
                    OverlapBody = OverlapComp->GetBodyInstance();
                }

                if (!OverlapBody)
                {
                    UE_LOG(LogVPCMovement, Warning, TEXT("%s could not find overlap body for body index %d"), *GetName(), OverlapBodyIndex);
                    continue;
                }

                // Early out if this is not a destructible and the body is not simulated
                if (!OverlapBody->IsInstanceSimulatingPhysics() && !Cast<UDestructibleComponent>(OverlapComp))
                {
                    continue;
                }

                FTransform BodyTransform = OverlapBody->GetUnrealWorldTransform();

                FVector BodyVelocity = OverlapBody->GetUnrealWorldVelocity();
                FVector BodyLocation = BodyTransform.GetLocation();

                // Trace to get the hit location on the capsule
                FHitResult Hit;
                bool bHasHit = UpdatedPrimitive->LineTraceComponent(Hit, BodyLocation, MyLocation, QueryParams);

                FVector HitLoc = Hit.ImpactPoint;
                bool bIsPenetrating = Hit.bStartPenetrating || Hit.PenetrationDepth > StopBodyDistance;

                // If we didn't hit the capsule, we're inside the capsule
                if (!bHasHit) 
                {
                    HitLoc = BodyLocation;
                    bIsPenetrating = true;
                }

                const float DistanceNow = (HitLoc - BodyLocation).SizeSquared2D();
                const float DistanceLater = (HitLoc - (BodyLocation + BodyVelocity * DeltaTime)).SizeSquared2D();

                if (bHasHit && DistanceNow < StopBodyDistance && !bIsPenetrating)
                {
                    OverlapBody->SetLinearVelocity(FVector(0.0f, 0.0f, 0.0f), false);
                }
                else if (DistanceLater <= DistanceNow || bIsPenetrating)
                {
                    OverlapBody->AddRadialForceToBody(MyLocation, RepulsionForceRadius, RepulsionForce * Mass, ERadialImpulseFalloff::RIF_Constant);
                }
            }
        }
    }
}

void UVPCMovementComponent::ApplyAccumulatedForces(float DeltaTime)
{
    Velocity += PendingImpulseToApply + (PendingForceToApply * DeltaTime);
    
    // Don't call ClearAccumulatedForces() because it could affect launch velocity
    PendingImpulseToApply = FVector::ZeroVector;
    PendingForceToApply = FVector::ZeroVector;
}

void UVPCMovementComponent::ClearAccumulatedForces()
{
    PendingImpulseToApply = FVector::ZeroVector;
    PendingForceToApply = FVector::ZeroVector;
}

void UVPCMovementComponent::AddRadialForce(const FVector& Origin, float Radius, float Strength, enum ERadialImpulseFalloff Falloff)
{
    FVector Delta = UpdatedComponent->GetComponentLocation() - Origin;
    const float DeltaMagnitude = Delta.Size();

    // Do nothing if outside radius
    if (DeltaMagnitude > Radius)
    {
        return;
    }

    Delta = Delta.GetSafeNormal();

    float ForceMagnitude = Strength;
    if (Falloff == RIF_Linear && Radius > 0.0f)
    {
        ForceMagnitude *= (1.0f - (DeltaMagnitude / Radius));
    }

    AddForce(Delta * ForceMagnitude);
}
 
void UVPCMovementComponent::AddRadialImpulse(const FVector& Origin, float Radius, float Strength, enum ERadialImpulseFalloff Falloff, bool bVelChange)
{
    FVector Delta = UpdatedComponent->GetComponentLocation() - Origin;
    const float DeltaMagnitude = Delta.Size();

    // Do nothing if outside radius
    if (DeltaMagnitude > Radius)
    {
        return;
    }

    Delta = Delta.GetSafeNormal();

    float ImpulseMagnitude = Strength;
    if (Falloff == RIF_Linear && Radius > 0.0f)
    {
        ImpulseMagnitude *= (1.0f - (DeltaMagnitude / Radius));
    }

    AddImpulse(Delta * ImpulseMagnitude, bVelChange);
}

void UVPCMovementComponent::RegisterComponentTickFunctions(bool bRegister)
{
    Super::RegisterComponentTickFunctions(bRegister);

    if (bRegister)
    {
        if (SetupActorComponentTickFunction(&PostPhysicsTickFunction))
        {
            PostPhysicsTickFunction.Target = this;
            PostPhysicsTickFunction.AddPrerequisite(this, this->PrimaryComponentTick);
        }
    }
    else
    {
        if(PostPhysicsTickFunction.IsTickFunctionRegistered())
        {
            PostPhysicsTickFunction.UnRegisterTickFunction();
        }
    }
}

void UVPCMovementComponent::ApplyWorldOffset(const FVector& InOffset, bool bWorldShift)
{
    OldBaseLocation += InOffset;
    LastUpdateLocation += InOffset;

    if (CharacterOwner != nullptr && CharacterOwner->Role == ROLE_AutonomousProxy)
    {
        FNetworkPredictionData_Client_VPC* ClientData = GetPredictionData_Client_VPC();
        if (ClientData != nullptr)
        {
            for (int32 i = 0; i < ClientData->ReplaySamples.Num(); i++)
            {
                ClientData->ReplaySamples[i].Location += InOffset;
            }
        }
    }
}

void UVPCMovementComponent::TickCharacterPose(float DeltaTime)
{
    check(CharacterOwner && CharacterOwner->GetMesh());
    USkeletalMeshComponent* CharacterMesh = CharacterOwner->GetMesh();

    // bAutonomousTickPose is set, we control TickPose from the Character's Movement and Networking updates, and bypass the Component's update.
    // (Or Simulating Root Motion for remote clients)
    CharacterMesh->bIsAutonomousTickPose = true;

    if (CharacterMesh->ShouldTickPose())
    {
        CharacterMesh->TickPose(DeltaTime, true);
    }

    CharacterMesh->bIsAutonomousTickPose = false;
}



//BEGIN FNetworkPredictionData_Client_VPC

FNetworkPredictionData_Client_VPC::FNetworkPredictionData_Client_VPC(const UVPCMovementComponent& ClientMovement)
    : CurrentTimeStamp(0.f)
    , OriginalMeshTranslationOffset(ForceInitToZero)
    , MeshTranslationOffset(ForceInitToZero)
    , OriginalMeshRotationOffset(FQuat::Identity)
    , MeshRotationOffset(FQuat::Identity)   
    , MeshRotationTarget(FQuat::Identity)
    , LastCorrectionDelta(0.f)
    , LastCorrectionTime(0.f)
    , SmoothingServerTimeStamp(0.f)
    , SmoothingClientTimeStamp(0.f)
    , MaxSmoothNetUpdateDist(0.f)
    , NoSmoothNetUpdateDist(0.f)
    , SmoothNetUpdateTime(0.f)
    , SmoothNetUpdateRotationTime(0.f)
    , MaxMoveDeltaTime(0.125f)
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
    , LastSmoothLocation_Debug(FVector::ZeroVector)
    , LastServerLocation_Debug(FVector::ZeroVector)
    , SimulatedDebugDrawTime_Debug(0.0f)
#endif
{
    MaxSmoothNetUpdateDist = ClientMovement.NetworkMaxSmoothUpdateDistance;
    NoSmoothNetUpdateDist = ClientMovement.NetworkNoSmoothUpdateDistance;

    const bool bIsListenServer = (ClientMovement.GetNetMode() == NM_ListenServer);
    SmoothNetUpdateTime = (bIsListenServer ? ClientMovement.ListenServerNetworkSimulatedSmoothLocationTime : ClientMovement.NetworkSimulatedSmoothLocationTime);
    SmoothNetUpdateRotationTime = (bIsListenServer ? ClientMovement.ListenServerNetworkSimulatedSmoothRotationTime : ClientMovement.NetworkSimulatedSmoothRotationTime);

    AGameNetworkManager* GameNetworkManager = AGameNetworkManager::StaticClass()->GetDefaultObject<AGameNetworkManager>();
    if (GameNetworkManager)
    {
        MaxMoveDeltaTime = GameNetworkManager->MaxMoveDeltaTime;
    }
}

FNetworkPredictionData_Client_VPC::~FNetworkPredictionData_Client_VPC()
{
}

//END FNetworkPredictionData_Client_VPC



//BEGIN FNetworkPredictionData_Server_VPC

FNetworkPredictionData_Server_VPC::FNetworkPredictionData_Server_VPC(const UVPCMovementComponent& ServerMovement)
    : LastUpdateTime(0.f)
{
    if (const UWorld* World = ServerMovement.GetWorld())
    {
        ServerTimeStamp = World->GetTimeSeconds();
    }
}

FNetworkPredictionData_Server_VPC::~FNetworkPredictionData_Server_VPC()
{
}

//END FNetworkPredictionData_Server_VPC
