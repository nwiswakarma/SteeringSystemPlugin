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

#include "ControlInputComponent.h"

UControlInputComponent::UControlInputComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
    bMoveInputIgnored = false;
    bEnableAcceleration = true;
}

void UControlInputComponent::AddInputVector(FVector WorldAccel, bool bForce /*=false*/)
{
    AddInputVector_Direct(WorldAccel, bForce);
}

void UControlInputComponent::SetEnableAcceleration(bool bInEnableAcceleration)
{
    SetEnableAcceleration_Direct(bInEnableAcceleration);
}

FVector UControlInputComponent::GetPendingInputVector() const
{
	return GetPendingInputVector_Direct();
}

FVector UControlInputComponent::GetLastInputVector() const
{
	return GetLastInputVector_Direct();
}

FVector UControlInputComponent::ConsumeInputVector()
{
	return ConsumeInputVector_Direct();
}

bool UControlInputComponent::IsMoveInputIgnored() const
{
	return IsMoveInputIgnored_Direct();
}

bool UControlInputComponent::IsAccelerationEnabled() const
{
    return IsAccelerationEnabled_Direct();
}

void UControlInputComponent::AddInputVector_Direct(FVector WorldAccel, bool bForce /*=false*/)
{
	if (bForce || ! IsMoveInputIgnored())
	{
		ControlInputVector += WorldAccel;
	}
}

void UControlInputComponent::SetEnableAcceleration_Direct(bool bInEnableAcceleration)
{
    bEnableAcceleration = bInEnableAcceleration;
}

FVector UControlInputComponent::GetPendingInputVector_Direct() const
{
	return ControlInputVector;
}

FVector UControlInputComponent::GetLastInputVector_Direct() const
{
	return LastControlInputVector;
}

FVector UControlInputComponent::ConsumeInputVector_Direct()
{
	LastControlInputVector = ControlInputVector;
	ControlInputVector = FVector::ZeroVector;
    bEnableAcceleration = true;
	return LastControlInputVector;
}

bool UControlInputComponent::IsMoveInputIgnored_Direct() const
{
	return bMoveInputIgnored;
}

bool UControlInputComponent::IsAccelerationEnabled_Direct() const
{
    return bEnableAcceleration;
}
