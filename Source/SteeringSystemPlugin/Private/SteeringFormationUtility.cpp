// 

#include "SteeringFormationUtility.h"
#include "Behaviors/RegroupFormationBehavior.h"
#include "Behaviors/MoveFormationBehavior.h"

FFormationBehaviorRef USteeringFormationUtility::CreateRegroupBehavior()
{
    FPSFormationBehavior Behavior( new FRegroupFormationBehavior() );
    return FFormationBehaviorRef(Behavior);
}

FFormationBehaviorRef USteeringFormationUtility::CreateMoveBehavior(const FSteeringTarget& SteeringTarget)
{
    FPSFormationBehavior Behavior( new FMoveFormationBehavior(SteeringTarget) );
    return FFormationBehaviorRef(Behavior);
}
