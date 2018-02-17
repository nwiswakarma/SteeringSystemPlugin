// 

#include "Behaviors/RegroupFormationBehavior.h"
#include "Behaviors/FormationFollowBehavior.h"
#include "UnrealMathUtility.h"

void FRegroupFormationBehavior::OnActivated()
{
    check(HasValidData());

    InitializeAnchor();
    UpdateBehaviorAssignments();
    UpdateFormationOrientation();
}

void FRegroupFormationBehavior::OnDeactivated()
{
    MemberSet.Empty();
    FinishedBehaviors.Empty();
}

bool FRegroupFormationBehavior::UpdateBehaviorState(float DeltaTime)
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

bool FRegroupFormationBehavior::CalculateSteeringImpl(float DeltaTime)
{
    check(HasValidData());

    FSteeringFormation& Formation(GetFormation());

    if (Formation.IsPatternUpdateRequired())
    {
        UpdateBehaviorAssignments();
    }

    const bool bFinished = UpdateBehaviorState(DeltaTime);
    return bFinished;
}

void FRegroupFormationBehavior::InitializeAnchor()
{
    check(HasValidData());

    FSteeringFormation& Formation(GetFormation());

    const ISteerable& Primary(*Formation.GetPrimary());
    const FVector Location(Primary.GetSteerableLocation());
    const FQuat Orientation(Primary.GetSteerableOrientation());

    Formation.SetAnchorLocationAndOrientation(Location, Orientation);
}

void FRegroupFormationBehavior::UpdateBehaviorAssignments()
{
    check(HasValidData());

    FSteeringFormation& Formation(GetFormation());

    const TArray<ISteerable*>& Members(Formation.GetMembers());
    const TSet<ISteerable*>& SrcMemberSet(MemberSet);
    TSet<ISteerable*> DstMemberSet;

    DstMemberSet.Reserve(Members.Num());

    const FSteeringTarget SteeringTarget(Formation.GetAnchorLocation());

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

void FRegroupFormationBehavior::UpdateFormationOrientation()
{
    check(HasValidData());

    FSteeringFormation& Formation(GetFormation());
    Formation.SetOrientation(Formation.GetAnchorOrientation());
}
