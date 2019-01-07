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

/**
 * Movement component meant for use with SmoothDeltaActor.
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "VSmoothDeltaMovementComponent.h"
#include "SplineFollowingMovementComponent.generated.h"

/** 
 *
 */
UCLASS(ClassGroup=Movement, meta=(BlueprintSpawnableComponent))
class STEERINGSYSTEMPLUGIN_API USplineFollowingMovementComponent : public UVSmoothDeltaMovementComponent
{
	GENERATED_BODY()

public:

    /** Event called after owning actor movement source changes. */
    virtual void MovementSourceChange() override;

protected:

    /** Current movement source. */
    UPROPERTY()
    class USplineComponent* SplineSource;

    /** Actual server movement update implementation */
    virtual void MovementUpdate(float DeltaTime) override;
    virtual void MovementUpdateImpl(float DeltaTime, bool bHandleImpact);

    /**
     * Moves along the given movement direction using simple movement rules based on the current movement mode (usually used by simulated proxies).
     */
    virtual void MoveSmooth(float DeltaTime) override;
    virtual void MoveSmooth_Visual() override;

    /** Update transform to the current simulation time */
    virtual void SnapToCurrentTime() override;

    virtual float GetClampedSmoothSimulationOffset(float SimulationTimeDelta) const override;
    FORCEINLINE float GetSplineTime(float InSplineDuration) const;
    FORCEINLINE float GetSplineTime(float InSimulationTime, float InSplineDuration) const;
    FORCEINLINE float GetSplineTime(float InSimulationTime, float InSimulationDuration, float InSplineDuration) const;

    FORCEINLINE bool HasValidSource() const;
};
