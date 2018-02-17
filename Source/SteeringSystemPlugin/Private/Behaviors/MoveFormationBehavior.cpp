// 

#include "Behaviors/MoveFormationBehavior.h"
#include "Behaviors/FormationFollowBehavior.h"
#include "UnrealMathUtility.h"

void FMoveFormationBehavior::OnActivated()
{
    check(HasValidData());
    InitializeAnchor();
    UpdateBehaviorAssignments();
    UpdateFormationOrientation();
}

void FMoveFormationBehavior::OnDeactivated()
{
    MemberSet.Empty();
    FinishedBehaviors.Empty();

    if (HasValidData())
    {
        GetFormation().SetVelocityLimit(0.f);
    }
}

bool FMoveFormationBehavior::UpdateBehaviorState(float DeltaTime)
{
    int32 BehaviorIdx = 0;
    while (BehaviorIdx < FinishedBehaviors.Num())
    {
        if (*FinishedBehaviors[BehaviorIdx])
        {
            FinishedBehaviors.RemoveAtSwap(BehaviorIdx, 1, false);
            continue;
        }

        BehaviorIdx++;
    }

    return (FinishedBehaviors.Num() == 0);
}

bool FMoveFormationBehavior::CalculateSteeringImpl(float DeltaTime)
{
    check(HasValidData());

    MoveSpeedUpdateTimer += DeltaTime;

    if (MoveSpeedUpdateTimer > MoveSpeedUpdateInterval)
    {
        UpdateMoveSpeed(MoveSpeedUpdateTimer);
        MoveSpeedUpdateTimer = 0.f;
    }

    InterpMoveSpeed(DeltaTime);

    FSteeringFormation& Formation(GetFormation());

    if (Formation.IsPatternUpdateRequired())
    {
        UpdateBehaviorAssignments();
    }

    if (! bTargetReached)
    {
        UpdateAnchorMovement(DeltaTime);
    }

    const bool bFinished = UpdateBehaviorState(DeltaTime);
    return bFinished;
}

void FMoveFormationBehavior::UpdateMoveSpeed(float DeltaTime)
{
    check(HasValidData());

    FSteeringFormation& Formation(GetFormation());
    const TArray<ISteerable*>& Members(Formation.GetMembers());

    bool bFormationLockRefreshed = false;
    float MinSpeed = (Members.Num()>0) ? BIG_NUMBER : 0.f;

    if (FormationLockTimer > KINDA_SMALL_NUMBER)
    {
        FormationLockTimer -= DeltaTime;
    }

    for (ISteerable* Member : Members)
    {
        // Skip invalid members
        if (! Member)
        {
            continue;
        }

        const float MaxSpeed = Member->GetMaxLinearSpeed();

        // Find member with lowest maximum velocity
        if (MaxSpeed > KINDA_SMALL_NUMBER && MaxSpeed < MinSpeed)
        {
            MinSpeed = MaxSpeed;
        }

        // Lock timer already refreshed, continue looking for member
        // with lowest maximum velocity
        if (bFormationLockRefreshed)
        {
            continue;
        }

        // Check for lock timer refresh

        FVector SrcLocation = Member->GetSteerableLocation();
        FVector SlotLocation;

        Formation.CalculateSlotLocation(Member, SlotLocation);

        FVector SlotDelta = SlotLocation-SrcLocation;
        float DistFromSlot = SlotDelta.Size();

        // Find member furthest from current formation slot
        if (DistFromSlot > 150.f)
        {
            FormationLockTimer = FormationLockDelay;
            bFormationLockRefreshed = true;
        }
    }

    float AnchorSpeed = MinSpeed;

    if (FormationLockTimer > KINDA_SMALL_NUMBER)
    {
        AnchorSpeed = MinSpeed * .5f;

        // Dropping velocity limit is instant when caused by broken formation
        TargetVelocityLimit = AnchorSpeed;
        CurrentVelocityLimit = AnchorSpeed;
    }
    else
    {
        TargetVelocityLimit = AnchorSpeed;
    }

    // Calculate anchor movement speed based on distance from formation primary

    const ISteerable& Primary(*Formation.GetPrimary());
    const FVector PrimaryLocation(Primary.GetSteerableLocation());
    const FVector AnchorLocation(Formation.GetAnchorLocation());
    const float DistFromPrimarySq = (PrimaryLocation-AnchorLocation).SizeSquared();
    const float PrimaryInnerRadiusSq = PrimaryInnerRadius*PrimaryInnerRadius;
    const float PrimaryOuterRadiusSq = PrimaryOuterRadius*PrimaryOuterRadius;

    if (DistFromPrimarySq > PrimaryInnerRadiusSq)
    {
        AnchorSpeed *= .5f;
    }
    else if (DistFromPrimarySq > PrimaryOuterRadiusSq)
    {
        AnchorSpeed = 0.f;
    }

    TargetAnchorSpeed = AnchorSpeed;
}

void FMoveFormationBehavior::InterpMoveSpeed(float DeltaTime)
{
    check(HasValidData());

    CurrentAnchorSpeed = FMath::FInterpTo(
        CurrentAnchorSpeed,
        TargetAnchorSpeed,
        DeltaTime,
        AnchorInterpSpeed);

    CurrentVelocityLimit = FMath::FInterpTo(
        CurrentVelocityLimit,
        TargetVelocityLimit,
        DeltaTime,
        VelocityLimitInterpSpeed);

    GetFormation().SetVelocityLimit(CurrentVelocityLimit);
}

void FMoveFormationBehavior::InitializeAnchor()
{
    check(HasValidData());

    FSteeringFormation& Formation(GetFormation());

    const ISteerable& Primary(*Formation.GetPrimary());
    const FVector Location(Primary.GetSteerableLocation());
    const FQuat Orientation(Primary.GetSteerableOrientation());

    Formation.SetAnchorLocationAndOrientation(Location, Orientation);

    bTargetReached = false;
}

void FMoveFormationBehavior::UpdateAnchorMovement(float DeltaTime)
{
    check(HasValidData());

    // Not currently moving, skip anchor movement update
    if (CurrentAnchorSpeed < KINDA_SMALL_NUMBER)
    {
        return;
    }

    FSteeringFormation& Formation(GetFormation());

    const FVector SrcLocation = Formation.GetAnchorLocation();
    const FVector& DstLocation(SteeringTarget.Location);
    const FVector DeltaLocation = DstLocation-SrcLocation;

    const float DeltaDistSq = DeltaLocation.SizeSquared();
    const float TargetDistSq = TargetRadius*TargetRadius;

    FVector NewLocation;

    if (DeltaDistSq > TargetDistSq)
    {
        const FVector DeltaDirection = DeltaLocation.GetUnsafeNormal();
        const float AnchorSpeed = CurrentAnchorSpeed*DeltaTime;

        NewLocation = SrcLocation + DeltaDirection*AnchorSpeed;
    }
    // Snap to target location within target radius and mark finish
    else
    {
        NewLocation = DstLocation;
        bTargetReached = true;
    }

    Formation.SetAnchorLocation(NewLocation);
}

void FMoveFormationBehavior::UpdateBehaviorAssignments()
{
    check(HasValidData());

    FSteeringFormation& Formation(GetFormation());

    const TArray<ISteerable*>& Members(Formation.GetMembers());
    const TSet<ISteerable*>& SrcMemberSet(MemberSet);
    TSet<ISteerable*> DstMemberSet;

    DstMemberSet.Reserve(Members.Num());

    for (ISteerable* Member : Members)
    {
        if (! Member)
        {
            continue;
        }

        // Add behavior to non-registered member
        if (! SrcMemberSet.Contains(Member))
        {
            FFormationFollowBehavior* Behavior = new FFormationFollowBehavior(SteeringTarget);
            TSharedPtr<FFormationFollowBehavior> BehaviorPtr(Behavior);
            Member->AddSteeringBehavior(BehaviorPtr);

            // Keep weak pointer of member move behavior
            FinishedBehaviors.Emplace(BehaviorPtr->GetBehaviorFinishedPtr());
        }

        // Register member assignment
        DstMemberSet.Emplace(Member);
    }

    MemberSet = MoveTemp(DstMemberSet);
}

void FMoveFormationBehavior::UpdateFormationOrientation()
{
    check(HasValidData());

    FSteeringFormation& Formation(GetFormation());
    const FVector AnchorLocation(Formation.GetAnchorLocation());
    const FVector TargetDirection = (SteeringTarget.Location-AnchorLocation).GetSafeNormal();

    Formation.SetOrientation(TargetDirection.ToOrientationQuat());
}
