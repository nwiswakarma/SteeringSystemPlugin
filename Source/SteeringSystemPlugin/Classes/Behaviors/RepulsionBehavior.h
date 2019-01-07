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

#include "SteeringBehavior.h"

class FRepulsionBehavior : public FSteeringBehavior
{
protected:

    FVector RepulsionLocation;
    float RepulsionDistance;

    virtual bool CalculateSteeringImpl(float DeltaTime, FSteeringAcceleration& OutControlInput) override;

public:

    FRepulsionBehavior(const FVector& InRepulsionLocation, float InRepulsionDistance)
        : RepulsionLocation(InRepulsionLocation)
        , RepulsionDistance(FMath::Abs(InRepulsionDistance))
    {
    }

    FORCEINLINE virtual FName GetType() const override
    {
        static const FName Type(TEXT("RepulsionBehavior"));
        return Type;
    }
};

class FLineRepulsionBehavior : public FSteeringBehavior
{
protected:

    FVector LineOrigin;
    FVector LineDirection;
    FVector ParallelRepulsionDirection;
    float RepulsionDistance;

    virtual bool CalculateSteeringImpl(float DeltaTime, FSteeringAcceleration& OutControlInput) override;

public:

    FLineRepulsionBehavior(const FVector& InLineOrigin, const FVector& InLineDirection, float InRepulsionDistance, const FVector& InParallelRepulsionDirection)
        : LineOrigin(InLineOrigin)
        , LineDirection(InLineDirection)
        , RepulsionDistance(FMath::Abs(InRepulsionDistance))
        , ParallelRepulsionDirection(InParallelRepulsionDirection)
    {
    }

    FORCEINLINE virtual FName GetType() const override
    {
        static const FName Type(TEXT("LineRepulsionBehavior"));
        return Type;
    }
};
