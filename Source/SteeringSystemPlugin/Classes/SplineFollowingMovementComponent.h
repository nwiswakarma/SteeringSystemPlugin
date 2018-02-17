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
