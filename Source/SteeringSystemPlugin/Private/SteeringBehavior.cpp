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

#include "SteeringBehavior.h"

void FSteeringFormationData::CalculateDimension()
{
    const int32 Count = Agents.Num();

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

void FSteeringFormationData::CalculateMaxRadius()
{
    const int32 Count = Agents.Num();

    if (Count == 0)
    {
        MaxRadius = 1.f;
        return;
    }

    // Get maximum bounding radius
    for (const ISteerable* Agent : Agents)
    {
        if (Agent)
        {
            MaxRadius = FMath::Max(MaxRadius, Agent->GetOuterRadius());
        }
    }

    // Invalid max bounding radius, reset to default radius
    if (MaxRadius < KINDA_SMALL_NUMBER)
    {
        MaxRadius = 1.f;
    }
}

void FSteeringFormationData::CalculateOrigin()
{
    if (ProjectedLocations.Num() > 0)
    {
        FBox BoundingBox(ForceInitToZero);

        for (const FVector& Location : ProjectedLocations)
        {
            BoundingBox += Location;
        }

        BoundingOrigin = BoundingBox.GetCenter();
    }
    else
    {
        FBox BoundingBox(ForceInitToZero);

        for (const ISteerable* Agent : Agents)
        {
            if (Agent)
            {
                BoundingBox += Agent->GetSteerableLocation();
            }
        }

        BoundingOrigin = BoundingBox.GetCenter();
    }
}

void FSteeringFormationData::ProjectLocation(const FVector& TargetLocation)
{
    const int32 AgentCount = Agents.Num();

    // No registered agent, abort
    if (AgentCount == 0)
    {
        return;
    }

    // Calculate origin from previous projected locations
    // or from agents bounding origin if there is none
    CalculateOrigin();

    // Reset current projected locations
    ProjectedLocations.Reset();

    const FVector& Origin(BoundingOrigin);
    const FVector TargetDirection = (TargetLocation-Origin).GetSafeNormal();
    const FQuat Orientation = TargetDirection.ToOrientationQuat();

    FBox BoundingBox;
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
        const FVector pos(x*MaxRadius, y*MaxRadius, 0.f);

        BoundingBox += pos;
        ProjectedLocations.Emplace(pos);
    }

    // Project odd-filled rows
    for (int32 i=0; i<OddCount; ++i)
    {
        const int32 x = LastX;
        const int32 y = i+1;
        const FVector pos(x*MaxRadius, y*OddSize, 0.f);

        BoundingBox += pos;
        ProjectedLocations.Emplace(pos);
    }

    // Get bound offset
    const FVector BoundExtents = BoundingBox.GetExtent();

    for (FVector& ProjectedLocation : ProjectedLocations)
    {
        ProjectedLocation = TargetLocation + Orientation.RotateVector(ProjectedLocation-BoundExtents);
    }

    // Assign new forward vector
    ForwardVector = Orientation.GetForwardVector();
}

void FSteeringFormationData::ClearProjectedLocations()
{
    ProjectedLocations.Reset();
}
