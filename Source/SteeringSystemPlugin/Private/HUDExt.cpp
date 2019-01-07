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
