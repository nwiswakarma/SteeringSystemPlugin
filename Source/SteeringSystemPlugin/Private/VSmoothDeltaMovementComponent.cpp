// 

#include "VSmoothDeltaMovementComponent.h"
#include "VSmoothDeltaActor.h"
#include "VPawn.h"
#include "EngineStats.h"
#include "Engine/NetDriver.h"
#include "Engine/NetworkObjectList.h"
#include "GameFramework/GameNetworkManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogVSDMC, Log, All);
DEFINE_LOG_CATEGORY_STATIC(LogVSDMCNetSmoothing, Log, All);

DECLARE_CYCLE_STAT(TEXT("VSDMC Tick"), STAT_VSDMCMovementTick, STATGROUP_Steering);
DECLARE_CYCLE_STAT(TEXT("VSDMC NonSimulated Time"), STAT_VSDMCMovementNonSimulated, STATGROUP_Steering);
DECLARE_CYCLE_STAT(TEXT("VSDMC Simulated Time"), STAT_VSDMCMovementSimulated, STATGROUP_Steering);
DECLARE_CYCLE_STAT(TEXT("VSDMC PerformMovement"), STAT_VSDMCMovementPerformMovement, STATGROUP_Steering);
DECLARE_CYCLE_STAT(TEXT("VSDMC NetSmoothCorrection"), STAT_VSDMCMovementSmoothCorrection, STATGROUP_Steering);
DECLARE_CYCLE_STAT(TEXT("VSDMC SmoothClientPosition"), STAT_VSDMCMovementSmoothClientPosition, STATGROUP_Steering);
DECLARE_CYCLE_STAT(TEXT("VSDMC SmoothClientPosition_Interp"), STAT_VSDMCMovementSmoothClientPosition_Interp, STATGROUP_Steering);
DECLARE_CYCLE_STAT(TEXT("VSDMC SmoothClientPosition_Visual"), STAT_VSDMCMovementSmoothClientPosition_Visual, STATGROUP_Steering);
DECLARE_CYCLE_STAT(TEXT("VSDMC Physics Interation"), STAT_VSDMCPhysicsInteraction, STATGROUP_Steering);
DECLARE_CYCLE_STAT(TEXT("VSDMC Update Acceleration"), STAT_VSDMCUpdateAcceleration, STATGROUP_Steering);
DECLARE_CYCLE_STAT(TEXT("VSDMC MoveUpdateDelegate"), STAT_VSDMCMoveUpdateDelegate, STATGROUP_Steering);

const float UVSmoothDeltaMovementComponent::MIN_TICK_TIME = 1e-6f;

UVSmoothDeltaMovementComponent::UVSmoothDeltaMovementComponent(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    Duration = 1.f;
    bSnapToCurrentOnSourceChange = true;
    bEnableScopedMovementUpdates = true;

    MaxDepenetrationWithGeometry = 500.f;
    MaxDepenetrationWithGeometryAsProxy = 100.f;
    MaxDepenetrationWithPawn = 100.f;
    MaxDepenetrationWithPawnAsProxy = 2.f;

    // Simulated smooth simulation time
    NetworkSmoothSimulatedTime = 0.100f;
    ListenServerNetworkSmoothSimulatedTime = 0.040f;
    NetworkMaxSmoothUpdateDistance = 256.f;
    NetworkNoSmoothUpdateDistance = 384.f;
    NetworkSmoothingMode = ENetworkSmoothingMode::Exponential;
    
    // Spawned as teleported
    bJustTeleported = true;

    // Initially true until we get a net update, so we don't try to smooth to an uninitialized value.
    bNetworkSmoothingComplete = true;

    Mass = 100.0f;
    bEnablePhysicsInteraction = true;
    bTouchForceScaledToMass = true;
    bPushForceScaledToMass = false;
    bScalePushForceToVelocity = true;
    InitialPushForceFactor = 500.0f;
    PushForceFactor = 750000.0f;
    TouchForceFactor = 1.0f;
    MinTouchForce = -1.0f;
    MaxTouchForce = 250.0f;
    RepulsionForce = 2.5f;

    LastUpdateLocation = FVector::ZeroVector;
    LastUpdateRotation = FQuat::Identity;
    LastUpdateVelocity = FVector::ZeroVector;

    PendingImpulseToApply = FVector::ZeroVector;
    PendingForceToApply = FVector::ZeroVector;

    bMovementInProgress = false;
    bDeferUpdateMoveComponent = false;
    ServerLastUpdateTimeStamp = 0.f;
    ServerLastSimulationTime = 0.f;
}

void UVSmoothDeltaMovementComponent::SetUpdatedComponent(USceneComponent* NewUpdatedComponent)
{
    if (NewUpdatedComponent)
    {
        if (!ensureMsgf(Cast<AVSmoothDeltaActor>(NewUpdatedComponent->GetOwner()), TEXT("%s must update a component owned by a Pawn"), *GetName()))
        {
            return;
        }
    }

    // Movement is currently in progress, defer update
    if (bMovementInProgress)
    {
        bDeferUpdateMoveComponent = true;
        DeferredUpdatedMoveComponent = NewUpdatedComponent;
        return;
    }

    // Clear deferred flag and component
    bDeferUpdateMoveComponent = false;
    DeferredUpdatedMoveComponent = NULL;

    // Unregister old updated primitive bound events
    USceneComponent* OldUpdatedComponent = UpdatedComponent;
    UPrimitiveComponent* OldPrimitive = Cast<UPrimitiveComponent>(UpdatedComponent);

    if (IsValid(OldPrimitive) && OldPrimitive->OnComponentBeginOverlap.IsBound())
    {
        OldPrimitive->OnComponentBeginOverlap.RemoveDynamic(this, &UVSmoothDeltaMovementComponent::PrimitiveTouched);
    }

    Super::SetUpdatedComponent(NewUpdatedComponent);

    SDOwner = NewUpdatedComponent ? CastChecked<AVSmoothDeltaActor>(NewUpdatedComponent->GetOwner()) : NULL;

    if (UpdatedComponent != OldUpdatedComponent)
    {
        ClearAccumulatedForces();
    }

    if (UpdatedComponent == NULL)
    {
        StopMovementImmediately();
    }

    // Register updated primitive overlap events
    if (IsValid(UpdatedPrimitive) && bEnablePhysicsInteraction)
    {
        UpdatedPrimitive->OnComponentBeginOverlap.AddUniqueDynamic(this, &UVSmoothDeltaMovementComponent::PrimitiveTouched);
    }
}

void UVSmoothDeltaMovementComponent::Serialize(FArchive& Ar)
{
    AVSmoothDeltaActor* CurrentOwner = SDOwner;
    Super::Serialize(Ar);

    if (Ar.IsLoading())
    {
        // This was marked Transient so it won't be saved out, but we need still to reject old saved values.
        SDOwner = CurrentOwner;
    }
}

AVSmoothDeltaActor* UVSmoothDeltaMovementComponent::GetSDOwner() const
{
    return SDOwner;
}

void UVSmoothDeltaMovementComponent::MovementSourceChange()
{
    if (bSnapToCurrentOnSourceChange)
    {
        SnapToCurrentTime();
    }
}

void UVSmoothDeltaMovementComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
    SCOPED_NAMED_EVENT(UVSmoothDeltaMovementComponent, FColor::Yellow);
    SCOPE_CYCLE_COUNTER(STAT_VSDMCMovementTick);

    if (! HasValidData() || ShouldSkipUpdate(DeltaTime))
    {
        return;
    }

    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // Super tick may destroy/invalidate SDOwner or UpdatedComponent, so we need to re-check.
    if (! HasValidData())
    {
        return;
    }

    // See if we fell out of the world.
    const bool bIsSimulatingPhysics = UpdatedComponent->IsSimulatingPhysics();

    if (SDOwner->Role == ROLE_Authority && !SDOwner->CheckStillInWorld())
    {
        return;
    }

    if (SDOwner->Role == ROLE_Authority)
    {
        SCOPE_CYCLE_COUNTER(STAT_VSDMCMovementNonSimulated);
        PerformMovement(DeltaTime);
    }
    else if (SDOwner->Role == ROLE_SimulatedProxy)
    {
        SimulatedTick(DeltaTime);
    }

    if (bEnablePhysicsInteraction)
    {
        SCOPE_CYCLE_COUNTER(STAT_VSDMCPhysicsInteraction);
        ApplyRepulsionForce(DeltaTime);
    }
}

void UVSmoothDeltaMovementComponent::PerformMovement(float DeltaTime)
{
    SCOPE_CYCLE_COUNTER(STAT_VSDMCMovementPerformMovement);

    if (! HasValidData())
    {
        return;
    }
    
    // No movement if we can't move, or if currently doing physical simulation on UpdatedComponent
    if (UpdatedComponent->Mobility != EComponentMobility::Movable || UpdatedComponent->IsSimulatingPhysics())
    {
        ClearAccumulatedForces();
        return;
    }

    FVector OldVelocity;
    FVector OldLocation;

    {
        FScopedMovementUpdate ScopedMovementUpdate(UpdatedComponent, bEnableScopedMovementUpdates ? EScopedUpdate::DeferredUpdates : EScopedUpdate::ImmediateUpdates);

        OldVelocity = Velocity;
        OldLocation = UpdatedComponent->GetComponentLocation();

        ApplyAccumulatedForces(DeltaTime);
        ClearAccumulatedForces();

        // Perform movement update
        StartMovementUpdate(DeltaTime);

        if (! HasValidData())
        {
            return;
        }

        OnMovementUpdated(DeltaTime, OldLocation, OldVelocity);
    } // End scoped movement update

    // Call external post-movement events. These happen after the scoped movement completes in case the events want to use the current state of overlaps etc.
    CallMovementUpdateDelegate(DeltaTime, OldLocation, OldVelocity);

    UpdateComponentVelocity();

    const bool bHasAuthority = (SDOwner && SDOwner->HasAuthority());

    // If we move we want to avoid a long delay before replication catches up to notice this change, especially if it's throttling our rate.
    if (bHasAuthority && UpdatedComponent && UNetDriver::IsAdaptiveNetUpdateFrequencyEnabled())
    {
        if (const UWorld* W = GetWorld())
        {
            UNetDriver* NetDriver = W->GetNetDriver();
            if (NetDriver && NetDriver->IsServer())
            {
                FNetworkObjectInfo* NetActor = NetDriver->GetNetworkObjectInfo(SDOwner);
                if (NetActor && W->GetTimeSeconds() <= NetActor->NextUpdateTime && NetDriver->IsNetworkActorUpdateFrequencyThrottled(*NetActor))
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
            const UWorld* W = GetWorld();
            ServerLastUpdateTimeStamp = W ? W->GetTimeSeconds() : 0.f;
        }
    }

    LastUpdateLocation = NewLocation;
    LastUpdateRotation = NewRotation;
    LastUpdateVelocity = Velocity;
    ServerLastSimulationTime = CurrentSimulationTime;
}

void UVSmoothDeltaMovementComponent::StartMovementUpdate(float DeltaTime)
{
    if ((DeltaTime < MIN_TICK_TIME) || (Duration <= 0.f) || !HasValidData())
    {
        return;
    }

    if (UpdatedComponent->IsSimulatingPhysics())
    {
        UE_LOG(LogVSDMC, Log,
            TEXT("UVSmoothDeltaMovementComponent::StartMovementUpdate: UpdateComponent (%s) is simulating physics - aborting."),
            *UpdatedComponent->GetPathName());
        return;
    }

    const bool bSavedMovementInProgress = bMovementInProgress;
    bMovementInProgress = true;

    CurrentSimulationTime = GetClampedSimulationTime(CurrentSimulationTime + DeltaTime);
    MovementUpdate(DeltaTime);

    bMovementInProgress = bSavedMovementInProgress;

    if (bDeferUpdateMoveComponent)
    {
        SetUpdatedComponent(DeferredUpdatedMoveComponent);
    }
}

void UVSmoothDeltaMovementComponent::MovementUpdate(float DeltaTime)
{
    // Blank implementation, intended for derived classes to override.
}

void UVSmoothDeltaMovementComponent::SimulatedTick(float DeltaTime)
{
    SCOPE_CYCLE_COUNTER(STAT_VSDMCMovementSimulated);
    checkSlow(SDOwner != nullptr);
    
    //if (NetworkSmoothingMode == ENetworkSmoothingMode::Replay)
    //{
    //    const FVector OldLocation = UpdatedComponent ? UpdatedComponent->GetComponentLocation() : FVector::ZeroVector;
    //    const FVector OldVelocity = Velocity;
    //
    //    // Interpolate between appropriate samples
    //    {
    //        SCOPE_CYCLE_COUNTER(STAT_VSDMCMovementSmoothClientPosition);
    //        SmoothClientPosition(DeltaTime);
    //    }
    //
    //    // Update replicated movement mode
    //    ApplyNetworkMovementMode(SDOwner->GetReplicatedMovementMode());
    //
    //    UpdateComponentVelocity();
    //    bJustTeleported = false;
    //
    //    // Note: we do not call the Super implementation, that runs prediction.
    //    // We do still need to call these though
    //    OnMovementUpdated(DeltaTime, OldLocation, OldVelocity);
    //    CallMovementUpdateDelegate(DeltaTime, OldLocation, OldVelocity);
    //
    //    LastUpdateLocation = UpdatedComponent ? UpdatedComponent->GetComponentLocation() : FVector::ZeroVector;
    //    LastUpdateRotation = UpdatedComponent ? UpdatedComponent->GetComponentQuat() : FQuat::Identity;
    //    LastUpdateVelocity = Velocity;
    //
    //    //TickCharacterPose( DeltaTime );
    //    return;
    //}
    
    {
        // Avoid moving the mesh during movement if SmoothClientPosition will take care of it.
        const FScopedPreventAttachedComponentMove PreventMeshMovement(bNetworkSmoothingComplete ? nullptr : SDOwner->GetMeshRoot());
    
        if (SDOwner->IsMatineeControlled())
        {
            PerformMovement(DeltaTime);
        }
        else
        {
            SimulateMovement(DeltaTime);
        }
    }
    
    // Smooth mesh location after moving the capsule above.
    if (!bNetworkSmoothingComplete)
    {
        SCOPE_CYCLE_COUNTER(STAT_VSDMCMovementSmoothClientPosition);
        SmoothClientPosition(DeltaTime);
    }
    else
    {
        UE_LOG(LogVSDMCNetSmoothing, Verbose, TEXT("Skipping network smoothing for %s."), *GetNameSafe(SDOwner));
    }
}

void UVSmoothDeltaMovementComponent::SimulateMovement(float DeltaTime)
{
    if (!HasValidData() || UpdatedComponent->Mobility != EComponentMobility::Movable || UpdatedComponent->IsSimulatingPhysics())
    {
        return;
    }

    const bool bIsSimulatedProxy = (SDOwner->Role == ROLE_SimulatedProxy);

    // Workaround for replication not being updated initially
    if (bIsSimulatedProxy && (SDOwner->GetReplicatedUpdateTimeStamp() <= 0.f || Duration <= MIN_TICK_TIME))
    {
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

                if (bJustTeleported)
                {
                    bJustTeleported = false;
                }
            }
        }

        ClearAccumulatedForces();

        // Simulated pawns predict location
        OldVelocity = Velocity;
        OldLocation = UpdatedComponent->GetComponentLocation();

        // Calculate current simulation time
        CurrentSimulationTime = GetClampedSimulationTime(CurrentSimulationTime + DeltaTime);

        MoveSmooth(DeltaTime);

        OnMovementUpdated(DeltaTime, OldLocation, OldVelocity);
    } // End scoped movement update

    // Call custom post-movement events. These happen after the scoped movement completes in case the events want to use the current state of overlaps etc.
    CallMovementUpdateDelegate(DeltaTime, OldLocation, OldVelocity);

    UpdateComponentVelocity();
    bJustTeleported = false;

    LastUpdateLocation = UpdatedComponent ? UpdatedComponent->GetComponentLocation() : FVector::ZeroVector;
    LastUpdateRotation = UpdatedComponent ? UpdatedComponent->GetComponentQuat() : FQuat::Identity;
    LastUpdateVelocity = Velocity;
}

void UVSmoothDeltaMovementComponent::MoveSmooth(float DeltaTime)
{
    // Blank implementation, intended for derived classes to override.
}

void UVSmoothDeltaMovementComponent::MoveSmooth_Visual()
{
    // Blank implementation, intended for derived classes to override.
}

void UVSmoothDeltaMovementComponent::SnapToCurrentTime()
{
    // Blank implementation, intended for derived classes to override.
}

void UVSmoothDeltaMovementComponent::SmoothClientPosition(float DeltaTime)
{
    if (!HasValidData() || NetworkSmoothingMode == ENetworkSmoothingMode::Disabled)
    {
        return;
    }
    
    // We shouldn't be running this on dedicated server or standalone mode (listen server / client only)
    checkSlow(GetNetMode() != NM_DedicatedServer);
    checkSlow(GetNetMode() != NM_Standalone);
    
    // Only client proxies or remote clients on a listen server should run this code.
    const bool bIsSimulatedProxy = (SDOwner->Role == ROLE_SimulatedProxy);
    
    if (!ensure(bIsSimulatedProxy))
    {
        return;
    }
    
    // Update mesh offsets
    SmoothClientPosition_Interpolate(DeltaTime);
    SmoothClientPosition_UpdateVisuals();
}

void UVSmoothDeltaMovementComponent::SmoothClientPosition_Interpolate(float DeltaTime)
{
    SCOPE_CYCLE_COUNTER(STAT_VSDMCMovementSmoothClientPosition_Interp);

    const bool bIsListenServer = (GetNetMode() == NM_ListenServer);
    const float SmoothNetUpdateTime = (bIsListenServer ? ListenServerNetworkSmoothSimulatedTime : NetworkSmoothSimulatedTime);

    if (NetworkSmoothingMode == ENetworkSmoothingMode::Linear)
    {
//        const UWorld* MyWorld = GetWorld();
//
//        // Increment client position.
//        ClientData->SmoothingClientTimeStamp += DeltaTime;
//
//        float LerpPercent = 0.f;
//        const float LerpLimit = 1.15f;
//        const float TargetDelta = ClientData->LastCorrectionDelta;
//
//        if (TargetDelta > SMALL_NUMBER)
//        {
//            // Don't let the client get too far ahead (happens on spikes). But we do want a buffer for variable network conditions.
//            const float MaxClientTimeAheadPercent = 0.15f;
//            const float MaxTimeAhead = TargetDelta * MaxClientTimeAheadPercent;
//            ClientData->SmoothingClientTimeStamp = FMath::Min<float>(ClientData->SmoothingClientTimeStamp, ClientData->SmoothingServerTimeStamp + MaxTimeAhead);
//
//            // Compute interpolation alpha based on our client position within the server delta. We should take TargetDelta seconds to reach alpha of 1.
//            const float RemainingTime = ClientData->SmoothingServerTimeStamp - ClientData->SmoothingClientTimeStamp;
//            const float CurrentSmoothTime = TargetDelta - RemainingTime;
//            LerpPercent = FMath::Clamp(CurrentSmoothTime / TargetDelta, 0.0f, LerpLimit);
//
//            UE_LOG(LogVSDMCNetSmoothing, VeryVerbose,
//                TEXT("Interpolate: WorldTime: %.6f, ServerTimeStamp: %.6f, ClientTimeStamp: %.6f, Elapsed: %.6f, Alpha: %.6f for %s"),
//                MyWorld->GetTimeSeconds(), ClientData->SmoothingServerTimeStamp, ClientData->SmoothingClientTimeStamp, CurrentSmoothTime, LerpPercent, *GetNameSafe(SDOwner));
//        }
//        else
//        {
//            LerpPercent = 1.0f;
//        }
//
//        if (LerpPercent >= 1.0f - KINDA_SMALL_NUMBER)
//        {
//            if (Velocity.IsNearlyZero())
//            {
//                ClientData->MeshTranslationOffset = FVector::ZeroVector;
//                ClientData->SmoothingClientTimeStamp = ClientData->SmoothingServerTimeStamp;
//                bNetworkSmoothingComplete = true;
//            }
//            else
//            {
//                // Allow limited forward prediction.
//                ClientData->MeshTranslationOffset = FMath::LerpStable(ClientData->OriginalMeshTranslationOffset, FVector::ZeroVector, LerpPercent);
//                bNetworkSmoothingComplete = (LerpPercent >= LerpLimit);
//            }
//
//            ClientData->MeshRotationOffset = ClientData->MeshRotationTarget;
//        }
//        else
//        {
//            ClientData->MeshTranslationOffset = FMath::LerpStable(ClientData->OriginalMeshTranslationOffset, FVector::ZeroVector, LerpPercent);
//            ClientData->MeshRotationOffset = FQuat::FastLerp(ClientData->OriginalMeshRotationOffset, ClientData->MeshRotationTarget, LerpPercent).GetNormalized();
//        }
//
//#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
//        // Show lerp percent
//        if (VSDMCMovementCVars::NetVisualizeSimulatedCorrections >= 1)
//        {
//            const FColor DebugColor = FColor::White;
//            const FVector DebugLocation = SDOwner->GetMeshRoot()->GetComponentLocation()+FVector(0.f, 0.f, 300.0f) - SDOwner->GetBaseTranslationOffset();
//            FString DebugText = FString::Printf(TEXT("Lerp: %2.2f"), LerpPercent);
//            DrawDebugString(GetWorld(), DebugLocation, DebugText, nullptr, DebugColor, 0.f, true);
//        }
//#endif
    }
    else if (NetworkSmoothingMode == ENetworkSmoothingMode::Exponential)
    {
        if (DeltaTime < SmoothNetUpdateTime)
        {
            SmoothSimulationOffset = SmoothSimulationOffset * (1.f - DeltaTime / SmoothNetUpdateTime);
        }
        else
        {
            SmoothSimulationOffset = 0.f;
        }

        // Network smoothing completed
        if (SmoothSimulationOffset < 1e-3)
        {
            bNetworkSmoothingComplete = true;
            SmoothSimulationOffset = 0.f;
        }
    }
    else if (NetworkSmoothingMode == ENetworkSmoothingMode::Replay)
    {
//        const UWorld* MyWorld = GetWorld();
//
//        if (!MyWorld || !MyWorld->DemoNetDriver)
//        {
//            return;
//        }
//
//        const float CurrentTime = MyWorld->DemoNetDriver->DemoCurrentTime;
//
//        // Remove old samples
//        while (ClientData->ReplaySamples.Num() > 0)
//        {
//            if (ClientData->ReplaySamples[0].Time > CurrentTime - 1.0f)
//            {
//                break;
//            }
//
//            ClientData->ReplaySamples.RemoveAt(0);
//        }
//
//        FReplayExternalDataArray* ExternalReplayData = MyWorld->DemoNetDriver->GetExternalDataArrayForObject(SDOwner);
//
//        // Grab any samples available, deserialize them, then clear originals
//        if (ExternalReplayData && ExternalReplayData->Num() > 0)
//        {
//            for (int i = 0; i < ExternalReplayData->Num(); i++)
//            {
//                FVSDMCReplaySample ReplaySample;
//
//                (*ExternalReplayData)[i].Reader << ReplaySample;
//
//                ReplaySample.Time = (*ExternalReplayData)[i].TimeSeconds;
//
//                ClientData->ReplaySamples.Add(ReplaySample);
//            }
//
//            if (VSDMCMovementCVars::FixReplayOverSampling > 0)
//            {
//                // Remove invalid replay samples that can occur due to oversampling (sampling at higher rate than physics is being ticked)
//                // We detect this by finding samples that have the same location but have a velocity that says the character should be moving
//                // If we don't do this, then characters will look like they are skipping around, which looks really bad
//                for (int i = 1; i < ClientData->ReplaySamples.Num(); i++)
//                {
//                    if (ClientData->ReplaySamples[i].Location.Equals(ClientData->ReplaySamples[i-1].Location, KINDA_SMALL_NUMBER))
//                    {
//                        if (ClientData->ReplaySamples[i-1].Velocity.SizeSquared() > FMath::Square(KINDA_SMALL_NUMBER) && ClientData->ReplaySamples[i].Velocity.SizeSquared() > FMath::Square(KINDA_SMALL_NUMBER))
//                        {
//                            ClientData->ReplaySamples.RemoveAt(i);
//                            i--;
//                        }
//                    }
//                }
//            }
//
//            ExternalReplayData->Empty();
//        }
//
//        bool bFoundSample = false;
//
//        for (int i=0; i < ClientData->ReplaySamples.Num()-1; i++)
//        {
//            if (CurrentTime >= ClientData->ReplaySamples[i].Time && CurrentTime <= ClientData->ReplaySamples[i+1].Time)
//            {
//                const float EPSILON     = SMALL_NUMBER;
//                const float Delta       = (ClientData->ReplaySamples[i+1].Time - ClientData->ReplaySamples[i].Time);
//                const float LerpPercent = Delta > EPSILON ? FMath::Clamp<float>((float)(CurrentTime-ClientData->ReplaySamples[i].Time) / Delta, 0.0f, 1.0f) : 1.0f;
//
//                const FVSDMCReplaySample& ReplaySample1 = ClientData->ReplaySamples[i];
//                const FVSDMCReplaySample& ReplaySample2 = ClientData->ReplaySamples[i+1];
//
//                const FVector Location = FMath::Lerp(ReplaySample1.Location, ReplaySample2.Location, LerpPercent);
//                const FQuat Rotation = FQuat::FastLerp(FQuat(ReplaySample1.Rotation), FQuat(ReplaySample2.Rotation), LerpPercent).GetNormalized();
//                Velocity = FMath::Lerp(ReplaySample1.Velocity, ReplaySample2.Velocity, LerpPercent);
//                //Acceleration = FMath::Lerp(ClientData->ReplaySamples[i].Acceleration, ClientData->ReplaySamples[i+1].Acceleration, LerpPercent);
//                Acceleration = ClientData->ReplaySamples[i+1].Acceleration;
//
//                UpdateComponentVelocity();
//
//                USceneComponent* Mesh = SDOwner->GetMeshRoot();
//
//                if (Mesh)
//                {
//                    Mesh->RelativeLocation = SDOwner->GetBaseTranslationOffset();
//                    Mesh->RelativeRotation = SDOwner->GetBaseRotationOffset().Rotator();
//                }
//
//                ClientData->MeshTranslationOffset = Location;
//                ClientData->MeshRotationOffset = Rotation;
//
//#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
//                if (VSDMCMovementCVars::NetVisualizeSimulatedCorrections >= 1)
//                {
//                    const float Radius      = 4.0f;
//                    const int32 Sides       = 8;
//                    const float ArrowSize   = 4.0f;
//                    const FColor DebugColor = FColor::White;
//
//                    const FVector DebugLocation = SDOwner->GetMeshRoot()->GetComponentLocation() + FVector(0.f, 0.f, 300.0f) - SDOwner->GetBaseTranslationOffset();
//
//                    FString DebugText = FString::Printf(TEXT("Lerp: %2.2f"), LerpPercent);
//                    DrawDebugString(GetWorld(), DebugLocation, DebugText, nullptr, DebugColor, 0.f, true);
//                    DrawDebugBox(GetWorld(), DebugLocation, FVector(45, 45, 45), SDOwner->GetMeshRoot()->GetComponentQuat(), FColor(0, 255, 0));
//
//                    DrawDebugDirectionalArrow(GetWorld(), DebugLocation, DebugLocation + Velocity, 20.0f, FColor(255, 0, 0, 255));
//                }
//#endif
//                bFoundSample = true;
//                break;
//            }
//        }
//
//        if (!bFoundSample)
//        {
//            int32 BestSample = -1;
//            float BestTime = 0.0f;
//
//            for (int i = 0; i < ClientData->ReplaySamples.Num(); i++)
//            {
//                const FVSDMCReplaySample& ReplaySample = ClientData->ReplaySamples[i];
//
//                if (BestSample == -1 || FMath::Abs(BestTime - ReplaySample.Time) < BestTime)
//                {
//                    BestTime = ReplaySample.Time;
//                    BestSample = i;
//                }
//            }
//
//            if (BestSample != -1)
//            {
//                const FVSDMCReplaySample& ReplaySample = ClientData->ReplaySamples[BestSample];
//
//                Velocity        = ReplaySample.Velocity;
//                Acceleration    = ReplaySample.Acceleration;
//
//                UpdateComponentVelocity();
//
//                USceneComponent* Mesh = SDOwner->GetMeshRoot();
//
//                if (Mesh)
//                {
//                    Mesh->RelativeLocation = SDOwner->GetBaseTranslationOffset();
//                    Mesh->RelativeRotation = SDOwner->GetBaseRotationOffset().Rotator();
//                }
//
//                ClientData->MeshTranslationOffset   = ReplaySample.Location;
//                ClientData->MeshRotationOffset      = ReplaySample.Rotation.Quaternion();
//            }
//        }
//
//#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
//        // Show future samples
//        if (VSDMCMovementCVars::NetVisualizeSimulatedCorrections >= 1)
//        {
//            const float Radius      = 4.0f;
//            const int32 Sides       = 8;
//            const float ArrowSize   = 4.0f;
//            const FColor DebugColor = FColor::White;
//
//            // Draw points ahead up to a few seconds
//            for (int i = 0; i < ClientData->ReplaySamples.Num(); i++)
//            {
//                const bool bHasMorePoints = i < ClientData->ReplaySamples.Num() - 1;
//                const bool bActiveSamples = (bHasMorePoints && CurrentTime >= ClientData->ReplaySamples[i].Time && CurrentTime <= ClientData->ReplaySamples[i+1].Time);
//
//                if (ClientData->ReplaySamples[i].Time >= CurrentTime || bActiveSamples)
//                {
//                    //const FVector Adjust = FVector( 0.f, 0.f, 300.0f + i * 15.0f );
//                    const FVector Adjust = FVector(0.f, 0.f, 300.0f);
//                    const FVector Location = ClientData->ReplaySamples[i].Location + Adjust;
//
//                    if (bHasMorePoints)
//                    {
//                        const FVector NextLocation = ClientData->ReplaySamples[i+1].Location + Adjust;
//                        DrawDebugDirectionalArrow(GetWorld(), Location, NextLocation, 4.0f, FColor(0, 255, 0, 255));
//                    }
//
//                    DrawCircle(GetWorld(), Location, FVector(1, 0, 0), FVector(0, 1, 0), FColor(255, 0, 0, 255), Radius, Sides, false, 0.0f);
//
//                    if (VSDMCMovementCVars::NetVisualizeSimulatedCorrections >= 2)
//                    {
//                        DrawDebugDirectionalArrow(GetWorld(), Location, Location + ClientData->ReplaySamples[i].Velocity, 20.0f, FColor(255, 0, 0, 255));
//                    }
//
//                    if (VSDMCMovementCVars::NetVisualizeSimulatedCorrections >= 3)
//                    {
//                        DrawDebugDirectionalArrow(GetWorld(), Location, Location + ClientData->ReplaySamples[i].Acceleration, 20.0f, FColor(255, 255, 255, 255));
//                    }
//                }
//                
//                if (ClientData->ReplaySamples[i].Time - CurrentTime > 2.0f)
//                {
//                    break;
//                }
//            }
//        }
//#endif
//
//        bNetworkSmoothingComplete = false;
    }
    else
    {
        // Unhandled mode
    }

//#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
//    //UE_LOG(LogVSDMCNetSmoothing, VeryVerbose, TEXT("SmoothClientPosition_Interpolate %s: Translation: %s Rotation: %s"),
//    //  *GetNameSafe(SDOwner), *ClientData->MeshTranslationOffset.ToString(), *ClientData->MeshRotationOffset.ToString());
//
//    if (VSDMCMovementCVars::NetVisualizeSimulatedCorrections >= 1 && NetworkSmoothingMode != ENetworkSmoothingMode::Replay)
//    {
//        const FVector DebugLocation = SDOwner->GetMeshRoot()->GetComponentLocation() + FVector(0.f, 0.f, 300.0f) - SDOwner->GetBaseTranslationOffset();
//        DrawDebugBox(GetWorld(), DebugLocation, FVector(45, 45, 45), SDOwner->GetMeshRoot()->GetComponentQuat(), FColor(0, 255, 0));
//
//        //DrawDebugCoordinateSystem(GetWorld(), UpdatedComponent->GetComponentLocation() + FVector(0, 0, 300.0f), UpdatedComponent->GetComponentRotation(), 45.0f);
//        //DrawDebugBox(GetWorld(), UpdatedComponent->GetComponentLocation() + FVector(0, 0, 300.0f), FVector(45, 45, 45), UpdatedComponent->GetComponentQuat(), FColor(0, 255, 0));
//
//        if (VSDMCMovementCVars::NetVisualizeSimulatedCorrections >= 3)
//        {
//            ClientData->SimulatedDebugDrawTime_Debug += DeltaTime;
//
//            if (ClientData->SimulatedDebugDrawTime_Debug >= 1.0f / 60.0f)
//            {
//                const float Radius      = 2.0f;
//                const bool  bPersist    = false;
//                const float Lifetime    = 10.0f;
//                const int32 Sides       = 8;
//
//                const FVector SmoothLocation    = SDOwner->GetMeshRoot()->GetComponentLocation() - SDOwner->GetBaseTranslationOffset();
//                const FVector SimulatedLocation = UpdatedComponent->GetComponentLocation();
//
//                DrawCircle(GetWorld(), SmoothLocation + FVector(0, 0, 1.5f), FVector(1, 0, 0), FVector(0, 1, 0), FColor(0, 0, 255, 255), Radius, Sides, bPersist, Lifetime);
//                DrawCircle(GetWorld(), SimulatedLocation + FVector(0, 0, 2.0f), FVector(1, 0, 0), FVector(0, 1, 0), FColor(255, 0, 0, 255), Radius, Sides, bPersist, Lifetime);
//
//                ClientData->SimulatedDebugDrawTime_Debug = 0.0f;
//            }
//        }
//    }
//#endif
}

void UVSmoothDeltaMovementComponent::SmoothClientPosition_UpdateVisuals()
{
    SCOPE_CYCLE_COUNTER(STAT_VSDMCMovementSmoothClientPosition_Visual);

    if (HasValidData())
    {
        MoveSmooth_Visual();
    }
}

void UVSmoothDeltaMovementComponent::SmoothCorrection(float NewServerSimulationTime)
{
    SCOPE_CYCLE_COUNTER(STAT_VSDMCMovementSmoothCorrection);

    if (! HasValidData())
    {
        return;
    }

    // We shouldn't be running this on a standalone or dedicated server net mode
    checkSlow(GetNetMode() != NM_DedicatedServer);
    checkSlow(GetNetMode() != NM_Standalone);

    // Only client proxies should run this code.
    const bool bIsSimulatedProxy = (SDOwner->Role == ROLE_SimulatedProxy);

    ensure(bIsSimulatedProxy);

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
        //UpdatedComponent->SetWorldLocationAndRotation(NewLocation, NewRotation);
        //bNetworkSmoothingComplete = true;
    }
    else
    {
        const UWorld* MyWorld = GetWorld();

        if (!ensure(MyWorld != nullptr))
        {
            return;
        }

        // Get clamped smooth simulation offset from client - server simulation time difference
        SmoothSimulationOffset = GetClampedSmoothSimulationOffset(CurrentSimulationTime - NewServerSimulationTime);

        if (NetworkSmoothingMode == ENetworkSmoothingMode::Linear)
        {
        //    ClientData->OriginalMeshTranslationOffset   = ClientData->MeshTranslationOffset;

        //    // Remember the current and target rotation, we're going to lerp between them
        //    ClientData->OriginalMeshRotationOffset      = OldRotation;
        //    ClientData->MeshRotationOffset              = OldRotation;
        //    ClientData->MeshRotationTarget              = NewRotation;

        //    // Move the capsule, but not the mesh.
        //    // Note: we don't change rotation, we lerp towards it in SmoothClientPosition.
        //    const FScopedPreventAttachedComponentMove PreventMeshMove(SDOwner->GetMeshRoot());
        //    UpdatedComponent->SetWorldLocation(NewLocation);
        }
        else
        {
            const FScopedPreventAttachedComponentMove PreventMeshMove(SDOwner->GetMeshRoot());

            // Snap updated component to current time transform without moving attached mesh root
            CurrentSimulationTime = GetClampedSimulationTime(NewServerSimulationTime);
            SnapToCurrentTime();
        }

        //////////////////////////////////////////////////////////////////////////
        // Update smoothing timestamps

        //{
        //    // If running ahead, pull back slightly. This will cause the next delta to seem slightly longer, and cause us to lerp to it slightly slower.
        //    if (ClientData->SmoothingClientTimeStamp > ClientData->SmoothingServerTimeStamp)
        //    {
        //        const double OldClientTimeStamp = ClientData->SmoothingClientTimeStamp;
        //        ClientData->SmoothingClientTimeStamp = FMath::LerpStable(ClientData->SmoothingServerTimeStamp, OldClientTimeStamp, 0.5);

        //        //UE_LOG(LogVSDMCNetSmoothing, VeryVerbose,
        //        //    TEXT("SmoothCorrection: Pull back client from ClientTimeStamp: %.6f to %.6f, ServerTimeStamp: %.6f for %s"),
        //        //    OldClientTimeStamp, ClientData->SmoothingClientTimeStamp, ClientData->SmoothingServerTimeStamp, *GetNameSafe(SDOwner));
        //    }

        //    // Using server timestamp lets us know how much time actually elapsed, regardless of packet lag variance.
        //    double OldServerTimeStamp = ClientData->SmoothingServerTimeStamp;

        //    ClientData->SmoothingServerTimeStamp = bIsSimulatedProxy
        //        ? SDOwner->GetReplicatedLastTransformUpdateTimeStamp()
        //        : ServerLastTransformUpdateTimeStamp;

        //    // Initial update has no delta.
        //    if (ClientData->LastCorrectionTime == 0)
        //    {
        //        ClientData->SmoothingClientTimeStamp = ClientData->SmoothingServerTimeStamp;
        //        OldServerTimeStamp = ClientData->SmoothingServerTimeStamp;
        //    }

        //    // Don't let the client fall too far behind or run ahead of new server time.
        //    const double ServerDeltaTime = ClientData->SmoothingServerTimeStamp - OldServerTimeStamp;
        //    const double MaxDelta = FMath::Clamp(ServerDeltaTime * 1.25, 0.0, ClientData->MaxMoveDeltaTime * 2.0);

        //    ClientData->SmoothingClientTimeStamp = FMath::Clamp(
        //        ClientData->SmoothingClientTimeStamp,
        //        ClientData->SmoothingServerTimeStamp - MaxDelta,
        //        ClientData->SmoothingServerTimeStamp);

        //    // Compute actual delta between new server timestamp and client simulation.
        //    ClientData->LastCorrectionDelta = ClientData->SmoothingServerTimeStamp - ClientData->SmoothingClientTimeStamp;
        //    ClientData->LastCorrectionTime = MyWorld->GetTimeSeconds();

        //    //UE_LOG(LogVSDMCNetSmoothing, VeryVerbose,
        //    //    TEXT("SmoothCorrection: WorldTime: %.6f, ServerTimeStamp: %.6f, ClientTimeStamp: %.6f, Delta: %.6f for %s"),
        //    //    MyWorld->GetTimeSeconds(),
        //    //    ClientData->SmoothingServerTimeStamp,
        //    //    ClientData->SmoothingClientTimeStamp,
        //    //    ClientData->LastCorrectionDelta,
        //    //    *GetNameSafe(SDOwner));
        //}

//#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
//        if (VSDMCMovementCVars::NetVisualizeSimulatedCorrections >= 2)
//        {
//            const float Radius      = 4.0f;
//            const bool  bPersist    = false;
//            const float Lifetime    = 10.0f;
//            const int32 Sides       = 8;
//            const float ArrowSize   = 4.0f;
//
//            const FVector SimulatedLocation = OldLocation;
//            const FVector ServerLocation    = NewLocation + FVector( 0, 0, 0.5f );
//
//            const FVector SmoothLocation    = SDOwner->GetMeshRoot()->GetComponentLocation() - SDOwner->GetBaseTranslationOffset() + FVector(0, 0, 1.0f);
//
//            //DrawDebugCoordinateSystem( GetWorld(), ServerLocation + FVector( 0, 0, 300.0f ), UpdatedComponent->GetComponentRotation(), 45.0f, bPersist, Lifetime );
//
//            // Draw simulated location
//            DrawCircle(GetWorld(), SimulatedLocation, FVector(1, 0, 0), FVector(0, 1, 0), FColor(255, 0, 0, 255), Radius, Sides, bPersist, Lifetime);
//
//            // Draw server (corrected location)
//            DrawCircle(GetWorld(), ServerLocation, FVector(1, 0, 0), FVector(0, 1, 0), FColor(0, 255, 0, 255), Radius, Sides, bPersist, Lifetime);
//
//            // Draw smooth simulated location
//            FRotationMatrix SmoothMatrix(SDOwner->GetMeshRoot()->GetComponentRotation());
//            DrawDebugDirectionalArrow(GetWorld(), SmoothLocation, SmoothLocation + SmoothMatrix.GetScaledAxis(EAxis::Y) * 5, ArrowSize, FColor(255, 255, 0, 255), bPersist, Lifetime);
//            DrawCircle(GetWorld(), SmoothLocation, FVector(1, 0, 0), FVector(0, 1, 0), FColor(0, 0, 255, 255), Radius, Sides, bPersist, Lifetime);
//
//            if (ClientData->LastServerLocation_Debug != FVector::ZeroVector)
//            {
//                // Arrow showing simulated line
//                DrawDebugDirectionalArrow(GetWorld(), ClientData->LastServerLocation_Debug, SimulatedLocation, ArrowSize, FColor(255, 0, 0, 255), bPersist, Lifetime);
//                
//                // Arrow showing server line
//                DrawDebugDirectionalArrow(GetWorld(), ClientData->LastServerLocation_Debug, ServerLocation, ArrowSize, FColor(0, 255, 0, 255), bPersist, Lifetime);
//                
//                // Arrow showing smooth location plot
//                DrawDebugDirectionalArrow(GetWorld(), ClientData->LastSmoothLocation_Debug, SmoothLocation, ArrowSize, FColor(0, 0, 255, 255), bPersist, Lifetime);
//
//                // Line showing correction
//                DrawDebugDirectionalArrow(GetWorld(), SimulatedLocation, ServerLocation, ArrowSize, FColor(128, 0, 0, 255), bPersist, Lifetime);
//    
//                // Line showing smooth vector
//                DrawDebugDirectionalArrow(GetWorld(), ServerLocation, SmoothLocation, ArrowSize, FColor(0, 0, 128, 255), bPersist, Lifetime);
//            }
//
//            ClientData->LastServerLocation_Debug = ServerLocation;
//            ClientData->LastSmoothLocation_Debug = SmoothLocation;
//        }
//#endif
    }
}

void UVSmoothDeltaMovementComponent::ResetClientSmoothingData()
{
    SmoothSimulationOffset = 0.f;
    //LastCorrectionDelta = 0.f;
    //LastCorrectionTime = 0.f;
    //SmoothingServerTimeStamp = 0.f;
    //SmoothingClientTimeStamp = 0.f;
}

void UVSmoothDeltaMovementComponent::OnMovementUpdated(float DeltaTime, const FVector& OldLocation, const FVector& OldVelocity)
{
    // Blank implementation, intended for derived classes to override.
}

void UVSmoothDeltaMovementComponent::CallMovementUpdateDelegate(float DeltaTime, const FVector& OldLocation, const FVector& OldVelocity)
{
    SCOPE_CYCLE_COUNTER(STAT_VSDMCMoveUpdateDelegate);

    // Update component velocity in case events want to read it
    //UpdateComponentVelocity();

    // Delegate (for blueprints)
    //if (SDOwner)
    //{
    //    SDOwner->OnMovementUpdated.Broadcast(DeltaTime, OldLocation, OldVelocity);
    //}
}

void UVSmoothDeltaMovementComponent::PrimitiveTouched(UPrimitiveComponent* OverlappedComp, AActor* Other, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult )
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

        if (bTouchForceScaledToMass)
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

void UVSmoothDeltaMovementComponent::ApplyRepulsionForce(float DeltaTime)
{
    if (UpdatedPrimitive && RepulsionForce > 0.0f && SDOwner != nullptr)
    {
        const TArray<FOverlapInfo>& Overlaps = UpdatedPrimitive->GetOverlapInfos();
        if (Overlaps.Num() > 0)
        {
            FCollisionQueryParams QueryParams (SCENE_QUERY_STAT(CMC_ApplyRepulsionForce));
            QueryParams.bReturnFaceIndex = false;
            QueryParams.bReturnPhysicalMaterial = false;

            const float CollisionRadius = SDOwner->GetSimpleCollisionRadius();
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
                    UE_LOG(LogVSDMC, Warning, TEXT("%s could not find overlap body for body index %d"), *GetName(), OverlapBodyIndex);
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

void UVSmoothDeltaMovementComponent::ApplyImpactPhysicsForces(const FHitResult& Impact, const FVector& ImpactVelocity)
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
                const FVector VirtualVelocity = ImpactVelocity;

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

void UVSmoothDeltaMovementComponent::ApplyAccumulatedForces(float DeltaTime)
{
    // Blank implementation, intended for derived classes to override.
}

void UVSmoothDeltaMovementComponent::ClearAccumulatedForces()
{
    PendingImpulseToApply = FVector::ZeroVector;
    PendingForceToApply = FVector::ZeroVector;
}

FVector UVSmoothDeltaMovementComponent::GetPenetrationAdjustment(const FHitResult& Hit) const
{
    FVector Result = Super::GetPenetrationAdjustment(Hit);

    if (SDOwner)
    {
        const bool bIsProxy = (SDOwner->Role == ROLE_SimulatedProxy);
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

bool UVSmoothDeltaMovementComponent::ResolvePenetrationImpl(const FVector& Adjustment, const FHitResult& Hit, const FQuat& NewRotation)
{
    // If peneration resolving movement occurs, mark that we teleported,
    // so we don't incorrectly adjust velocity based on a potentially very different movement than our movement direction.
    bJustTeleported |= Super::ResolvePenetrationImpl(Adjustment, Hit, NewRotation);
    return bJustTeleported;
}

void UVSmoothDeltaMovementComponent::HandleImpact(const FHitResult& Impact, float TimeSlice, const FVector& MoveDelta)
{
    if (bEnablePhysicsInteraction)
    {
        ApplyImpactPhysicsForces(Impact, Velocity);
    }
}

float UVSmoothDeltaMovementComponent::SlideAlongSurface(const FVector& Delta, float Time, const FVector& InNormal, FHitResult& Hit, bool bHandleImpact)
{
    if (! Hit.bBlockingHit)
    {
        return 0.f;
    }

    return Super::SlideAlongSurface(Delta, Time, InNormal, Hit, bHandleImpact);
}

bool UVSmoothDeltaMovementComponent::HasValidData() const
{
    return UpdatedComponent && IsValid(SDOwner);;
}

bool UVSmoothDeltaMovementComponent::ShouldCancelAdaptiveReplication() const
{
    // Update sooner if important properties changed.
    const bool bVelocityChanged = (Velocity != LastUpdateVelocity);
    const bool bLocationChanged = (UpdatedComponent->GetComponentLocation() != LastUpdateLocation);
    const bool bRotationChanged = (UpdatedComponent->GetComponentQuat() != LastUpdateRotation);

    return (bVelocityChanged || bLocationChanged || bRotationChanged);
}

float UVSmoothDeltaMovementComponent::GetClampedSmoothSimulationOffset(float InSimulationTimeDelta) const
{
    // Return zeroed offset, intended for derived classes to override.
    return 0.f;
}

float UVSmoothDeltaMovementComponent::GetClampedSimulationTime(float InSimulationTime) const
{
    return (InSimulationTime < Duration) ? FMath::Clamp(InSimulationTime, 0.f, Duration) : FMath::Max(0.f, InSimulationTime-Duration);
}
