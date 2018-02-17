// 

#include "HUDExt.h"

void AHUDExt::FindFilteredActorsInSelectionRectangle(const TArray<USceneComponent*>& Components, const FVector2D& FirstPoint, const FVector2D& SecondPoint, TArray<AActor*>& OutActors, bool bActorMustBeFullyEnclosed)
{
    // Because this is a HUD function it is likely to get called each tick,
    // so make sure any previous contents of the out actor array have been cleared!
    OutActors.Reset();

    // Create Selection Rectangle from Points
    FBox2D SelectionRectangle(ForceInit);

    // This method ensures that an appropriate rectangle is generated, 
    // no matter what the coordinates of first and second point actually are.
    SelectionRectangle += FirstPoint;
    SelectionRectangle += SecondPoint;

    //The Actor Bounds Point Mapping
    const FVector BoundsPointMapping[8] =
    {
        FVector(1.f, 1.f, 1.f),
        FVector(1.f, 1.f, -1.f),
        FVector(1.f, -1.f, 1.f),
        FVector(1.f, -1.f, -1.f),
        FVector(-1.f, 1.f, 1.f),
        FVector(-1.f, 1.f, -1.f),
        FVector(-1.f, -1.f, 1.f),
        FVector(-1.f, -1.f, -1.f)   };

    for (USceneComponent* Comp : Components)
    {
        if (! IsValid(Comp) || ! Comp->GetOwner())
        {
            continue;
        }

        AActor* OwningActor = Comp->GetOwner();

        const FBox CompBounds = Comp->Bounds.GetBox();
        const FVector BoxCenter = CompBounds.GetCenter();
        const FVector BoxExtents = CompBounds.GetExtent();

        // Build 2D bounding box of actor in screen space
        FBox2D ActorBox2D(ForceInit);
        for (uint8 BoundsPointItr = 0; BoundsPointItr < 8; BoundsPointItr++)
        {
            // Project vert into screen space.
            const FVector ProjectedWorldLocation = Project(BoxCenter + (BoundsPointMapping[BoundsPointItr] * BoxExtents));
            // Add to 2D bounding box
            ActorBox2D += FVector2D(ProjectedWorldLocation.X, ProjectedWorldLocation.Y);
        }

        if (bActorMustBeFullyEnclosed)
        {
            if (SelectionRectangle.IsInside(ActorBox2D))
            {
                OutActors.Add(OwningActor);
            }
        }
        else
        {
            if (SelectionRectangle.Intersect(ActorBox2D))
            {
                OutActors.Add(OwningActor);
            }
        }
    }
}

TArray<AActor*> AHUDExt::GetFilteredActorsInSelectionRectangle(const TArray<USceneComponent*>& Components, const FVector2D& FirstPoint, const FVector2D& SecondPoint, bool bActorMustBeFullyEnclosed)
{
    TArray<AActor*> OutActors;
    FindFilteredActorsInSelectionRectangle(Components, FirstPoint, SecondPoint, OutActors, bActorMustBeFullyEnclosed);
    return MoveTemp(OutActors);
}
