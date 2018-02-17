// 

/**
 * Movement component meant for use with SmoothDeltaActor.
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameFramework/MovementComponent.h"
#include "VSmoothDeltaMovementComponent.generated.h"

class AVSmoothDeltaActor;

/** 
 *
 */
UCLASS(ClassGroup=Movement, meta=(BlueprintSpawnableComponent))
class STEERINGSYSTEMPLUGIN_API UVSmoothDeltaMovementComponent : public UMovementComponent
{
	GENERATED_BODY()

public:

    /** Movement duration for each cycle. */
    UPROPERTY(Category="Smooth Delta Movement (General Settings)", EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0"))
    float Duration;

    /** Movement duration for each cycle. */
    UPROPERTY(Category="Smooth Delta Movement (General Settings)", EditAnywhere, BlueprintReadWrite)
    bool bSnapToCurrentOnSourceChange;

    /**
    * Max distance we allow simulated proxies to depenetrate when moving out of anything but Pawns.
    * This is generally more tolerant than with Pawns, because other geometry is either not moving,
    * or is moving predictably with a bit of delay compared to on the server.
    *
    * @see MaxDepenetrationWithGeometryAsProxy, MaxDepenetrationWithPawn, MaxDepenetrationWithPawnAsProxy
    */
    UPROPERTY(Category="Smooth Delta Movement (General Settings)", EditAnywhere, BlueprintReadWrite, AdvancedDisplay, meta=(ClampMin="0", UIMin="0"))
    float MaxDepenetrationWithGeometry;

    /**
    * Max distance we allow simulated proxies to depenetrate when moving out of anything but Pawns.
    * This is generally more tolerant than with Pawns, because other geometry is either not moving,
    * or is moving predictably with a bit of delay compared to on the server.
    *
    * @see MaxDepenetrationWithGeometry, MaxDepenetrationWithPawn, MaxDepenetrationWithPawnAsProxy
    */
    UPROPERTY(Category="Smooth Delta Movement (General Settings)", EditAnywhere, BlueprintReadWrite, AdvancedDisplay, meta=(ClampMin="0", UIMin="0"))
    float MaxDepenetrationWithGeometryAsProxy;

    /**
    * Max distance we are allowed to depenetrate when moving out of other Pawns.
    *
    * @see MaxDepenetrationWithGeometry, MaxDepenetrationWithGeometryAsProxy, MaxDepenetrationWithPawnAsProxy
    */
    UPROPERTY(Category="Smooth Delta Movement (General Settings)", EditAnywhere, BlueprintReadWrite, AdvancedDisplay, meta=(ClampMin="0", UIMin="0"))
    float MaxDepenetrationWithPawn;

    /**
     * Max distance we allow simulated proxies to depenetrate when moving out of other Pawns.
     * Typically we don't want a large value, because we receive a server authoritative position that we should not then ignore by pushing them out of the local player.
     *
     * @see MaxDepenetrationWithGeometry, MaxDepenetrationWithGeometryAsProxy, MaxDepenetrationWithPawn
     */
    UPROPERTY(Category="Smooth Delta Movement (General Settings)", EditAnywhere, BlueprintReadWrite, AdvancedDisplay, meta=(ClampMin="0", UIMin="0"))
    float MaxDepenetrationWithPawnAsProxy;

    /**
     * If true, high-level movement updates will be wrapped in a movement scope that accumulates updates and defers a bulk of the work until the end.
     * When enabled, touch and hit events will not be triggered until the end of multiple moves within an update, which can improve performance.
     *
     * @see FScopedMovementUpdate
     */
    UPROPERTY(Category="Smooth Delta Movement (General Settings)", EditAnywhere, AdvancedDisplay)
    uint32 bEnableScopedMovementUpdates:1;

    /**
     * Used by movement code to determine if a change in position is based on normal movement or a teleport.
     * If not a teleport, velocity can be recomputed based on the change in position.
     */
    UPROPERTY(Category="Smooth Delta Movement (General Settings)", Transient, VisibleInstanceOnly, BlueprintReadWrite)
    uint32 bJustTeleported:1;

    /** Mass of pawn (for when momentum is imparted to it). */
    UPROPERTY(Category="Character Movement (Physics Interaction)", EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0"))
    float Mass;

    /** If enabled, the player will interact with physics objects when walking into them. */
    UPROPERTY(Category="Character Movement (Physics Interaction)", EditAnywhere, BlueprintReadWrite)
    bool bEnablePhysicsInteraction;

    /** If enabled, the TouchForceFactor is applied per kg mass of the affected object. */
    UPROPERTY(Category="Character Movement (Physics Interaction)", EditAnywhere, BlueprintReadWrite, meta=(editcondition = "bEnablePhysicsInteraction"))
    bool bTouchForceScaledToMass;

    /** If enabled, the PushForceFactor is applied per kg mass of the affected object. */
    UPROPERTY(Category="Character Movement (Physics Interaction)", EditAnywhere, BlueprintReadWrite, meta=(editcondition = "bEnablePhysicsInteraction"))
    bool bPushForceScaledToMass;

    /** If enabled, the applied push force will try to get the physics object to the same velocity than the player, not faster. This will only
        scale the force down, it will never apply more force than defined by PushForceFactor. */
    UPROPERTY(Category="Character Movement (Physics Interaction)", EditAnywhere, BlueprintReadWrite, meta=(editcondition = "bEnablePhysicsInteraction"))
    bool bScalePushForceToVelocity;

    /** Initial impulse force to apply when the player bounces into a blocking physics object. */
    UPROPERTY(Category="Character Movement (Physics Interaction)", EditAnywhere, BlueprintReadWrite, meta=(editcondition = "bEnablePhysicsInteraction"))
    float InitialPushForceFactor;

    /** Force to apply when the player collides with a blocking physics object. */
    UPROPERTY(Category="Character Movement (Physics Interaction)", EditAnywhere, BlueprintReadWrite, meta=(editcondition = "bEnablePhysicsInteraction"))
    float PushForceFactor;

    /** Force to apply to physics objects that are touched by the player. */
    UPROPERTY(Category="Character Movement (Physics Interaction)", EditAnywhere, BlueprintReadWrite, meta=(editcondition = "bEnablePhysicsInteraction"))
    float TouchForceFactor;

    /** Minimum Force applied to touched physics objects. If < 0.0f, there is no minimum. */
    UPROPERTY(Category="Character Movement (Physics Interaction)", EditAnywhere, BlueprintReadWrite, meta=(editcondition = "bEnablePhysicsInteraction"))
    float MinTouchForce;

    /** Maximum force applied to touched physics objects. If < 0.0f, there is no maximum. */
    UPROPERTY(Category="Character Movement (Physics Interaction)", EditAnywhere, BlueprintReadWrite, meta=(editcondition = "bEnablePhysicsInteraction"))
    float MaxTouchForce;

    /** Force per kg applied constantly to all overlapping components. */
    UPROPERTY(Category="Character Movement (Physics Interaction)", EditAnywhere, BlueprintReadWrite, meta=(editcondition = "bEnablePhysicsInteraction"))
    float RepulsionForce;

    /**
     * How long to take to smoothly interpolate from the old pawn position on the client to the corrected one sent by the server. Not used by Linear smoothing.
     */
    UPROPERTY(Category="Smooth Delta Movement (Networking)", EditDefaultsOnly, AdvancedDisplay, meta=(ClampMin="0.0", ClampMax="1.0", UIMin="0.0", UIMax="1.0"))
    float NetworkSmoothSimulatedTime;

    /**
    * Similar setting as NetworkSmoothSimulatedTime but only used on Listen servers.
    */
    UPROPERTY(Category="Smooth Delta Movement (Networking)", EditDefaultsOnly, AdvancedDisplay, meta=(ClampMin="0.0", ClampMax="1.0", UIMin="0.0", UIMax="1.0"))
    float ListenServerNetworkSmoothSimulatedTime;

    /** Maximum distance character is allowed to lag behind server location when interpolating between updates. */
    UPROPERTY(Category="Smooth Delta Movement (Networking)", EditDefaultsOnly, meta=(ClampMin="0.0", UIMin="0.0"))
    float NetworkMaxSmoothUpdateDistance;

    /**
     * Maximum distance beyond which character is teleported to the new server location without any smoothing.
     */
    UPROPERTY(Category="Smooth Delta Movement (Networking)", EditDefaultsOnly, meta=(ClampMin="0.0", UIMin="0.0"))
    float NetworkNoSmoothUpdateDistance;

    /** Smoothing mode for simulated proxies in network game. */
    UPROPERTY(Category="Smooth Delta Movement (Networking)", EditAnywhere, BlueprintReadOnly)
    ENetworkSmoothingMode NetworkSmoothingMode;

    /** True when a network replication update is received for simulated proxies. */
    UPROPERTY(Transient)
    uint32 bNetworkUpdateReceived:1;

    /**
     * Signals that smoothed position/rotation has reached target, and no more smoothing is necessary until a future update.
     * This is used as an optimization to skip calls to SmoothClientPosition() when true. SmoothCorrection() sets it false when a new network update is received.
     * SmoothClientPosition_Interpolate() sets this to true when the interpolation reaches the target, before one last call to SmoothClientPosition_UpdateVisuals().
     * If this is not desired, override SmoothClientPosition() to always set this to false to avoid this feature.
     */
    uint32 bNetworkSmoothingComplete:1;

    /**
     * Minimum delta time considered when ticking.
     * Delta times below this are not considered. This is a very small non-zero value to avoid potential divide-by-zero in simulation code.
     */
    static const float MIN_TICK_TIME;

    /**
     * Default UObject constructor.
     */
    UVSmoothDeltaMovementComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/**
     * Overridden to only allow registration with components owned by a VSmoothDeltaActor.
     */
	virtual void SetUpdatedComponent(USceneComponent* NewUpdatedComponent) override;
	virtual void Serialize(FArchive& Ar) override;

	/** Return the Pawn that owns UpdatedComponent. */
	UFUNCTION(BlueprintCallable, Category="SmoothDeltaActor|Components|SmoothDeltaMovement")
	AVSmoothDeltaActor* GetSDOwner() const;

    /** Get the value of ServerLastUpdateTimeStamp. */
    FORCEINLINE float GetServerLastUpdateTimeStamp() const
    {
        return ServerLastUpdateTimeStamp;
    }

    /** Get the value of ServerLastSimulationTime. */
    FORCEINLINE float GetServerLastSimulationTime() const
    {
        return ServerLastSimulationTime;
    }

    /** Event called after owning actor movement source changes. */
    virtual void MovementSourceChange();

    //Begin UActorComponent Interface
    virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;
    //End UActorComponent Interface

    /**
     * React to new transform from network update. Sets bNetworkSmoothingComplete to false to ensure future smoothing updates.
     * IMPORTANT: It is expected that this function triggers any movement/transform updates to match the network update if desired.
     */
    virtual void SmoothCorrection(float NewServerSimulationTime);

    /**
     * Reset existing client smoothing data.
     */
    virtual void ResetClientSmoothingData();

    /** Applies repulsion force to all touched components. */
    virtual void ApplyRepulsionForce(float DeltaTime);

    /** Applies momentum accumulated through AddImpulse() and AddForce(), then clears those forces. */
    virtual void ApplyAccumulatedForces(float DeltaTime);

    /** Clears forces accumulated through AddImpulse() and AddForce(), and also pending launch velocity. */
	UFUNCTION(BlueprintCallable, Category="SmoothDeltaActor|Components|SmoothDeltaMovement")
    virtual void ClearAccumulatedForces();

    /** Checks whether movement data is valid */
    virtual bool HasValidData() const;

    virtual float GetClampedSmoothSimulationOffset(float SimulationTimeDelta) const;
    FORCEINLINE float GetClampedSimulationTime(float InSimulationTime) const;

protected:

	/** Pawn that owns this component. */
	UPROPERTY(Transient, DuplicateTransient)
	AVSmoothDeltaActor* SDOwner;

    /**
     * Current simulation time.
     */
    UPROPERTY()
    float CurrentSimulationTime;

    /**
     * Current simulation time. Used to perform smooth simulated movement on simulated proxies.
     */
    UPROPERTY()
    float SmoothSimulationOffset;

    /** Timestamp when location or rotation last changed during an update. Only valid on the server. */
    UPROPERTY(Transient)
    float ServerLastUpdateTimeStamp;

    /** Timestamp when location or rotation last changed during an update. Only valid on the server. */
    UPROPERTY(Transient)
    float ServerLastSimulationTime;

    /**
     * Location after last PerformMovement or SimulateMovement update.
     * Used internally to detect changes in velocity from external sources.
     */
    UPROPERTY()
    FVector LastUpdateLocation;

    /**
     * Rotation after last PerformMovement or SimulateMovement update.
     * Used internally to detect changes in velocity from external sources.
     */
    UPROPERTY()
    FQuat LastUpdateRotation;

    /**
     * Velocity after last PerformMovement or SimulateMovement update.
     * Used internally to detect changes in velocity from external sources.
     */
    UPROPERTY()
    FVector LastUpdateVelocity;

    /** Accumulated impulse to be resolved next tick. */
    UPROPERTY()
    FVector PendingImpulseToApply;

    /** Accumulated force to be resolved next tick. */
    UPROPERTY()
    FVector PendingForceToApply;

    /**
     * True during movement update.
     * Used internally so that attempts to change CharacterOwner and UpdatedComponent are deferred until after an update.
     * @see IsMovementInProgress()
     */
    UPROPERTY()
    uint32 bMovementInProgress:1;

    /** Indicate that new updated component need to be set after performing movement update */
    UPROPERTY()
    uint32 bDeferUpdateMoveComponent:1;

    /** New updated component that need to be set after performing movement update */
    UPROPERTY()
    USceneComponent* DeferredUpdatedMoveComponent;

    // Movement functions broken out based on owner's network Role.
    // TickComponent calls the correct version based on the Role.
    // These may be called during move playback and correction during network updates.
    //

    /** Perform movement on an server */
    virtual void PerformMovement(float DeltaTime);

    /** Starts movement update functions called by PerformMovement() on the server */
    virtual void StartMovementUpdate(float DeltaTime);

    /** Actual server movement update implementation */
    virtual void MovementUpdate(float DeltaTime);

    /** Tick for simulated proxies */
    void SimulatedTick(float DeltaTime);

    /** Simulate movement on a non-owning client. Called by SimulatedTick(). */
    virtual void SimulateMovement(float DeltaTime);

    /**
     * Moves along the given movement direction using simple movement rules based on the current movement mode (usually used by simulated proxies).
     */
    virtual void MoveSmooth(float DeltaTime);
    virtual void MoveSmooth_Visual();

    /** Update transform to the current simulation time */
    virtual void SnapToCurrentTime();

    /**
     * Smooth mesh location for network interpolation, based on values set up by SmoothCorrection.
     *
     * Internally this simply calls SmoothClientPosition_Interpolate() then SmoothClientPosition_UpdateVisuals().
     * This function is not called when bNetworkSmoothingComplete is true.
     */
    virtual void SmoothClientPosition(float DeltaTime);

    /**
     * Update interpolation values for client smoothing. Does not change actual mesh location.
     * Sets bNetworkSmoothingComplete to true when the interpolation reaches the target.
     */
    void SmoothClientPosition_Interpolate(float DeltaTime);

    /** Update mesh location based on interpolated values. */
    void SmoothClientPosition_UpdateVisuals();

    /** Called when the collision capsule touches another primitive component */
    UFUNCTION()
    virtual void PrimitiveTouched(UPrimitiveComponent* OverlappedComp, AActor* Other, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

    /**
     * Event triggered at the end of a movement update. If scoped movement updates are enabled (bEnableScopedMovementUpdates), this is within such a scope.
     * If that is not desired, bind to the CharacterOwner's OnMovementUpdated event instead, as that is triggered after the scoped movement update.
     */
    virtual void OnMovementUpdated(float DeltaTime, const FVector& OldLocation, const FVector& OldVelocity);

    /** Internal function to call OnMovementUpdated delegate on CharacterOwner. */
    virtual void CallMovementUpdateDelegate(float DeltaTime, const FVector& OldLocation, const FVector& OldVelocity);

    /**
     * Apply physics forces to the impacted component, if bEnablePhysicsInteraction is true.
     * @param Impact                HitResult that resulted in the impact
     * @param ImpactAcceleration    Acceleration of the character at the time of impact
     * @param ImpactVelocity        Velocity of the character at the time of impact
     */
    virtual void ApplyImpactPhysicsForces(const FHitResult& Impact, const FVector& ImpactVelocity);

    /** Overridden to enforce max distances based on hit geometry. */
    virtual FVector GetPenetrationAdjustment(const FHitResult& Hit) const override;

    /** Overridden to set bJustTeleported to true, so we don't make incorrect velocity calculations based on adjusted movement. */
    virtual bool ResolvePenetrationImpl(const FVector& Adjustment, const FHitResult& Hit, const FQuat& NewRotation) override;

    /** Handle a blocking impact. Calls ApplyImpactPhysicsForces for the hit, if bEnablePhysicsInteraction is true. */
    virtual void HandleImpact(const FHitResult& Hit, float TimeSlice=0.f, const FVector& MoveDelta = FVector::ZeroVector) override;

    /** Custom version of SlideAlongSurface that handles different movement modes separately; namely during walking physics we might not want to slide up slopes. */
    virtual float SlideAlongSurface(const FVector& Delta, float Time, const FVector& Normal, FHitResult& Hit, bool bHandleImpact) override;

    /**
     * On the server if we know we are having our replication rate throttled,
     * this method checks if important replicated properties have changed that should cause us to return to the normal replication rate.
     */
    virtual bool ShouldCancelAdaptiveReplication() const;
};
