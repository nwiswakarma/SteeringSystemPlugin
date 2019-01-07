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

#include "Behaviors/RepulsionBehavior.h"

#include "UnrealMathUtility.h"
#include "Kismet/KismetMathLibrary.h"

bool FRepulsionBehavior::CalculateSteeringImpl(float DeltaTime, FSteeringAcceleration& OutControlInput)
{
    check(HasValidData());

    const FVector SrcLocation(Steerable->GetSteerableLocation());
    const FVector SrcNormal(Steerable->GetForwardVector());
    const FVector DeltaLocation(SrcLocation-RepulsionLocation);

    // Check for arrival
    if (DeltaLocation.SizeSquared() > (RepulsionDistance*RepulsionDistance))
    {
        return true;
    }

    OutControlInput = FSteeringAcceleration(DeltaLocation.GetSafeNormal(), true, true);

    return false;
}

bool FLineRepulsionBehavior::CalculateSteeringImpl(float DeltaTime, FSteeringAcceleration& OutControlInput)
{
    check(HasValidData());

    const FVector SrcLocation(Steerable->GetSteerableLocation());
    const FVector SrcNormal(Steerable->GetForwardVector());

    const FVector DstLocation(UKismetMathLibrary::FindClosestPointOnLine(SrcLocation, LineOrigin, LineDirection));
    const FVector DeltaLocation(SrcLocation-DstLocation);
    const float DeltaDistSq(DeltaLocation.SizeSquared());

    // Check for arrival
    if (DeltaDistSq > (RepulsionDistance*RepulsionDistance))
    {
        return true;
    }

    FVector RepulsionDirection = (DeltaDistSq > SMALL_NUMBER) ? DeltaLocation.GetSafeNormal() : ParallelRepulsionDirection;
    OutControlInput = FSteeringAcceleration(RepulsionDirection, true, true);

    return false;
}
