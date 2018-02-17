// 

#pragma once

#include "FormationBehavior.h"
#include "Templates/SharedPointer.h"

class FRegroupFormationBehavior : public FFormationBehavior
{
protected:

    TSet<ISteerable*> MemberSet;
    TArray<TSharedRef<bool>> FinishedBehaviors;

    virtual void OnActivated() override;
    virtual void OnDeactivated() override;
    virtual bool UpdateBehaviorState(float DeltaTime) override;
    virtual bool CalculateSteeringImpl(float DeltaTime) override;

    void InitializeAnchor();
    void UpdateBehaviorAssignments();
    void UpdateFormationOrientation();

public:

    FRegroupFormationBehavior()
    {
        bRequirePrimary = true;
    }

    FORCEINLINE virtual FName GetType() const override
    {
        static const FName Type(TEXT("RegroupFormationBehavior"));
        return Type;
    }
};
