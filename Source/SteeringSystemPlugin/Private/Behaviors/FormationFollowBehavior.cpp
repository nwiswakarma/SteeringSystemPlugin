// 

#include "Behaviors/FormationFollowBehavior.h"
#include "SteeringFormation.h"
#include "UnrealMathUtility.h"

void FFormationFollowBehavior::SetSteerable(ISteerable* InSteerable)
{
    FSteeringBehavior::SetSteerable(InSteerable);
    // Set behavior delegate steerable
    RegroupBehavior.SetSteerable(InSteerable);
    CheckpointBehavior.SetSteerable(InSteerable);
    ArriveBehavior.SetSteerable(InSteerable);
    AlignBehavior.SetSteerable(InSteerable);
}

void FFormationFollowBehavior::OnActivated()
{
    RegroupBehavior.Activate();
    CheckpointBehavior.Activate();
    ArriveBehavior.Activate();
    AlignBehavior.Activate();
}

void FFormationFollowBehavior::OnDeactivated()
{
    RegroupBehavior.Deactivate();
    CheckpointBehavior.Deactivate();
    ArriveBehavior.Deactivate();
    AlignBehavior.Deactivate();

    MarkFinished();
}

bool FFormationFollowBehavior::CalculateRegroupSteering(float DeltaTime, FSteeringAcceleration& ControlInput)
{
    const FVector SrcLocation(Steerable->GetSteerableLocation());
    FVector& DstLocation(RegroupBehavior.SteeringTarget.Location);

    CalculateCurrentSlotLocation(DstLocation);

    const FVector DstDelta = DstLocation-SrcLocation;
    const float DstDeltaDist = DstDelta.Size();

    bool bLimitVelocity = false;

    if (DstDeltaDist > 50.f && DstDeltaDist < 150.f)
    {
        FVector FinalLocation;
        CalculateTargetSlotLocation(FinalLocation);

        const FVector DstDeltaDir = DstDelta.GetSafeNormal();

        const FVector FinalDelta = FinalLocation-SrcLocation;
        const FVector FinalDeltaDir = FinalDelta.GetSafeNormal();

        // Currently backfacing regroup slot location,
        // move towards next checkpoint and limit velocity
        if ((DstDeltaDir | FinalDeltaDir) < 0.f)
        {
            CalculateCheckpoint(DstLocation);
            bLimitVelocity = true;
        }
    }

    bool bRegrouped = RegroupBehavior.CalculateSteering(DeltaTime, ControlInput);

    if (! bRegrouped && bLimitVelocity)
    {
        ClampMaxInput(ControlInput, GetMaxSpeed()*MinVelocityLimit);
    }

    return bRegrouped;
}

bool FFormationFollowBehavior::CalculateSteeringImpl(float DeltaTime, FSteeringAcceleration& ControlInput)
{
    check(HasValidData());

    FSteeringFormation& Formation(*Steerable->GetFormation());
    float VelocityLimit, ReachTime;

    // Assign regroup target location
    bool bRegrouped = CalculateRegroupSteering(DeltaTime, ControlInput);

    // Make sure to regroup before performing further movement
    if (! bRegrouped)
    {
        return false;
    }

    // Assign checkpoint target location
    {
        FVector& SlotLocation(CheckpointBehavior.SteeringTarget.Location);

        ReachTime = CalculateCheckpoint(SlotLocation);
        CalculateVelocityLimit(SlotLocation, ReachTime, VelocityLimit);
    }
    bool bCheckpointPassed = CheckpointBehavior.CalculateSteering(DeltaTime, ControlInput);

    // Make sure pass checkpoint before performing further movement
    if (! bCheckpointPassed)
    {
        ClampMaxInput(ControlInput, VelocityLimit);
        return false;
    }

    // Assign arrival target location
    {
        FVector& SlotLocation(ArriveBehavior.SteeringTarget.Location);
        Formation.CalculateSlotLocation(Steerable, SlotLocation, SteeringTarget.GetLocation());
    }

    bool bArrived = ArriveBehavior.CalculateSteering(DeltaTime, ControlInput);
    bool bAligned = false;

    // Clamp max input
    ClampMaxInput(ControlInput, VelocityLimit);

    if (bArrived)
    {
        FQuat& SlotOrientation(AlignBehavior.SteeringTarget.Rotation);
        Formation.CalculateSlotOrientation(Steerable, SlotOrientation);

        bAligned = AlignBehavior.CalculateSteering(DeltaTime, ControlInput);
    }

    const bool bFinished = (bArrived && bAligned);

    if (bFinished)
    {
        (*bBehaviorFinished) = true;
    }

    return bFinished;
}

float FFormationFollowBehavior::CalculateCheckpoint(FVector& SlotLocation, float InReachInterval)
{
    check(HasValidData());

    FSteeringFormation& Formation(*Steerable->GetFormation());
    const FVector TargetLocation(SteeringTarget.GetLocation());

    FVector AnchorLocation = Formation.GetAnchorLocation();
    FVector TargetOffset;

    const float MaxSpeed = GetMaxSpeed();
    const float ReachSpeed = MaxSpeed * InReachInterval;

    const FVector FinalDeltaLocation = (TargetLocation-AnchorLocation);
    const float FinalDeltaDist = FinalDeltaLocation.Size();

    float ReachTime = 1.f;

    if (ReachSpeed < FinalDeltaDist)
    {
        FVector ForwardVector = FinalDeltaLocation.GetSafeNormal();
        TargetOffset = AnchorLocation + (ForwardVector*ReachSpeed);
    }
    else
    {
        TargetOffset = TargetLocation;

        if (ReachSpeed > KINDA_SMALL_NUMBER)
        {
            ReachTime = FinalDeltaDist/ReachSpeed;
        }
    }

    Formation.CalculateSlotLocation(Steerable, SlotLocation, TargetOffset);

    return ReachTime;
}

void FFormationFollowBehavior::CalculateVelocityLimit(const FVector& SlotLocation, float InReachInterval, float InReachTime, float& VelocityLimit)
{
    check(HasValidData());

    const FVector CurrentLocation = Steerable->GetSteerableLocation();
    const FVector DeltaLocation = (SlotLocation-CurrentLocation);
    const float DeltaDist = DeltaLocation.Size();

    const float MaxSpeed = GetMaxSpeed();
    const float ReachSpeed = MaxSpeed * InReachInterval * InReachTime;

    float LimitRatio;

    if (ReachSpeed > KINDA_SMALL_NUMBER)
    {
        LimitRatio = DeltaDist/ReachSpeed;
    }
    else
    {
        LimitRatio = 1.f;
    }

    VelocityLimit = FMath::Clamp(LimitRatio, MinVelocityLimit, 1.f) * MaxSpeed;
}

void FFormationFollowBehavior::CalculateCurrentSlotLocation(FVector& SlotLocation)
{
    check(HasValidData());

    FSteeringFormation& Formation(*Steerable->GetFormation());
    Formation.CalculateSlotLocation(Steerable, SlotLocation);
}

void FFormationFollowBehavior::CalculateTargetSlotLocation(FVector& SlotLocation)
{
    check(HasValidData());

    FSteeringFormation& Formation(*Steerable->GetFormation());
    Formation.CalculateSlotLocation(Steerable, SlotLocation, SteeringTarget.GetLocation());
}

float FFormationFollowBehavior::GetMaxSpeed() const
{
    check(HasValidData());

    const FSteeringFormation& Formation(*Steerable->GetFormation());
    const float FormationVelocityLimit = Formation.GetVelocityLimit();

    return (FormationVelocityLimit > KINDA_SMALL_NUMBER)
        ? FormationVelocityLimit
        : Steerable->GetMaxLinearSpeed();
}

void FFormationFollowBehavior::ClampMaxInput(FSteeringAcceleration& ControlInput, float VelocityLimit) const
{
    FVector& Linear(ControlInput.Linear);
    float MaxSpeed = Steerable->GetMaxLinearSpeed();

    if (MaxSpeed > KINDA_SMALL_NUMBER)
    {
        float MaxInv = VelocityLimit/MaxSpeed;
        Linear = Linear.GetClampedToMaxSize(FMath::Min(MaxInv, 1.f));
    }
    else
    {
        Linear = FVector::ZeroVector;
    }
}
