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
#include "SteeringTypes.h"
#include "SteeringFormation.h"

class FFormationBehavior : public ISteeringBehaviorBase
{
protected:

    // Whether the behavior requires formation primary for calculation
    bool bRequirePrimary = false;

    // Owning formation
    FSteeringFormation* OwningFormation = nullptr;

    virtual bool UpdateBehaviorState(float DeltaTime)
    {
        // Blank implementation, return true as finished
        return true;
    }

    virtual bool CalculateSteeringImpl(float DeltaTime) = 0;

public:

    virtual ~FFormationBehavior()
    {
        OwningFormation = nullptr;
    }

    virtual void SetFormation(FSteeringFormation* InFormation)
    {
        OwningFormation = InFormation;
    }

    FORCEINLINE FSteeringFormation& GetFormation()
    {
        return *OwningFormation;
    }

    FORCEINLINE const FSteeringFormation& GetFormation() const
    {
        return *OwningFormation;
    }

    FORCEINLINE virtual bool HasValidData() const
    {
        return OwningFormation && OwningFormation->HasValidData() && (!bRequirePrimary || OwningFormation->HasPrimary());
    }

    bool CalculateSteering(float DeltaTime)
    {
        if (IsActive() && HasValidData())
        {
            return CalculateSteeringImpl(DeltaTime);
        }

        return true;
    }
};
