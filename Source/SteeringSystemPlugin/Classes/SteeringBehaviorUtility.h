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

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SteeringTypes.h"
#include "SteeringBehaviorUtility.generated.h"

struct FSteeringBehaviorRef;
class ISteerable;

UCLASS()
class STEERINGSYSTEMPLUGIN_API USteeringBehaviorUtility : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

    UFUNCTION(BlueprintCallable)
    static FSteeringTarget MakeTargetLocation(const FVector& InLocation);

    UFUNCTION(BlueprintCallable)
    static FSteeringTarget MakeTargetDirection(const FVector& InDirection);

    UFUNCTION(BlueprintCallable)
    static FSteeringTarget MakeTargetRotation(const FRotator& InRotation);

    UFUNCTION(BlueprintCallable)
    static FSteeringBehaviorRef CreateArriveBehavior(const FSteeringTarget& InSteeringTarget, float InOuterRadius = 150.f, float InInnerRadius = 50.f);

    UFUNCTION(BlueprintCallable)
    static FSteeringBehaviorRef CreateAlignBehavior(const FSteeringTarget& InSteeringTarget);

    UFUNCTION(BlueprintCallable)
    static FSteeringBehaviorRef CreateRepulsionBehavior(const FVector& InRepulsionLocation, float InRepulsionDistance = 150.f);

    UFUNCTION(BlueprintCallable)
    static FSteeringBehaviorRef CreateLineRepulsionBehavior(const FVector& InLineOrigin, const FVector& InLineDirection, float InRepulsionDistance = 150.f, const FVector& InParallelRepulsionDirection = FVector::UpVector);

    UFUNCTION(BlueprintCallable)
    static FSteeringFormationData CreateFormation(const TArray<FSteerableRef>& Agents, uint8 FormationType = 0);

    UFUNCTION(BlueprintCallable)
    static void ProjectFormationAt(FSteeringFormationData& Formation, const FVector& TargetLocation);
};
