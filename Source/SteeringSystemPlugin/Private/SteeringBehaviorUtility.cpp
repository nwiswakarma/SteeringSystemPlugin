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

#include "SteeringBehaviorUtility.h"
#include "Behaviors/AlignBehavior.h"
#include "Behaviors/ArriveBehavior.h"
#include "Behaviors/RepulsionBehavior.h"

FSteeringTarget USteeringBehaviorUtility::MakeTargetLocation(const FVector& InLocation)
{
    return FSteeringTarget(InLocation);
}

FSteeringTarget USteeringBehaviorUtility::MakeTargetRotation(const FRotator& InRotation)
{
    return FSteeringTarget(InRotation);
}

FSteeringTarget USteeringBehaviorUtility::MakeTargetDirection(const FVector& InDirection)
{
    return FSteeringTarget(InDirection.ToOrientationQuat());
}

FSteeringBehaviorRef USteeringBehaviorUtility::CreateArriveBehavior(const FSteeringTarget& InSteeringTarget, float InOuterRadius, float InInnerRadius)
{
    FPSSteeringBehavior Behavior( new FArriveBehavior(InSteeringTarget, InOuterRadius, InInnerRadius) );
    return FSteeringBehaviorRef(Behavior);
}

FSteeringBehaviorRef USteeringBehaviorUtility::CreateAlignBehavior(const FSteeringTarget& InSteeringTarget)
{
    FPSSteeringBehavior Behavior( new FAlignBehavior(InSteeringTarget) );
    return FSteeringBehaviorRef(Behavior);
}

FSteeringBehaviorRef USteeringBehaviorUtility::CreateRepulsionBehavior(const FVector& InRepulsionLocation, float InRepulsionDistance)
{
    FPSSteeringBehavior Behavior( new FRepulsionBehavior(InRepulsionLocation, InRepulsionDistance) );
    return FSteeringBehaviorRef(Behavior);
}

FSteeringBehaviorRef USteeringBehaviorUtility::CreateLineRepulsionBehavior(const FVector& InLineOrigin, const FVector& InLineDirection, float InRepulsionDistance, const FVector& InParallelRepulsionDirection)
{
    FPSSteeringBehavior Behavior( new FLineRepulsionBehavior(InLineOrigin, InLineDirection, InRepulsionDistance, InParallelRepulsionDirection) );
    return FSteeringBehaviorRef(Behavior);
}

FSteeringFormationData USteeringBehaviorUtility::CreateFormation(const TArray<FSteerableRef>& Agents, uint8 FormationType)
{
    return FSteeringFormationData(Agents);
}

void USteeringBehaviorUtility::ProjectFormationAt(FSteeringFormationData& Formation, const FVector& TargetLocation)
{
    Formation.ProjectLocation(TargetLocation);
}
