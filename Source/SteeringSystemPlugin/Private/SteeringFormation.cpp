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

#include "SteeringFormation.h"
#include "FormationBehavior.h"

void FDefaultFormationPattern::CalculateDimension()
{
    check(HasOwningFormation());

    const FSteeringFormation& Formation(GetOwningFormation());
    const int32 Count = Formation.GetMemberCount();

    if (Count <= 2)
    {
        DimX = FMath::Max(Count, 1);
        DimY = 1;
        return;
    }

    const int32 CountPow2 = FPlatformMath::RoundUpToPowerOfTwo(Count);
    DimX = CountPow2 >> 1;
    DimY = 1 << 1;

    for (int32 i=2; DimX>=DimY; ++i)
    {
        const int32 w = CountPow2 >> i;
        const int32 h = 1 << i;

        if (w < h)
        {
            break;
        }

        DimX = w;
        DimY = h;
    }
}

void FDefaultFormationPattern::CalculateMaxRadius()
{
    check(HasOwningFormation());

    const FSteeringFormation& Formation(GetOwningFormation());
    const int32 Count = Formation.GetMemberCount();

    if (Count == 0)
    {
        MaxRadius = 1.f;
        return;
    }

    // Find maximum bounding radius
    for (const ISteerable* Member : Formation.GetMembers())
    {
        if (Member)
        {
            MaxRadius = FMath::Max(MaxRadius, Member->GetOuterRadius());
        }
    }

    // Invalid max bounding radius, reset to default radius
    if (MaxRadius < KINDA_SMALL_NUMBER)
    {
        MaxRadius = 1.f;
    }
}

void FDefaultFormationPattern::CalculateSlotOffsets()
{
    check(HasOwningFormation());

    const FSteeringFormation& Formation(GetOwningFormation());
    const int32 AgentCount = Formation.GetMemberCount();

    // No registered agent, abort
    if (AgentCount == 0)
    {
        return;
    }

    // Reset current slot offsets
    SlotOffsets.Reset();

    const int32 LastX = AgentCount / DimX;
    const int32 EvenCount = LastX * DimX;
    const int32 OddCount = AgentCount % DimX;
    const float DimSize = MaxRadius * (DimX-1);
    const float OddSize = DimSize / FMath::Max(OddCount+1, 1);

    // Project fully-filled rows
    for (int32 i=0; i<EvenCount; ++i)
    {
        const int32 x = i / DimX;
        const int32 y = i % DimX;
        SlotOffsets.Emplace(x*MaxRadius, y*MaxRadius, 0.f);
    }

    // Project odd-filled rows
    for (int32 i=0; i<OddCount; ++i)
    {
        const int32 x = LastX;
        const int32 y = i+1;
        SlotOffsets.Emplace(x*MaxRadius, y*OddSize, 0.f);
    }
}

void FDefaultFormationPattern::UpdateFormationPattern()
{
    if (HasOwningFormation())
    {
        CalculateDimension();
        CalculateMaxRadius();
        CalculateSlotOffsets();
    }
}

void FDefaultFormationPattern::CalculateSlotLocation(int32 SlotIndex, FVector& SlotLocation)
{
    if (SlotOffsets.IsValidIndex(SlotIndex))
    {
        SlotLocation = SlotOffsets[SlotIndex];
    }
    else
    {
        SlotLocation = FVector::ZeroVector;
    }
}

void FDefaultFormationPattern::CalculateSlotOrientation(int32 SlotIndex, FQuat& SlotOrientation)
{
    bool bHasPrimary = false;

    if (HasOwningFormation())
    {
        FSteeringFormation& Formation(GetOwningFormation());

        if (Formation.HasPrimary())
        {
            SlotOrientation = Formation.GetPrimary()->GetSteerableOrientation();
            bHasPrimary = true;
        }
        else
        {
            SlotOrientation = Formation.GetOrientation();
        }
    }

    if (! bHasPrimary)
    {
        SlotOrientation = FQuat::Identity;
    }
}

ISteerable* FDefaultPrimaryAssignmentStrategy::FindPrimary(const FSteeringFormation& Formation)
{
    return (Formation.GetMemberCount() > 0) ? Formation.GetMember(0) : nullptr;
}

void FDefaultSlotAssignmentStrategy::UpdateSlotAssignments()
{
    if (HasOwningFormation())
    {
        FSteeringFormation& Formation(GetOwningFormation());
        FSlotAssignmentMap& SlotMap(Formation.GetSlotMap());
        int32 SlotIndex = 0;

        for (FSlotAssignmentPair& SlotPair : SlotMap)
        {
            SlotPair.Value = SlotIndex++;
        }
    }
}

FSteeringFormation::FSteeringFormation()
    : FormationProxy(nullptr)
    , Primary(nullptr)
    , VelocityLimit(0.f)
    , Orientation(FQuat::Identity)
    , bRequirePatternUpdate(true)
{
    SetDefaultFormationPattern();
    SetDefaultSlotAssignmentStrategy();
    SetDefaultPrimaryAssignmentStrategy();
}

FSteeringFormation::~FSteeringFormation()
{
    ClearMembers();
    ClearBehaviors();

    FormationProxy = nullptr;
    Primary = nullptr;

    if (Members.Num())
    {
        Members.Empty();
    }

    if (Behaviors.Num())
    {
        Behaviors.Empty();
    }
}

// ~ Update Functions

void FSteeringFormation::UpdatePrimaryAssignment()
{
    check(PrimaryAssignmentStrategy.IsValid());

    // No registered primary, find one and assign
    if (! Primary && Members.Num() > 0)
    {
        SetPrimary(PrimaryAssignmentStrategy->FindPrimary(*this));
    }
}

void FSteeringFormation::UpdateFormationPattern()
{
    check(FormationPattern.IsValid());
    FormationPattern->UpdateFormationPattern();
}

void FSteeringFormation::UpdateSlotAssignments()
{
    check(SlotAssignmentStrategy.IsValid());
    SlotAssignmentStrategy->UpdateSlotAssignments();
}

void FSteeringFormation::UpdateBehavior(float DeltaTime)
{
    bool bCurrentBehaviorFinished;

    // Execute current active behavior. If it finished, immediately execute next behavior if any
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

bool FSteeringFormation::UpdateActiveBehavior(float DeltaTime)
{
    check(Behaviors.Num() > 0);

    FFormationBehaviorListNode* Node(Behaviors.GetTail());
    FPSFormationBehavior& Behavior(Node->GetValue());
    bool bInProgress = Behavior.IsValid();
    bool bIsDone = false;

    if (bInProgress)
    {
        if (! Behavior->IsActive())
        {
            Behavior->Activate();
        }

        bIsDone = Behavior->CalculateSteering(DeltaTime);

        if (bIsDone)
        {
            if (Behavior->IsActive())
            {
                Behavior->Deactivate();
            }

            bInProgress = false;
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

void FSteeringFormation::UpdateFormation(float DeltaTime)
{
    // Update primary steerable assignment
    UpdatePrimaryAssignment();

    // Update pattern and slot assignment if required
    if (bRequirePatternUpdate)
    {
        UpdateFormationPattern();
        UpdateSlotAssignments();
    }

    // Update formation behavior
    UpdateBehavior(DeltaTime);

    // Clear pattern update flag
    if (bRequirePatternUpdate)
    {
        bRequirePatternUpdate = false;
    }
}

// ~ Behavior Registration

void FSteeringFormation::AddBehavior(FPSFormationBehavior InBehavior)
{
    if (InBehavior.IsValid())
    {
        InBehavior->SetFormation(this);
        Behaviors.AddTail(InBehavior);
    }
}

void FSteeringFormation::EnqueueBehavior(FPSFormationBehavior InBehavior)
{
    if (InBehavior.IsValid())
    {
        InBehavior->SetFormation(this);
        Behaviors.AddHead(InBehavior);
    }
}

void FSteeringFormation::RemoveBehavior(FPSFormationBehavior InBehavior)
{
    if (InBehavior.IsValid())
    {
        FFormationBehaviorListNode* Node(Behaviors.FindNode(InBehavior));

        if (Node)
        {
            FPSFormationBehavior& Behavior(Node->GetValue());

            // Reset behavior
            ResetRegisteredBehavior(Behavior);

            // Remove behavior node
            Behaviors.RemoveNode(Node, true);
        }
    }
}

void FSteeringFormation::ClearBehaviors()
{
    for (FPSFormationBehavior& Behavior : Behaviors)
    {
        ResetRegisteredBehavior(Behavior);
    }

    Behaviors.Empty();
}

void FSteeringFormation::ResetRegisteredBehavior(FPSFormationBehavior& Behavior)
{
    if (Behavior.IsValid())
    {
        // Deactivate behavior if required
        if (Behavior->IsActive())
        {
            Behavior->Deactivate();
        }

        Behavior->SetFormation(nullptr);
        Behavior.Reset();
    }
}

// ~ Formation Utility Functions

void FSteeringFormation::SetFormationProxy(IFormationProxy* InFormationProxy)
{
    // Formation proxy remains unmodified, abort
    if (FormationProxy == InFormationProxy)
    {
        return;
    }

    // Clear member dependency
    if (FormationProxy)
    {
        for (ISteerable* Member : Members)
        {
            FormationProxy->RemoveMemberDependency(Member);
        }
    }

    FormationProxy = InFormationProxy;

    // Add member dependency
    if (FormationProxy)
    {
        for (ISteerable* Member : Members)
        {
            FormationProxy->AddMemberDependency(Member);
        }
    }
}

void FSteeringFormation::SetPrimary(ISteerable* InPrimary)
{
    // Primary must be a member of the formation
    if (InPrimary && Members.Contains(InPrimary))
    {
        Primary = InPrimary;
    }
    else
    {
        Primary = nullptr;
    }
}

void FSteeringFormation::SetDefaultFormationPattern()
{
    SetFormationPattern(FPSFormationPattern(new FDefaultFormationPattern()));
}

void FSteeringFormation::SetDefaultSlotAssignmentStrategy()
{
    SetSlotAssignmentStrategy(FPSSlotAssignmentStrategy(new FDefaultSlotAssignmentStrategy()));
}

void FSteeringFormation::SetDefaultPrimaryAssignmentStrategy()
{
    SetPrimaryAssignmentStrategy(FPSPrimaryAssignmentStrategy(new FDefaultPrimaryAssignmentStrategy()));
}

void FSteeringFormation::SetFormationPattern(FPSFormationPattern InFormationPattern)
{
    if (InFormationPattern.IsValid())
    {
        if (FormationPattern.IsValid())
        {
            FormationPattern->SetOwningFormation(nullptr);
        }

        FormationPattern = InFormationPattern;
        FormationPattern->SetOwningFormation(this);
    }
}

void FSteeringFormation::SetSlotAssignmentStrategy(FPSSlotAssignmentStrategy InSlotAssignmentStrategy)
{
    if (InSlotAssignmentStrategy.IsValid())
    {
        if (SlotAssignmentStrategy.IsValid())
        {
            SlotAssignmentStrategy->SetOwningFormation(nullptr);
        }

        SlotAssignmentStrategy = InSlotAssignmentStrategy;
        SlotAssignmentStrategy->SetOwningFormation(this);
    }
}

void FSteeringFormation::SetPrimaryAssignmentStrategy(FPSPrimaryAssignmentStrategy InPrimaryAssignmentStrategy)
{
    if (InPrimaryAssignmentStrategy.IsValid())
    {
        PrimaryAssignmentStrategy = InPrimaryAssignmentStrategy;
    }
}

void FSteeringFormation::CalculateSlotLocation(ISteerable* Member, FVector& SlotLocation)
{
    check(HasValidData());
    check(FormationPattern.IsValid());

    if (SlotMap.Contains(Member))
    {
        const int32 SlotIndex = SlotMap.FindChecked(Member);

        FormationPattern->CalculateSlotLocation(SlotIndex, SlotLocation);
        SlotLocation = Orientation.RotateVector(SlotLocation);
        SlotLocation += GetAnchorLocation();
    }
}

void FSteeringFormation::CalculateSlotLocation(ISteerable* Member, FVector& SlotLocation, const FVector& SlotOffset)
{
    check(HasValidData());
    check(FormationPattern.IsValid());

    if (SlotMap.Contains(Member))
    {
        const int32 SlotIndex = SlotMap.FindChecked(Member);
        const FVector SlotAnchor = GetAnchorLocation();

        FormationPattern->CalculateSlotLocation(SlotIndex, SlotLocation);
        SlotLocation = Orientation.RotateVector(SlotLocation);
        SlotLocation += SlotAnchor + (SlotOffset-SlotAnchor);
    }
}

void FSteeringFormation::CalculateSlotOrientation(ISteerable* Member, FQuat& SlotOrientation)
{
    check(HasValidData());
    check(FormationPattern.IsValid());

    if (SlotMap.Contains(Member))
    {
        const int32 SlotIndex = SlotMap.FindChecked(Member);
        FormationPattern->CalculateSlotOrientation(SlotIndex, SlotOrientation);
    }
}

// ~ Formation Member Functions

void FSteeringFormation::AddMemberDependency(ISteerable* Member)
{
    if (FormationProxy)
    {
        FormationProxy->AddMemberDependency(Member);
    }

    Member->SetFormationDirect(this);
}

void FSteeringFormation::RemoveMemberDependency(ISteerable* Member)
{
    if (FormationProxy)
    {
        FormationProxy->RemoveMemberDependency(Member);
    }

    Member->SetFormationDirect(nullptr);
}

int32 FSteeringFormation::SetMembers(const TArray<ISteerable*>& InMembers)
{
    // Clear member tick dependency
    for (ISteerable* Member : Members)
    {
        RemoveMemberDependency(Member);
    }

    // Assign new members
    Members = InMembers;

    // Clear invalid members
    Members.RemoveAllSwap([&](ISteerable* Member) { return Member == nullptr; }, true);

    // Add member tick dependency
    for (ISteerable* Member : Members)
    {
        AddMemberDependency(Member);
        SlotMap.Emplace(Member, -1);
    }

    // Mark pattern update
    MarkPatternUpdate();

    return Members.Num();
}

bool FSteeringFormation::AddMember(ISteerable* Member, bool bSetAsPrimary)
{
    bool bMemberAdded = false;

    if (Member && ! SlotMap.Contains(Member))
    {
        Members.Emplace(Member);
        SlotMap.Emplace(Member, -1);

        // Add member dependency, if anchor presents
        AddMemberDependency(Member);

        // Set added member as primary if required
        if (bSetAsPrimary)
        {
            SetPrimary(Member);
        }

        // Mark formation update
        MarkPatternUpdate();

        bMemberAdded = true;
    }

    return bMemberAdded;
}

bool FSteeringFormation::RemoveMember(ISteerable* Member)
{
    bool bMemberRemoved = false;

    if (Member && SlotMap.Contains(Member))
    {
        Members.RemoveSwap(Member);
        SlotMap.Remove(Member);

        // Removed member is primary, clear primary
        if (Member == Primary)
        {
            SetPrimary(nullptr);
        }

        // Remove member dependency, if anchor presents
        RemoveMemberDependency(Member);

        // Mark formation update
        MarkPatternUpdate();

        bMemberRemoved = true;
    }

    return bMemberRemoved;
}

int32 FSteeringFormation::ClearMembers()
{
    int32 MemberCount = Members.Num();
    const TArray<ISteerable*> EmptyMembers;

    SetMembers(EmptyMembers);

    return MemberCount;
}
