// 

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameFramework/MovementComponent.h"
#include "VesselMovementComponent.generated.h"

class UControlInputComponent;
class URVO3DAgentComponent;

UCLASS(ClassGroup=Movement, meta=(BlueprintSpawnableComponent))
class STEERINGSYSTEMPLUGIN_API UVesselMovementComponent : public UMovementComponent
{
    GENERATED_UCLASS_BODY()

public:

    /** If true, search for the owner's control input component as the ControlInputComponent if there is not one currently assigned. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Component)
    bool bAutoRegisterControlComponent;

    /** If true, search for the owner's RVO agent component as the RVOAgentComponent if there is not one currently assigned. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Component)
    bool bAutoRegisterRVOAgentComponent;

    /** Maximum velocity magnitude allowed for the controlled Pawn. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=LinearMovement)
    float MaxSpeed;

    /** Acceleration applied by input (rate of change of velocity) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=LinearMovement)
    float Acceleration;

    /** Deceleration applied when there is no input (rate of change of velocity) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=LinearMovement)
    float Deceleration;

private:

    /**
     * Turning rate, in degrees/s
     */
    UPROPERTY(EditAnywhere, Category=RadialMovement, meta=(ClampMin="0", UIMin="0"))
    float TurnRate;

    /**
     * Minimum turning rate. If non-zero positive, turn rate will be interpolated from a turn speed range of
     * MinTurnRate to TurnRate at TurnAcceleration speed.
     */
    UPROPERTY(EditAnywhere, Category=RadialMovement, meta=(ClampMin="0", UIMin="0"))
    float MinTurnRate;

    /**
     * Interpolation speed of turn rate from MinTurnRate to TurnRate in degrees/s
     */
    UPROPERTY(EditAnywhere, Category=RadialMovement, meta=(ClampMin="0", UIMin="0"))
    float TurnAcceleration;

public:

    UFUNCTION(BlueprintCallable, Category=VesselMovementComponent)
    float GetTurnRate() const;

    UFUNCTION(BlueprintCallable, Category=VesselMovementComponent)
    float GetMinTurnRate() const;

    UFUNCTION(BlueprintCallable, Category=VesselMovementComponent)
    float GetTurnAcceleration() const;

    UFUNCTION(BlueprintCallable, Category=VesselMovementComponent)
    void SetTurnRate(float InTurnRate);

    UFUNCTION(BlueprintCallable, Category=VesselMovementComponent)
    void SetMinTurnRate(float InMinTurnRate);

    UFUNCTION(BlueprintCallable, Category=VesselMovementComponent)
    void SetTurnAcceleration(float InTurnAcceleration);

    UFUNCTION(BlueprintCallable, Category=VesselMovementComponent)
    virtual void SetControlInputComponent(UControlInputComponent* InControlInputComponent);

    UFUNCTION(BlueprintCallable, Category=VesselMovementComponent)
    virtual void SetRVOAgentComponent(URVO3DAgentComponent* InRVOAgentComponent);

//BEGIN UActorComponent Interface
    virtual void InitializeComponent() override;
    virtual void OnRegister() override;
    virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;
//END UActorComponent Interface

//BEGIN UMovementComponent Interface
    virtual float GetMaxSpeed() const override
    {
        return MaxSpeed;
    }

protected:

    virtual bool ResolvePenetrationImpl(const FVector& Adjustment, const FHitResult& Hit, const FQuat& NewRotation) override;
//END UMovementComponent Interface

    /**
     * The component that control movement input.
     * @see bAutoRegisterControlComponent, SetControlInputComponent()
     */
    UPROPERTY(BlueprintReadOnly, Transient, DuplicateTransient, Category=MovementComponent)
    UControlInputComponent* ControlInputComponent;

    /**
     * RVO3D avoidance agent component.
     * @see bAutoRegisterRVOAgentComponent, SetRVOAgentComponent()
     */
    UPROPERTY(BlueprintReadOnly, Transient, DuplicateTransient, Category=MovementComponent)
    URVO3DAgentComponent* RVOAgentComponent;

    /** Set to true when a position correction is applied. Used to avoid recalculating velocity when this occurs. */
    UPROPERTY(Transient)
    uint32 bPositionCorrected:1;

    /** Was avoidance updated in this frame? */
    UPROPERTY(Transient)
    bool bUseRVOAvoidance;

    /** Was avoidance updated in this frame? */
    UPROPERTY(Transient)
    bool bWasAvoidanceUpdated;

    /** Update Velocity based on input */
    virtual void ApplyControlInputToVelocity(float DeltaTime);

    /** Decelerate current velocity */
    virtual void Decelerate(float DeltaTime);

    /** calculate RVO avoidance and apply it to current velocity */
    virtual void CalcAvoidanceVelocity();

private:

    // Turn range properties
    bool bUseTurnRange;
    float CurrentTurnRate;
    float TurnInterpRangeInv;
    FVector LastTurnDelta;

    // Velocity orientation, implicitly updated component orientation
    FQuat VelocityOrientation;

    void RefreshTurnRange();
    void PrepareTurnRange(float DeltaTime, const FVector& VelocityNormal, const FVector& InputNormal);
    void ClearTurnRange();
    void InterpOrientation(FQuat& Orientation, const FVector& Current, const FVector& Target, float DeltaTime, float RotationSpeedDegrees);

    FORCEINLINE FVector GetControlInput() const;
};
