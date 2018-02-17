// 

#include "VPawnChar.h"
#include "VPCMovementComponent.h"
#include "DisplayDebugHelpers.h"
#include "Animation/AnimInstance.h"
#include "Components/SkinnedMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/ArrowComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/Canvas.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/DamageType.h"
#include "Net/UnrealNetwork.h"

DEFINE_LOG_CATEGORY_STATIC(LogVPawnChar, Log, All);

FName AVPawnChar::MeshRootComponentName(TEXT("MeshRoot"));
FName AVPawnChar::MeshComponentName(TEXT("CharacterMesh0"));
FName AVPawnChar::CharacterMovementComponentName(TEXT("CharMoveComp"));
FName AVPawnChar::ShapeComponentName(TEXT("RootCollision"));

AVPawnChar::AVPawnChar(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
    // Structure to hold one-time initialization
    struct FConstructorStatics
    {
        FName ID_Characters;
        FText NAME_Characters;
        FConstructorStatics()
            : ID_Characters(TEXT("Characters"))
            , NAME_Characters(NSLOCTEXT("SpriteCategory", "Characters", "Characters"))
        {
        }
    };
    static FConstructorStatics ConstructorStatics;

    ShapeComponent = CreateDefaultSubobject<UShapeComponent>(AVPawnChar::ShapeComponentName);
    RootComponent = ShapeComponent;

#if WITH_EDITORONLY_DATA
    ArrowComponent = CreateEditorOnlyDefaultSubobject<UArrowComponent>(TEXT("Arrow"));
    if (ArrowComponent)
    {
        ArrowComponent->ArrowColor = FColor(150, 200, 255);
        ArrowComponent->bTreatAsASprite = true;
        ArrowComponent->SpriteInfo.Category = ConstructorStatics.ID_Characters;
        ArrowComponent->SpriteInfo.DisplayName = ConstructorStatics.NAME_Characters;
        ArrowComponent->bIsScreenSizeScaled = true;
        if (RootComponent)
        {
            ArrowComponent->SetupAttachment(RootComponent);
        }
    }
#endif // WITH_EDITORONLY_DATA

    CharacterMovement = CreateDefaultSubobject<UVPCMovementComponent>(AVPawnChar::CharacterMovementComponentName);
    if (CharacterMovement && RootComponent)
    {
        CharacterMovement->UpdatedComponent = RootComponent;
    }

    MeshRoot = CreateDefaultSubobject<USceneComponent>(AVPawnChar::MeshRootComponentName);
    MeshRoot->SetupAttachment(RootComponent);

    Mesh = CreateOptionalDefaultSubobject<USkeletalMeshComponent>(AVPawnChar::MeshComponentName);
    if (Mesh)
    {
        Mesh->AlwaysLoadOnClient = true;
        Mesh->AlwaysLoadOnServer = true;
        Mesh->bOwnerNoSee = false;
        Mesh->MeshComponentUpdateFlag = EMeshComponentUpdateFlag::AlwaysTickPose;
        Mesh->bCastDynamicShadow = true;
        Mesh->bAffectDynamicIndirectLighting = true;
        Mesh->PrimaryComponentTick.TickGroup = TG_PrePhysics;
        static FName MeshCollisionProfileName(TEXT("CharacterMesh"));
        Mesh->SetCollisionProfileName(MeshCollisionProfileName);
        Mesh->bGenerateOverlapEvents = false;
        Mesh->SetCanEverAffectNavigation(false);
        Mesh->SetupAttachment(MeshRoot);
    }

    BaseRotationOffset = FQuat::Identity;
}

void AVPawnChar::PostInitializeComponents()
{
    Super::PostInitializeComponents();

    if (!IsPendingKill())
    {
        CacheInitialMeshOffset(MeshRoot->RelativeLocation, MeshRoot->RelativeRotation);

        if (Mesh)
        {
            // force animation tick after movement component updates
            if (Mesh->PrimaryComponentTick.bCanEverTick && CharacterMovement)
            {
                Mesh->PrimaryComponentTick.AddPrerequisite(CharacterMovement, CharacterMovement->PrimaryComponentTick);
            }
        }

        if (GetNetMode() != NM_Client)
        {
            if (CharacterMovement)
            {
                CharacterMovement->SetDefaultMovementMode();
            }
        }
    }
}

void AVPawnChar::TornOff()
{
    Super::TornOff();

    if (CharacterMovement)
    {
        CharacterMovement->ResetPredictionData_Client();
        CharacterMovement->ResetPredictionData_Server();
    }

    // We're no longer controlled remotely, resume regular ticking of animations.
    if (Mesh)
    {
        Mesh->bOnlyAllowAutonomousTickPose = false;
    }
}

void AVPawnChar::DisplayDebug(UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos)
{
    Super::DisplayDebug(Canvas, DebugDisplay, YL, YPos);

    float Indent = 0.f;

    static FName NAME_Physics = FName(TEXT("Physics"));
    if (DebugDisplay.IsDisplayOn(NAME_Physics) )
    {
        FIndenter PhysicsIndent(Indent);

        FString BaseString;
        if ( CharacterMovement == NULL || BasedMovement.MovementBase == NULL )
        {
            BaseString = "Not Based";
        }
        else
        {
            BaseString = BasedMovement.MovementBase->IsWorldGeometry() ? "World Geometry" : BasedMovement.MovementBase->GetName();
            BaseString = FString::Printf(TEXT("Based On %s"), *BaseString);
        }
        
        FDisplayDebugManager& DisplayDebugManager = Canvas->DisplayDebugManager;
        DisplayDebugManager.DrawString(FString::Printf(TEXT("RelativeLoc: %s Rot: %s %s"), *BasedMovement.Location.ToCompactString(), *BasedMovement.Rotation.ToCompactString(), *BaseString), Indent);

        if ( CharacterMovement != NULL )
        {
            CharacterMovement->DisplayDebug(Canvas, DebugDisplay, YL, YPos);
        }
    }
}

void AVPawnChar::ClearCrossLevelReferences()
{
    if (BasedMovement.MovementBase != NULL && GetOutermost() != BasedMovement.MovementBase->GetOutermost())
    {
        SetBase(NULL);
    }

    Super::ClearCrossLevelReferences();
}

void AVPawnChar::CacheInitialMeshOffset(FVector MeshRelativeLocation, FRotator MeshRelativeRotation)
{
    BaseTranslationOffset = MeshRelativeLocation;
    BaseRotationOffset = MeshRelativeRotation.Quaternion();

#if ENABLE_NAN_DIAGNOSTIC
    if (BaseRotationOffset.ContainsNaN())
    {
        logOrEnsureNanError(TEXT("AVPawnChar::PostInitializeComponents detected NaN in BaseRotationOffset! (%s)"), *BaseRotationOffset.ToString());
    }
    if (Mesh->RelativeRotation.ContainsNaN())
    {
        logOrEnsureNanError(TEXT("AVPawnChar::PostInitializeComponents detected NaN in Mesh->RelativeRotation! (%s)"), *Mesh->RelativeRotation.ToString());
    }
#endif
}

UVPMovementComponent* AVPawnChar::GetMovementComponent() const
{
    return CharacterMovement;
}

UActorComponent* AVPawnChar::FindComponentByClass(const TSubclassOf<UActorComponent> ComponentClass) const
{
    // If the character has a Mesh, treat it as the first 'hit' when finding components
    if (Mesh && ComponentClass && Mesh->IsA(ComponentClass))
    {
        return Mesh;
    }

    return Super::FindComponentByClass(ComponentClass);
}

void AVPawnChar::SetReplicateMovement(bool bInReplicateMovement)
{
    Super::SetReplicateMovement(bInReplicateMovement);

    if (CharacterMovement != nullptr && Role == ROLE_Authority)
    {
        // Set prediction data time stamp to current time to stop extrapolating
        // from time bReplicateMovement was turned off to when it was turned on again
        FNetworkPredictionData_Server* NetworkPrediction = CharacterMovement->HasPredictionData_Server() ? CharacterMovement->GetPredictionData_Server() : nullptr;

        if (NetworkPrediction != nullptr)
        {
            NetworkPrediction->ServerTimeStamp = GetWorld()->GetTimeSeconds();
        }
    }
}

void AVPawnChar::SetBase(UPrimitiveComponent* NewBaseComponent, const FName InBoneName, bool bNotifyPawn)
{
    // If NewBaseComponent is null, ignore bone name.
    const FName BoneName = (NewBaseComponent ? InBoneName : NAME_None);

    // See what changed.
    const bool bBaseChanged = (NewBaseComponent != BasedMovement.MovementBase);
    const bool bBoneChanged = (BoneName != BasedMovement.BoneName);

    if (bBaseChanged || bBoneChanged)
    {
        // Verify no recursion
        AVPawn* Loop = (NewBaseComponent ? Cast<AVPawn>(NewBaseComponent->GetOwner()) : NULL);
        for ( ; Loop!=NULL; Loop=Cast<AVPawn>(Loop->GetMovementBase()))
        {
            if (Loop == this)
            {
                UE_LOG(LogVPawnChar, Warning,
                    TEXT(" SetBase failed! Recursion detected. Pawn %s already based on %s."),
                    *GetName(), *NewBaseComponent->GetName()); //-V595
                return;
            }
        }

        // Set base
        UPrimitiveComponent* OldBase = BasedMovement.MovementBase;
        BasedMovement.MovementBase = NewBaseComponent;
        BasedMovement.BoneName = BoneName;

        if (CharacterMovement)
        {
            const bool bBaseIsSimulating = NewBaseComponent && NewBaseComponent->IsSimulatingPhysics();
            if (bBaseChanged)
            {
                MovementBaseUtility::RemoveTickDependency(CharacterMovement->PrimaryComponentTick, OldBase);
                // We use a special post physics function if simulating, otherwise add normal tick prereqs.
                if (!bBaseIsSimulating)
                {
                    MovementBaseUtility::AddTickDependency(CharacterMovement->PrimaryComponentTick, NewBaseComponent);
                }
            }

            if (NewBaseComponent)
            {
                // Update OldBaseLocation/Rotation as those were referring to a different base
                // ... but not when handling replication for proxies (since they are going to copy this data from the replicated values anyway)
                if (!bInBaseReplication)
                {
                    // Force base location and relative position to be computed since we have a new base or bone so the old relative offset is meaningless.
                    CharacterMovement->SaveBaseLocation();
                }

                // Enable PostPhysics tick if we are standing on a physics object, as we need to to use post-physics transforms
                CharacterMovement->PostPhysicsTickFunction.SetTickFunctionEnable(bBaseIsSimulating);
            }
            else
            {
                BasedMovement.BoneName = NAME_None; // None, regardless of whether user tried to set a bone name, since we have no base component.
                BasedMovement.bRelativeRotation = false;
                CharacterMovement->PostPhysicsTickFunction.SetTickFunctionEnable(false);
            }

            if (Role == ROLE_Authority || Role == ROLE_AutonomousProxy)
            {
                BasedMovement.bServerHasBaseComponent = (BasedMovement.MovementBase != NULL); // Also set on proxies for nicer debugging.
                UE_LOG(LogVPawnChar, Verbose,
                    TEXT("Setting base on %s for '%s' to '%s'"),
                    Role == ROLE_Authority ? TEXT("Server") : TEXT("AutoProxy"), *GetName(), *GetFullNameSafe(NewBaseComponent));
            }
            else
            {
                UE_LOG(LogVPawnChar, Verbose, TEXT("Setting base on Client for '%s' to '%s'"), *GetName(), *GetFullNameSafe(NewBaseComponent));
            }
        }

        // Notify this actor of his new floor.
        if (bNotifyPawn)
        {
            BaseChange();
        }
    }
}

void AVPawnChar::SaveRelativeBasedMovement(const FVector& NewRelativeLocation, const FRotator& NewRotation, bool bRelativeRotation)
{
    checkSlow(BasedMovement.HasRelativeLocation());
    BasedMovement.Location = NewRelativeLocation;
    BasedMovement.Rotation = NewRotation;
    BasedMovement.bRelativeRotation = bRelativeRotation;
}

void AVPawnChar::BaseChange()
{
    // Blank implementation
}

void AVPawnChar::ApplyDamageMomentum(float DamageTaken, FDamageEvent const& DamageEvent, APawn* PawnInstigator, AActor* DamageCauser)
{
    UDamageType const* const DmgTypeCDO = DamageEvent.DamageTypeClass->GetDefaultObject<UDamageType>();
    float const ImpulseScale = DmgTypeCDO->DamageImpulse;

    if ( (ImpulseScale > 3.f) && (CharacterMovement != NULL) )
    {
        FHitResult HitInfo;
        FVector ImpulseDir;
        DamageEvent.GetBestHitInfo(this, PawnInstigator, HitInfo, ImpulseDir);

        FVector Impulse = ImpulseDir * ImpulseScale;
        bool const bMassIndependentImpulse = !DmgTypeCDO->bScaleMomentumByMass;

        CharacterMovement->AddImpulse(Impulse, bMassIndependentImpulse);
    }
}

//void AVPawnChar::LaunchCharacter(FVector LaunchVelocity, bool bXYOverride, bool bZOverride)
//{
//    UE_LOG(LogVPawnChar, Verbose, TEXT("AVPawnChar::LaunchCharacter '%s' (%f,%f,%f)"), *GetName(), LaunchVelocity.X, LaunchVelocity.Y, LaunchVelocity.Z);
//
//    if (CharacterMovement)
//    {
//        FVector FinalVel = LaunchVelocity;
//        const FVector Velocity = GetVelocity();
//
//        if (!bXYOverride)
//        {
//            FinalVel.X += Velocity.X;
//            FinalVel.Y += Velocity.Y;
//        }
//        if (!bZOverride)
//        {
//            FinalVel.Z += Velocity.Z;
//        }
//
//        CharacterMovement->Launch(FinalVel);
//
//        OnLaunched(LaunchVelocity, bXYOverride, bZOverride);
//    }
//}

void AVPawnChar::OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PrevCustomMode)
{
    K2_OnMovementModeChanged(PrevMovementMode, CharacterMovement->MovementMode, PrevCustomMode, CharacterMovement->CustomMovementMode);
    MovementModeChangedDelegate.Broadcast(this, PrevMovementMode, PrevCustomMode);
}

// ~ Replication

//
// Static variables for networking.
//
static uint8 VPawnChar_SavedMovementMode;

void AVPawnChar::PreNetReceive()
{
    VPawnChar_SavedMovementMode = ReplicatedMovementMode;
    Super::PreNetReceive();
}

void AVPawnChar::PostNetReceive()
{
    if (Role == ROLE_SimulatedProxy)
    {
        CharacterMovement->bNetworkUpdateReceived = true;
        CharacterMovement->bNetworkMovementModeChanged = (CharacterMovement->bNetworkMovementModeChanged || (VPawnChar_SavedMovementMode != ReplicatedMovementMode));
    }

    Super::PostNetReceive();
}

void AVPawnChar::OnRep_ReplicatedBasedMovement()
{
    if (Role != ROLE_SimulatedProxy)
    {
        return;
    }

    TGuardValue<bool> bInBaseReplicationGuard(bInBaseReplication, true);

    const bool bBaseChanged = (BasedMovement.MovementBase != ReplicatedBasedMovement.MovementBase || BasedMovement.BoneName != ReplicatedBasedMovement.BoneName);
    if (bBaseChanged)
    {
        // Even though we will copy the replicated based movement info, we need to use SetBase() to set up tick dependencies and trigger notifications.
        SetBase(ReplicatedBasedMovement.MovementBase, ReplicatedBasedMovement.BoneName);
    }

    // Make sure to use the values of relative location/rotation etc from the server.
    BasedMovement = ReplicatedBasedMovement;

    if (ReplicatedBasedMovement.HasRelativeLocation())
    {
        // Update transform relative to movement base
        const FVector OldLocation = GetActorLocation();
        const FQuat OldRotation = GetActorQuat();
        MovementBaseUtility::GetMovementBaseTransform(ReplicatedBasedMovement.MovementBase, ReplicatedBasedMovement.BoneName, CharacterMovement->OldBaseLocation, CharacterMovement->OldBaseQuat);
        const FVector NewLocation = CharacterMovement->OldBaseLocation + ReplicatedBasedMovement.Location;
        FRotator NewRotation;

        if (ReplicatedBasedMovement.HasRelativeRotation())
        {
            // Relative location, relative rotation
            NewRotation = (FRotationMatrix(ReplicatedBasedMovement.Rotation) * FQuatRotationMatrix(CharacterMovement->OldBaseQuat)).Rotator();
        }
        else
        {
            // Relative location, absolute rotation
            NewRotation = ReplicatedBasedMovement.Rotation;
        }

        // When position or base changes, movement mode will need to be updated. This assumes rotation changes don't affect that.
        CharacterMovement->bJustTeleported |= (bBaseChanged || GetActorLocation() != OldLocation);
        CharacterMovement->bNetworkSmoothingComplete = false;
        CharacterMovement->SmoothCorrection(OldLocation, OldRotation, NewLocation, NewRotation.Quaternion());
        OnUpdateSimulatedPosition(OldLocation, OldRotation);
    }
}

void AVPawnChar::OnUpdateSimulatedPosition(const FVector& OldLocation, const FQuat& OldRotation)
{
    CharacterMovement->bJustTeleported = true;
}

void AVPawnChar::PostNetReceiveLocationAndRotation()
{
    if (Role == ROLE_SimulatedProxy)
    {
        // Don't change transform if using relative position (it should be nearly the same anyway, or base may be slightly out of sync)
        if (!ReplicatedBasedMovement.HasRelativeLocation())
        {
            const FVector OldLocation = GetActorLocation();
            const FVector NewLocation = FRepMovement::RebaseOntoLocalOrigin(ReplicatedMovement.Location, this);
            const FQuat OldRotation = GetActorQuat();
        
            CharacterMovement->bNetworkSmoothingComplete = false;
            CharacterMovement->SmoothCorrection(OldLocation, OldRotation, NewLocation, ReplicatedMovement.Rotation.Quaternion());
            OnUpdateSimulatedPosition(OldLocation, OldRotation);
        }
    }
}

void AVPawnChar::PreReplication(IRepChangedPropertyTracker & ChangedPropertyTracker)
{
    Super::PreReplication(ChangedPropertyTracker);

    ReplicatedServerLastTransformUpdateTimeStamp = CharacterMovement->GetServerLastTransformUpdateTimeStamp();
    ReplicatedMovementMode = CharacterMovement->PackNetworkMovementMode();  
    ReplicatedBasedMovement = BasedMovement;

    // Optimization: only update and replicate these values if they are actually going to be used.
    if (BasedMovement.HasRelativeLocation())
    {
        // When velocity becomes zero, force replication so the position is updated to match the server (it may have moved due to simulation on the client).
        ReplicatedBasedMovement.bServerHasVelocity = !CharacterMovement->Velocity.IsZero();

        // Make sure absolute rotations are updated in case rotation occurred after the base info was saved.
        if (!BasedMovement.HasRelativeRotation())
        {
            ReplicatedBasedMovement.Rotation = GetActorRotation();
        }
    }
}

void AVPawnChar::PreReplicationForReplay(IRepChangedPropertyTracker & ChangedPropertyTracker)
{
    Super::PreReplicationForReplay(ChangedPropertyTracker);

    // If this is a replay, we save out certain values we need to runtime to do smooth interpolation
    // We'll be able to look ahead in the replay to have these ahead of time for smoother playback
    FVPCReplaySample ReplaySample;

    // If this is a client-recorded replay, use the mesh location and rotation, since these will always
    // be smoothed - unlike the actor position and rotation.
    const USkeletalMeshComponent* const MeshComponent = GetMesh();
    if (MeshComponent && GetWorld()->IsRecordingClientReplay())
    {
        // Remove the base transform from the mesh's transform, since on playback the base transform
        // will be stored in the mesh's RelativeLocation and RelativeRotation.
        const FTransform BaseTransform(GetBaseRotationOffset(), GetBaseTranslationOffset());
        const FTransform MeshRootTransform = BaseTransform.Inverse() * MeshComponent->GetComponentTransform();

        ReplaySample.Location = MeshRootTransform.GetLocation();
        ReplaySample.Rotation = MeshRootTransform.GetRotation().Rotator();
    }
    else
    {
        ReplaySample.Location = GetActorLocation();
        ReplaySample.Rotation = GetActorRotation();
    }

    ReplaySample.Velocity = GetVelocity();
    ReplaySample.Acceleration = CharacterMovement->GetCurrentAcceleration();

    FBitWriter Writer(0, true);
    Writer << ReplaySample;

    ChangedPropertyTracker.SetExternalData(Writer.GetData(), Writer.GetNumBits());
}

void AVPawnChar::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME_CONDITION( AVPawnChar, ReplicatedBasedMovement,   COND_SimulatedOnly );
    DOREPLIFETIME_CONDITION( AVPawnChar, ReplicatedMovementMode,    COND_SimulatedOnly );
    DOREPLIFETIME_CONDITION( AVPawnChar, ReplicatedServerLastTransformUpdateTimeStamp, COND_SimulatedOnlyNoReplay );

    // Change the condition of the replicated movement property to not replicate in replays since we handle this specifically
    // via saving this out in external replay data
    DOREPLIFETIME_CHANGE_CONDITION( AActor, ReplicatedMovement,     COND_SimulatedOrPhysicsNoReplay );
}

// ~ Anim Montage

float AVPawnChar::PlayAnimMontage(UAnimMontage* AnimMontage, float InPlayRate, FName StartSectionName)
{
    UAnimInstance* AnimInstance = (Mesh)? Mesh->GetAnimInstance() : NULL; 
    if (AnimMontage && AnimInstance)
    {
        float const Duration = AnimInstance->Montage_Play(AnimMontage, InPlayRate);

        if (Duration > 0.f)
        {
            // Start at a given Section.
            if (StartSectionName != NAME_None)
            {
                AnimInstance->Montage_JumpToSection(StartSectionName, AnimMontage);
            }

            return Duration;
        }
    }   

    return 0.f;
}

void AVPawnChar::StopAnimMontage(UAnimMontage* AnimMontage)
{
    UAnimInstance* AnimInstance = (Mesh) ? Mesh->GetAnimInstance() : NULL; 
    UAnimMontage* MontageToStop = (AnimMontage) ? AnimMontage : GetCurrentMontage();
    bool bShouldStopMontage =  AnimInstance && MontageToStop && !AnimInstance->Montage_GetIsStopped(MontageToStop);

    if (bShouldStopMontage)
    {
        AnimInstance->Montage_Stop(MontageToStop->BlendOut.GetBlendTime(), MontageToStop);
    }
}

UAnimMontage* AVPawnChar::GetCurrentMontage()
{
    UAnimInstance* AnimInstance = (Mesh) ? Mesh->GetAnimInstance() : NULL; 
    if (AnimInstance)
    {
        return AnimInstance->GetCurrentActiveMontage();
    }

    return NULL;
}

// ~ VPawnChar Specialization

AVPawnCharNoMesh::AVPawnCharNoMesh(const FObjectInitializer& ObjectInitializer)
: Super(
    ObjectInitializer
        .SetDefaultSubobjectClass<UCapsuleComponent>(AVPawnChar::ShapeComponentName)
        .DoNotCreateDefaultSubobject(AVPawnChar::MeshComponentName)
    )
{
    CapsuleComponent = CastChecked<UCapsuleComponent>(GetShapeComponent());
    CapsuleComponent->InitCapsuleSize(34.0f, 88.0f);
    CapsuleComponent->CanCharacterStepUpOn = ECB_No;
    CapsuleComponent->bShouldUpdatePhysicsVolume = true;
    CapsuleComponent->bCheckAsyncSceneOnMove = false;
    CapsuleComponent->bDynamicObstacle = true;
    CapsuleComponent->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);
    CapsuleComponent->SetCanEverAffectNavigation(false);
}

AVPawnCharNoMeshBox::AVPawnCharNoMeshBox(const FObjectInitializer& ObjectInitializer)
: Super(
    ObjectInitializer
        .SetDefaultSubobjectClass<UBoxComponent>(AVPawnChar::ShapeComponentName)
        .DoNotCreateDefaultSubobject(AVPawnChar::MeshComponentName)
    )
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
