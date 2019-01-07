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
#include "Engine/EngineTypes.h"
#include "Templates/SharedPointer.h"

class FSteeringBehavior;
class FSteeringFormation;

class ISteerable
{
public:
// ~ Steerable Interface
    // Behaviors
	virtual void AddSteeringBehavior(TSharedPtr<FSteeringBehavior> InBehavior) = 0;
	virtual void EnqueueSteeringBehavior(TSharedPtr<FSteeringBehavior> InBehavior) = 0;
	virtual void RemoveSteeringBehavior(TSharedPtr<FSteeringBehavior> InBehavior) = 0;
	virtual void ClearSteeringBehaviors() = 0;
    // Formations
	virtual void SetFormation(FSteeringFormation* InFormation, bool bSetAsPrimary=false) = 0;
	virtual void SetFormationDirect(FSteeringFormation* InFormation) = 0;
	virtual FSteeringFormation* GetFormation() = 0;
	virtual bool HasFormation() const = 0;
    // Steering Properties
	virtual FTickFunction* GetSteerableTickFunction() = 0;
	virtual FString GetSteerableName() const = 0;
	virtual FVector GetSteerableLocation() const = 0;
	virtual FQuat GetSteerableOrientation() const = 0;
	virtual FVector GetForwardVector() const = 0;
	virtual FVector GetLinearVelocity() const = 0;
	virtual float GetInnerRadius() const = 0;
	virtual float GetOuterRadius() const = 0;
	virtual int32 GetPriority() const = 0;
// ~ Limiter Interface
	virtual float GetMaxLinearSpeed() const = 0;
	virtual float GetMaxLinearAcceleration() const = 0;
	virtual float GetMinControlInput() const = 0;
};
