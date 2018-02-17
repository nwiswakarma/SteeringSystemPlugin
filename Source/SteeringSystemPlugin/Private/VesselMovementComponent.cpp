// 

#include "VesselMovementComponent.h"
#include "ControlInputComponent.h"
#include "RVO3DAgentComponent.h"
#include "GameFramework/Controller.h"

UVesselMovementComponent::UVesselMovementComponent(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    bWantsInitializeComponent = true;

    // Linear velocity
    MaxSpeed = 1200.f;
    Acceleration = 4000.f;
    Deceleration = 8000.f;

    // Orientation
    bUseTurnRange = false;
    TurnRate = 45.f;
    MinTurnRate = -1.f;
    TurnAcceleration = 45.f;
    CurrentTurnRate = 0.f;

    bPositionCorrected = false;
    bAutoRegisterControlComponent = true;
    bAutoRegisterRVOAgentComponent = true;

    bUseRVOAvoidance = true;
}

void UVesselMovementComponent::InitializeComponent()
{
    Super::InitializeComponent();

    if (! ControlInputComponent && bAutoRegisterControlComponent)
    {
        // Auto-register owner's root component if found.
        if (AActor* MyActor = GetOwner())
        {
            UControlInputComponent* NewControlComponent = MyActor->FindComponentByClass<UControlInputComponent>();

            if (NewControlComponent)
            {
                SetControlInputComponent(NewControlComponent);
            }
        }
    }

    if (! RVOAgentComponent && bAutoRegisterRVOAgentComponent)
    {
        // Auto-register owner's root component if found.
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

void UVesselMovementComponent::OnRegister()
{
    Super::OnRegister();

    if (! ControlInputComponent && bAutoRegisterControlComponent)
    {
        // Auto-register owner's root component if found.
        if (AActor* MyActor = GetOwner())
        {
            UControlInputComponent* NewControlComponent = MyActor->FindComponentByClass<UControlInputComponent>();

            if (NewControlComponent)
            {
                SetControlInputComponent(NewControlComponent);
            }
        }
    }

    if (! RVOAgentComponent && bAutoRegisterRVOAgentComponent)
    {
        // Auto-register owner's root component if found.
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

void UVesselMovementComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
    if (ShouldSkipUpdate(DeltaTime))
    {
        return;
    }

    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (! IsValid(ControlInputComponent) && ControlInputComponent->IsPendingKill())
    {
        SetControlInputComponent(nullptr);
    }

    //const AController* Controller = PawnOwner->GetController();
    //if (Controller && Controller->IsLocalController())
    {
        // Apply input for local players but also for AI that's not following a navigation path at the moment
        //if (Controller->IsLocalPlayerController() == true || Controller->IsFollowingAPath() == false)
        {
            ApplyControlInputToVelocity(DeltaTime);
        }

        // Reset flags
        bPositionCorrected = false;

        // Calculate delta velocity
        FVector Delta = Velocity * DeltaTime;

        if (! Delta.IsNearlyZero())
        {
            const FVector OldLocation = UpdatedComponent->GetComponentLocation();
            const FQuat Rotation = UpdatedComponent->GetComponentQuat();

            FHitResult Hit(1.f);
            SafeMoveUpdatedComponent(Delta, Rotation, true, Hit);

            if (Hit.IsValidBlockingHit())
            {
                HandleImpact(Hit, DeltaTime, Delta);
                // Try to slide the remaining distance along the surface.
                SlideAlongSurface(Delta, 1.f-Hit.Time, Hit.Normal, Hit, true);
            }

            // Update velocity unless there is a collision position correction
            // We don't want position changes to vastly reverse our direction (which can happen due to penetration fixups etc)
            if (! bPositionCorrected)
            {
                const FVector NewLocation = UpdatedComponent->GetComponentLocation();
                Velocity = ((NewLocation - OldLocation) / DeltaTime);
            }
        }

        // Rotate updated component
        MoveUpdatedComponent(FVector::ZeroVector, VelocityOrientation, true);
        
        // Finalize
        UpdateComponentVelocity();
    }
}

void UVesselMovementComponent::ApplyControlInputToVelocity(float DeltaTime)
{
    if (! ControlInputComponent || ControlInputComponent->IsMoveInputIgnored_Direct())
    {
        return;
    }

    check(UpdatedComponent);

    // Calculate RVO if enabled
    if (bUseRVOAvoidance)
    {
        CalcAvoidanceVelocity();
    }

    const bool bEnableAcceleration = ControlInputComponent->IsAccelerationEnabled_Direct();

    const FVector ControlInput( GetControlInput() );
    const float AnalogInputModifier = (ControlInput.SizeSquared() > 0.f ? ControlInput.Size() : 0.f);

    // Component and input max speed
    const float CompMaxSpeed = GetMaxSpeed();
    const float InputMaxSpeed = CompMaxSpeed * AnalogInputModifier;

    if (AnalogInputModifier > 0.f)
    {
        VelocityOrientation = UpdatedComponent->GetComponentQuat();

        // Orient velocity towards control input
        {
            const FVector ForwardNormal = VelocityOrientation.GetForwardVector();
            const FVector InputNormal = ControlInput.GetUnsafeNormal();

            // Interpolate velocity direction
            if (! FVector::Coincident(ForwardNormal, InputNormal))
            {
                PrepareTurnRange(DeltaTime, ForwardNormal, InputNormal);
                InterpOrientation(VelocityOrientation, ForwardNormal, InputNormal, DeltaTime, CurrentTurnRate);
            }
            // Set direction directly if above dot threshold
            else
            {
                ClearTurnRange();
                InterpOrientation(VelocityOrientation, ForwardNormal, InputNormal, DeltaTime, CurrentTurnRate);
            }
        }

        const FVector VelocityDirection(VelocityOrientation.GetForwardVector());

        // Accelerate if acceleration is enabled
        if (bEnableAcceleration)
        {
            const float CurrentSpeed = FMath::Min(Velocity.Size(), CompMaxSpeed);
            const float CurrentMaxSpeed = (InputMaxSpeed < CompMaxSpeed) ? InputMaxSpeed : CompMaxSpeed;

            const float DecelerationDelta = FMath::Abs(Deceleration) * DeltaTime;
            const float DecelerationThreshold = CurrentMaxSpeed + DecelerationDelta;

            // Current speed has yet reached max speed, accelerate
            if (CurrentSpeed < DecelerationThreshold)
            {
                const float AccelerationSpeed = CurrentSpeed + FMath::Abs(Acceleration)*DeltaTime;
                Velocity = VelocityDirection * FMath::Min(AccelerationSpeed, CurrentMaxSpeed);
            }
            // Current speed has reached over max speed, decelerate to match
            else
            {
                const float DecelerationSpeed = FMath::Max(Velocity.Size() - DecelerationDelta, 0.f);
                Velocity = VelocityDirection * FMath::Max(DecelerationSpeed, CurrentMaxSpeed);
            }
        }
        // Otherwise, decelerate
        else
        {
            Decelerate(DeltaTime);
        }
    }
    // No control input, brake with the current direction
    else
    {
        Decelerate(DeltaTime);
    }

    ControlInputComponent->ConsumeInputVector_Direct();
}

void UVesselMovementComponent::Decelerate(float DeltaTime)
{
    const float DecelerationDelta = FMath::Abs(Deceleration) * DeltaTime;

    // Dampen velocity magnitude based on deceleration.
    if (Velocity.SizeSquared() > DecelerationDelta)
    {
        const float DecelerationSpeed = FMath::Max(Velocity.Size() - DecelerationDelta, 0.f);
        Velocity = Velocity.GetSafeNormal() * DecelerationSpeed;
    }
    // Velocity too low, set to zero
    else
    {
        Velocity = FVector::ZeroVector;
    }
}

bool UVesselMovementComponent::ResolvePenetrationImpl(const FVector& Adjustment, const FHitResult& Hit, const FQuat& NewRotationQuat)
{
    bPositionCorrected |= Super::ResolvePenetrationImpl(Adjustment, Hit, NewRotationQuat);
    return bPositionCorrected;
}

void UVesselMovementComponent::CalcAvoidanceVelocity()
{
    if (! bUseRVOAvoidance || ! RVOAgentComponent)
    {
        return;
    }

    //if (MyOwner->Role != ROLE_Authority)
    //{
    //    return;
    //}

    // Perform RVO calculation if currently moving
    if (Velocity.SizeSquared() > KINDA_SMALL_NUMBER)
    {
        // Calculate RVO if not already performing locked avoidance
        if (! RVOAgentComponent->HasLockedPreferredVelocity())
        {
            if (RVOAgentComponent->HasAvoidanceVelocity())
            {
                // Had to divert course, lock this avoidance move in for a short time. This will make us a VO, so unlocked others will know to avoid us.
                RVOAgentComponent->LockPreferredVelocity(Velocity);
            }
        }
    }
}

void UVesselMovementComponent::RefreshTurnRange()
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

void UVesselMovementComponent::PrepareTurnRange(float DeltaTime, const FVector& VelocityNormal, const FVector& InputNormal)
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

void UVesselMovementComponent::ClearTurnRange()
{
    if (bUseTurnRange && CurrentTurnRate > MinTurnRate)
    {
        CurrentTurnRate = MinTurnRate;
        LastTurnDelta = FVector::ZeroVector;
    }
}

void UVesselMovementComponent::InterpOrientation(FQuat& Orientation, const FVector& Current, const FVector& Target, float DeltaTime, float RotationSpeedDegrees)
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

float UVesselMovementComponent::GetTurnRate() const
{
    return TurnRate;
}

float UVesselMovementComponent::GetMinTurnRate() const
{
    return MinTurnRate;
}

float UVesselMovementComponent::GetTurnAcceleration() const
{
    return TurnAcceleration;
}

void UVesselMovementComponent::SetTurnRate(float InTurnRate)
{
    TurnRate = FMath::Max(InTurnRate, 0.f);
    RefreshTurnRange();
}

void UVesselMovementComponent::SetMinTurnRate(float InMinTurnRate)
{
    MinTurnRate = InMinTurnRate;
    RefreshTurnRange();
}

void UVesselMovementComponent::SetTurnAcceleration(float InTurnAcceleration)
{
    TurnAcceleration = InTurnAcceleration;
    RefreshTurnRange();
}

void UVesselMovementComponent::SetControlInputComponent(UControlInputComponent* InControlInputComponent)
{
    // Don't assign pending kill components, but allow those to null out previous value
    ControlInputComponent = IsValid(InControlInputComponent) ? InControlInputComponent : nullptr;

    if (ControlInputComponent && ControlInputComponent != InControlInputComponent)
    {
        ControlInputComponent->ConsumeInputVector_Direct();
    }
}

void UVesselMovementComponent::SetRVOAgentComponent(URVO3DAgentComponent* InRVOAgentComponent)
{
    // Don't assign pending kill components, but allow those to null out previous value
    RVOAgentComponent = IsValid(InRVOAgentComponent) ? InRVOAgentComponent : nullptr;
}

FVector UVesselMovementComponent::GetControlInput() const
{
    check(ControlInputComponent);

    // Override input with RVO if required
    if (bUseRVOAvoidance && RVOAgentComponent && RVOAgentComponent->HasLockedPreferredVelocity())
    {
        const FVector AvoidanceVelocity( RVOAgentComponent->GetAvoidanceVelocity() );
        const float CompMaxSpeed = GetMaxSpeed();
        const float AvoidanceMagnitude = CompMaxSpeed>0.f ? (AvoidanceVelocity.Size() / CompMaxSpeed) : 0.f;
        return AvoidanceVelocity.GetSafeNormal() * AvoidanceMagnitude;
    }

    return ControlInputComponent->GetPendingInputVector_Direct().GetClampedToMaxSize(1.f);
}
