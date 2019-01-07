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
#include "WorldCollision.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "Engine/NetSerialization.h"
#include "Engine/EngineTypes.h"
#include "Engine/EngineBaseTypes.h"
#include "Interfaces/NetworkPredictionInterface.h"
#include "VPMovementComponent.h"
#include "VPCMovementTypes.h"
#include "VPCMovementComponent.generated.h"

class AVPawnChar;
class FDebugDisplayInfo;
class UPrimitiveComponent;
class UCanvas;
class UVPCMovementComponent;
class URVO3DAgentComponent;

//=============================================================================
/**
 * CharacterMovementComponent handles movement logic for the associated Character owner.
 *
 * Movement is affected primarily by current Velocity and Acceleration. Acceleration is updated each frame
 * based on the input vector accumulated thus far (see UVPMovementComponent::GetPendingInputVector()).
 *
 * Networking is fully implemented, with server-client correction and prediction included.
 *
 * @see AVPawnChar, UVPMovementComponent
 */

UCLASS(ClassGroup=Movement, meta=(BlueprintSpawnableComponent))
class STEERINGSYSTEMPLUGIN_API UVPCMovementComponent : public UVPMovementComponent, public INetworkPredictionInterface
{
    GENERATED_BODY()
public:

    /**
     * Default UObject constructor.
     */
    UVPCMovementComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:

    /** Character movement component belongs to */
    UPROPERTY(Transient, DuplicateTransient)
    AVPawnChar* CharacterOwner;

public:
    
    /**
     * Actor's current movement mode (walking, falling, etc).
     *    - walking:  Walking on a surface, under the effects of friction, and able to "step up" barriers. Vertical velocity is zero.
     *    - falling:  Falling under the effects of gravity, after jumping or walking off the edge of a surface.
     *    - flying:   Flying, ignoring the effects of gravity.
     *    - swimming: Swimming through a fluid volume, under the effects of gravity and buoyancy.
     *    - custom:   User-defined custom movement mode, including many possible sub-modes.
     * This is automatically replicated through the Character owner and for client-server movement functions.
     * @see SetMovementMode(), CustomMovementMode
     */
    UPROPERTY(Category="Character Movement (Movement Mode)", BlueprintReadOnly)
    TEnumAsByte<enum EMovementMode> MovementMode;

    /**
     * Current custom sub-mode if MovementMode is set to Custom.
     * This is automatically replicated through the Character owner and for client-server movement functions.
     * @see SetMovementMode()
     */
    UPROPERTY(Category="Character Movement (Movement Mode)", BlueprintReadOnly)
    uint8 CustomMovementMode;

    /** Saved location of object we are standing on, for UpdateBasedMovement() to determine if base moved in the last frame, and therefore pawn needs an update. */
    FVector OldBaseLocation;

    /** Saved location of object we are standing on, for UpdateBasedMovement() to determine if base moved in the last frame, and therefore pawn needs an update. */
    FQuat OldBaseQuat;

    /** The maximum speed. */
    UPROPERTY(Category="Character Movement", EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0"))
    float MaxSpeed;

    /** The maximum speed on locked orientation move. */
    UPROPERTY(Category="Character Movement", EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0"))
    float MaxLockedOrientationSpeed;

    /** The maximum speed when using Custom movement mode. */
    UPROPERTY(Category="Character Movement (Custom Movement)", EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0"))
    float MaxCustomMovementSpeed;

    /** Max Acceleration (rate of change of velocity) */
    UPROPERTY(Category="Character Movement", EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0"))
    float MaxAcceleration;

    /**
     * Maximum deceleration with non-zero acceleration.
     * @see MaxAcceleration
     */
    UPROPERTY(Category="Character Movement", EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0"))
    float MaxDeceleration;

    /**
     * Maximum deceleration with zero acceleration.
     * @see MaxAcceleration
     */
    UPROPERTY(Category="Character Movement", EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0"))
    float MaxZeroInputDeceleration;

    /**
     * Factor used to multiply actual value of friction used when braking.
     * This applies to any friction value that is currently used, which may depend on bUseSeparateBrakingFriction.
     * @note This is 2 by default for historical reasons, a value of 1 gives the true drag equation.
     * @see bUseSeparateBrakingFriction, GroundFriction, BrakingFriction
     */
    UPROPERTY(Category="Character Movement", EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0"))
    float BrakingFrictionFactor;

    /**
     * Friction (drag) coefficient applied when braking (whenever Acceleration = 0, or if character is exceeding max speed);
     * actual value used is this multiplied by BrakingFrictionFactor.
     *
     * When braking, this property allows you to control how much friction is applied when moving across the ground,
     * applying an opposing force that scales with current velocity.
     *
     * Braking is composed of friction (velocity-dependent drag) and constant deceleration.
     * This is the current value, used in all movement modes; if this is not desired, override it or bUseSeparateBrakingFriction when movement mode changes.
     *
     * @note Only used if bUseSeparateBrakingFriction setting is true, otherwise current friction such as GroundFriction is used.
     * @see bUseSeparateBrakingFriction, BrakingFrictionFactor, GroundFriction, BrakingDecelerationWalking
     */
    UPROPERTY(Category="Character Movement", EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0", EditCondition="bUseSeparateBrakingFriction"))
    float BrakingFriction;

    /**
     * If true, BrakingFriction will be used to slow the character to a stop (when there is no Acceleration).
     * If false, braking uses the same friction passed to CalcVelocity() (ie GroundFriction when walking), multiplied by BrakingFrictionFactor.
     * This setting applies to all movement modes; if only desired in certain modes, consider toggling it when movement modes change.
     * @see BrakingFriction
     */
    UPROPERTY(Category="Character Movement", EditDefaultsOnly, BlueprintReadWrite)
    uint32 bUseSeparateBrakingFriction:1;

    /**
     * If true, overlapping physics volume friction will be used to slow the velocity / acceleration.
     */
    UPROPERTY(Category="Character Movement", EditDefaultsOnly, BlueprintReadWrite)
    uint32 bEnableVolumeFriction:1;

    /**
     * Change in rotation per second, used when UseControllerDesiredRotation or OrientRotationToMovement are true.
     * Set a negative value for infinite rotation rate and instant turns.
     */
    UPROPERTY(Category="Character Movement (Rotation Settings)", EditAnywhere, BlueprintReadWrite)
    FRotator RotationRate;

    /**
     * If true, rotate the Character toward the direction of acceleration, using RotationRate as the rate of rotation change. Overrides UseControllerDesiredRotation.
     * Normally you will want to make sure that other settings are cleared, such as bUseControllerRotationYaw on the Character.
     */
    UPROPERTY(Category="Character Movement (Rotation Settings)", EditAnywhere, BlueprintReadWrite)
    uint32 bOrientRotationToMovement:1;

protected:

    /**
     * True during movement update.
     * Used internally so that attempts to change CharacterOwner and UpdatedComponent are deferred until after an update.
     * @see IsMovementInProgress()
     */
    UPROPERTY()
    uint32 bMovementInProgress:1;

public:

    /**
     * If true, high-level movement updates will be wrapped in a movement scope that accumulates updates and defers a bulk of the work until the end.
     * When enabled, touch and hit events will not be triggered until the end of multiple moves within an update, which can improve performance.
     *
     * @see FScopedMovementUpdate
     */
    UPROPERTY(Category="Character Movement", EditAnywhere, AdvancedDisplay)
    uint32 bEnableScopedMovementUpdates:1;

    /** Ignores size of acceleration component, and forces max acceleration to drive character at full velocity. */
    UPROPERTY()
    uint32 bForceMaxAccel:1;    

    /**
     * Signals that smoothed position/rotation has reached target, and no more smoothing is necessary until a future update.
     * This is used as an optimization to skip calls to SmoothClientPosition() when true. SmoothCorrection() sets it false when a new network update is received.
     * SmoothClientPosition_Interpolate() sets this to true when the interpolation reaches the target, before one last call to SmoothClientPosition_UpdateVisuals().
     * If this is not desired, override SmoothClientPosition() to always set this to false to avoid this feature.
     */
    uint32 bNetworkSmoothingComplete:1;

public:

    /** true to update CharacterOwner and UpdatedComponent after movement ends */
    UPROPERTY()
    uint32 bDeferUpdateMoveComponent:1;

    /** What to update CharacterOwner and UpdatedComponent after movement ends */
    UPROPERTY()
    USceneComponent* DeferredUpdatedMoveComponent;

    /** Mass of pawn (for when momentum is imparted to it). */
    UPROPERTY(Category="Character Movement", EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0"))
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

protected:

    /**
     * Current acceleration vector (with magnitude).
     * This is calculated each update based on the input vector and the constraints of MaxAcceleration and the current movement mode.
     */
    UPROPERTY()
    FVector Acceleration;

    /**
     * Location after last PerformMovement or SimulateMovement update.
     * Used internally to detect changes in position from outside character movement to try to validate the current floor.
     */
    UPROPERTY()
    FVector LastUpdateLocation;

    /**
     * Rotation after last PerformMovement or SimulateMovement update.
     */
    UPROPERTY()
    FQuat LastUpdateRotation;

    /**
     * Velocity after last PerformMovement or SimulateMovement update. Used internally to detect changes in velocity from external sources.
     */
    UPROPERTY()
    FVector LastUpdateVelocity;

    /** Timestamp when location or rotation last changed during an update. Only valid on the server. */
    UPROPERTY(Transient)
    float ServerLastTransformUpdateTimeStamp;

    /** Accumulated impulse to be added next tick. */
    UPROPERTY()
    FVector PendingImpulseToApply;

    /** Accumulated force to be added next tick. */
    UPROPERTY()
    FVector PendingForceToApply;

    /**
     * Modifier to applied to values such as acceleration and max speed due to analog input.
     */
    UPROPERTY()
    float AnalogInputModifier;

    /** Computes the analog input modifier based on current input vector and/or acceleration. */
    virtual float ComputeAnalogInputModifier() const;

    /** Used for throttling "stuck in geometry" logging. */
    float LastStuckWarningTime;

    /** Used when throttling "stuck in geometry" logging, to output the number of events we skipped if throttling. */
    uint32 StuckWarningCountSinceNotify;

public:

    /** Get the value of ServerLastTransformUpdateTimeStamp. */
    FORCEINLINE float GetServerLastTransformUpdateTimeStamp() const { return ServerLastTransformUpdateTimeStamp;  }

    /**
    * Max distance we allow simulated proxies to depenetrate when moving out of anything but Pawns.
    * This is generally more tolerant than with Pawns, because other geometry is either not moving,
    * or is moving predictably with a bit of delay compared to on the server.
    *
    * @see MaxDepenetrationWithGeometryAsProxy, MaxDepenetrationWithPawn, MaxDepenetrationWithPawnAsProxy
    */
    UPROPERTY(Category="Character Movement", EditAnywhere, BlueprintReadWrite, AdvancedDisplay, meta=(ClampMin="0", UIMin="0"))
    float MaxDepenetrationWithGeometry;

    /**
    * Max distance we allow simulated proxies to depenetrate when moving out of anything but Pawns.
    * This is generally more tolerant than with Pawns, because other geometry is either not moving,
    * or is moving predictably with a bit of delay compared to on the server.
    *
    * @see MaxDepenetrationWithGeometry, MaxDepenetrationWithPawn, MaxDepenetrationWithPawnAsProxy
    */
    UPROPERTY(Category="Character Movement", EditAnywhere, BlueprintReadWrite, AdvancedDisplay, meta=(ClampMin="0", UIMin="0"))
    float MaxDepenetrationWithGeometryAsProxy;

    /**
    * Max distance we are allowed to depenetrate when moving out of other Pawns.
    * @see MaxDepenetrationWithGeometry, MaxDepenetrationWithGeometryAsProxy, MaxDepenetrationWithPawnAsProxy
    */
    UPROPERTY(Category="Character Movement", EditAnywhere, BlueprintReadWrite, AdvancedDisplay, meta=(ClampMin="0", UIMin="0"))
    float MaxDepenetrationWithPawn;

    /**
     * Max distance we allow simulated proxies to depenetrate when moving out of other Pawns.
     * Typically we don't want a large value, because we receive a server authoritative position that
     * we should not then ignore by pushing them out of the local player.
     *
     * @see MaxDepenetrationWithGeometry, MaxDepenetrationWithGeometryAsProxy, MaxDepenetrationWithPawn
     */
    UPROPERTY(Category="Character Movement", EditAnywhere, BlueprintReadWrite, AdvancedDisplay, meta=(ClampMin="0", UIMin="0"))
    float MaxDepenetrationWithPawnAsProxy;

    /** Maximum distance character is allowed to lag behind server location when interpolating between updates. */
    UPROPERTY(Category="Character Movement (Networking)", EditDefaultsOnly, meta=(ClampMin="0.0", UIMin="0.0"))
    float NetworkMaxSmoothUpdateDistance;

    /**
     * Maximum distance beyond which character is teleported to the new server location without any smoothing.
     */
    UPROPERTY(Category="Character Movement (Networking)", EditDefaultsOnly, meta=(ClampMin="0.0", UIMin="0.0"))
    float NetworkNoSmoothUpdateDistance;

    /** Smoothing mode for simulated proxies in network game. */
    UPROPERTY(Category="Character Movement (Networking)", EditAnywhere, BlueprintReadOnly)
    ENetworkSmoothingMode NetworkSmoothingMode;

    /** Whether to enable adaptive replication cancelling. */
    UPROPERTY(Category="Character Movement (Networking)", EditAnywhere, BlueprintReadOnly)
    bool bCancelAdaptiveReplicationEnabled;

    /**
     * How long to take to smoothly interpolate from the old pawn position on the client to the corrected one sent by the server. Not used by Linear smoothing.
     */
    UPROPERTY(Category="Character Movement (Networking)", EditDefaultsOnly, AdvancedDisplay, meta=(ClampMin="0.0", ClampMax="1.0", UIMin="0.0", UIMax="1.0"))
    float NetworkSimulatedSmoothLocationTime;

    /**
     * How long to take to smoothly interpolate from the old pawn rotation on the client to the corrected one sent by the server. Not used by Linear smoothing.
     */
    UPROPERTY(Category="Character Movement (Networking)", EditDefaultsOnly, AdvancedDisplay, meta=(ClampMin="0.0", ClampMax="1.0", UIMin="0.0", UIMax="1.0"))
    float NetworkSimulatedSmoothRotationTime;

    /**
    * Similar setting as NetworkSimulatedSmoothLocationTime but only used on Listen servers.
    */
    UPROPERTY(Category="Character Movement (Networking)", EditDefaultsOnly, AdvancedDisplay, meta=(ClampMin="0.0", ClampMax="1.0", UIMin="0.0", UIMax="1.0"))
    float ListenServerNetworkSimulatedSmoothLocationTime;

    /**
    * Similar setting as NetworkSimulatedSmoothRotationTime but only used on Listen servers.
    */
    UPROPERTY(Category="Character Movement (Networking)", EditDefaultsOnly, AdvancedDisplay, meta=(ClampMin="0.0", ClampMax="1.0", UIMin="0.0", UIMax="1.0"))
    float ListenServerNetworkSimulatedSmoothRotationTime;

public:

    /**
     * Used by movement code to determine if a change in position is based on normal movement or a teleport.
     * If not a teleport, velocity can be recomputed based on the change in position.
     */
    UPROPERTY(Category="Character Movement", Transient, VisibleInstanceOnly, BlueprintReadWrite)
    uint32 bJustTeleported:1;

    /** True when a network replication update is received for simulated proxies. */
    UPROPERTY(Transient)
    uint32 bNetworkUpdateReceived:1;

    /** True when the networked movement mode has been replicated. */
    UPROPERTY(Transient)
    uint32 bNetworkMovementModeChanged:1;

    /**
     * Whether the character ignores changes in rotation of the base it is standing on.
     * If true, the character maintains current world rotation.
     * If false, the character rotates with the moving base.
     */
    UPROPERTY(Category="Character Movement", EditAnywhere, BlueprintReadWrite)
    uint32 bIgnoreBaseRotation:1;

    /** 
     * Set this to true if riding on a moving base that you know is clear from non-moving world obstructions.
     * Optimization to avoid sweeps during based movement, use with care.
     */
    UPROPERTY(Transient, Category="Character Movement", EditAnywhere, BlueprintReadWrite)
    uint32 bFastAttachedMove:1;

protected:

    /** Flag set in pre-physics update to indicate that based movement should be updated post-physics */
    uint32 bDeferUpdateBasedMovement:1;

public:

    /** If true, search for the owner's RVO agent component as the RVOAgentComponent if there is not one currently assigned. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Component)
    bool bAutoRegisterRVOAgentComponent;

    /** If set, component will use RVO avoidance. This only runs on the server. */
    UPROPERTY(Category="Character Movement (Avoidance)", EditAnywhere, BlueprintReadOnly)
    uint32 bUseRVOAvoidance:1;

    /** Whether current RVO performed with ZeroVelocityRVOThreshold velocity */
    UPROPERTY(Category="Character Movement (Avoidance)", EditAnywhere, BlueprintReadOnly)
    uint32 bZeroVelocityRVO:1;

    /** Minimum velocity magnitude above zero to be considered as zero velocity rvo */
    UPROPERTY(Category="Character Movement (Avoidance)", EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0", UIMin="0", ClampMax="1", UIMax="1"))
    float ZeroVelocityRVOThreshold;

    /** Zero velocity rvo lock timer duration */
    UPROPERTY(Category="Character Movement (Avoidance)", EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0", UIMin="0", ClampMax="1", UIMax="1"))
    float ZeroVelocityRVOLockDuration;

    /** Maximum velocity magnitude override for zero velocity rvo */
    UPROPERTY(Category="Character Movement (Avoidance)", EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0", UIMin="0", ClampMax="1", UIMax="1"))
    float ZeroVelocityRVOMaxSpeed;

    /** Minimum avoidance control input magnitude. */
    UPROPERTY(Category="Character Movement (Avoidance)", EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0", UIMin="0", ClampMax="1", UIMax="1"))
    float MinAvoidanceMagnitude;

    /** Change avoidance state and registers in RVO manager if needed */
    UFUNCTION(BlueprintCallable, Category="VPawn|Components|VPCMovement", meta = (UnsafeDuringActorConstruction = "true"))
    void SetAvoidanceEnabled(bool bEnable);

public:

    /** Get the Character that owns UpdatedComponent. */
    UFUNCTION(BlueprintCallable, Category="VPawn|Components|VPCMovement")
    AVPawnChar* GetCharacterOwner() const;

    /**
     * Change movement mode.
     *
     * @param NewMovementMode   The new movement mode
     * @param NewCustomMode     The new custom sub-mode, only applicable if NewMovementMode is Custom.
     */
    UFUNCTION(BlueprintCallable, Category="VPawn|Components|VPCMovement")
    virtual void SetMovementMode(EMovementMode NewMovementMode, uint8 NewCustomMode = 0);

protected:

    /** Called after MovementMode has changed. Base implementation does special handling for starting certain modes, then notifies the CharacterOwner. */
    virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode);

public:

    uint8 PackNetworkMovementMode() const;
    void UnpackNetworkMovementMode(const uint8 ReceivedMode, TEnumAsByte<EMovementMode>& OutMode, uint8& OutCustomMode) const;
    virtual void ApplyNetworkMovementMode(const uint8 ReceivedMode);

    //Begin UActorComponent Interface
    virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;
    virtual void OnRegister() override;
    virtual void BeginDestroy() override;
    virtual void PostLoad() override;
    virtual void Deactivate() override;
    virtual void RegisterComponentTickFunctions(bool bRegister) override;
    virtual void ApplyWorldOffset(const FVector& InOffset, bool bWorldShift) override;
    //End UActorComponent Interface

    //BEGIN UMovementComponent Interface
    virtual float GetMaxSpeed() const override;
    virtual float GetGravityZ() const override;
    virtual void AddRadialForce(const FVector& Origin, float Radius, float Strength, enum ERadialImpulseFalloff Falloff) override;
    virtual void AddRadialImpulse(const FVector& Origin, float Radius, float Strength, enum ERadialImpulseFalloff Falloff, bool bVelChange) override;
    //END UMovementComponent Interface

    /**
     * @return true if currently performing a movement update.
     * @see bMovementInProgress
     */
    bool IsMovementInProgress() const { return bMovementInProgress; }

    //Begin UVPMovementComponent Interface
    virtual void NotifyBumpedPawn(APawn* BumpedPawn) override;
    virtual void NotifyBumpedPawn(AVPawn* BumpedPawn) override;
    //End UVPMovementComponent Interface

    /** Make movement impossible (sets movement mode to MOVE_None). */
    UFUNCTION(BlueprintCallable, Category="VPawn|Components|VPCMovement")
    virtual void DisableMovement();

    /** Return true if we have a valid CharacterOwner and UpdatedComponent. */
    virtual bool HasValidData() const;

    /** Return PrimitiveComponent we are based on (standing and walking on). */
    UFUNCTION(BlueprintCallable, Category="VPawn|Components|VPCMovement")
    UPrimitiveComponent* GetMovementBase() const;

    /** Update or defer updating of position based on Base movement */
    virtual void MaybeUpdateBasedMovement(float DeltaTime);

    /** Update position based on Base movement */
    virtual void UpdateBasedMovement(float DeltaTime);

    /** Call SaveBaseLocation() if not deferring updates (bDeferUpdateBasedMovement is false). */
    virtual void MaybeSaveBaseLocation();

    /** Update OldBaseLocation and OldBaseQuat if there is a valid movement base, and store the relative location/rotation if necessary. Ignores bDeferUpdateBasedMovement and forces the update. */
    virtual void SaveBaseLocation();

    /** changes physics based on MovementMode */
    virtual void StartNewPhysics(float deltaTime);

    /** Can be overridden to choose to jump based on character velocity, base actor dimensions, etc. */
    virtual FVector GetBestDirectionOffActor(AActor* BaseActor) const; // Calculates the best direction to go to "jump off" an actor.

    /** @return how far to rotate character during the time interval DeltaTime. */
    virtual FRotator GetDeltaRotation(float DeltaTime) const;

    /** @return how far to rotate character during the time interval DeltaTime. */
    static float GetAxisDeltaRotation(float InAxisRotationRate, float DeltaTime);

    /**
      * Compute a target rotation based on current movement. Used by PhysicsRotation() when bOrientRotationToMovement is true.
      * Default implementation targets a rotation based on Acceleration.
      *
      * @param CurrentRotation  - Current rotation of the Character
      * @param DeltaTime        - Time slice for this movement
      * @param DeltaRotation    - Proposed rotation change based simply on DeltaTime * RotationRate
      *
      * @return The target rotation given current movement.
      */
    virtual FRotator ComputeOrientToMovementRotation(const FRotator& CurrentRotation, float DeltaTime, FRotator& DeltaRotation) const;

    /** 
     * Updates Velocity and Acceleration based on the current state, applying the effects of friction and acceleration or deceleration. Does not apply gravity.
     * This is used internally during movement updates. Normally you don't need to call this from outside code, but you might want to use it for custom movement modes.
     *
     * @param   DeltaTime                       time elapsed since last frame.
     * @param   Friction                        coefficient of friction when not accelerating, or in the direction opposite acceleration.
     * @param   bFluid                          true if moving through a fluid, causing Friction to always be applied regardless of acceleration.
     * @param   BrakingDeceleration             deceleration applied when not accelerating, or when exceeding max velocity.
     */
    UFUNCTION(BlueprintCallable, Category="VPawn|Components|VPCMovement")
    virtual void CalcVelocity(float DeltaTime, float Friction, bool bVolumeFrictionEnabled, float BrakingDeceleration);

    /** @return Minimum analog input speed [0..1] for the current state. */
    UFUNCTION(BlueprintCallable, Category = "VPawn|Components|VPCMovement")
    virtual float GetMinAnalogSpeed() const;

    /** @return Maximum acceleration for the current state. */
    UFUNCTION(BlueprintCallable, Category="VPawn|Components|VPCMovement")
    virtual float GetMaxAcceleration() const;

    /** @return Maximum deceleration for the current state when braking (ie when there is no acceleration). */
    UFUNCTION(BlueprintCallable, Category="VPawn|Components|VPCMovement")
    virtual float GetMaxDeceleration() const;

    /** @return Maximum deceleration for the current state when braking (ie when there is no acceleration). */
    UFUNCTION(BlueprintCallable, Category="VPawn|Components|VPCMovement")
    virtual float GetMaxZeroInputDeceleration() const;

    /** @return Current acceleration, computed from input vector each update. */
    UFUNCTION(BlueprintCallable, Category="VPawn|Components|VPCMovement", meta=(Keywords="Acceleration GetAcceleration"))
    FVector GetCurrentAcceleration() const;

    /** @return Modifier [0..1] based on the magnitude of the last input vector, which is used to modify the acceleration and max speed during movement. */
    UFUNCTION(BlueprintCallable, Category="VPawn|Components|VPCMovement")
    float GetAnalogInputModifier() const;

    /** Update the base of the character, which is the PrimitiveComponent we are standing on. */
    virtual void SetBase(UPrimitiveComponent* NewBase, const FName BoneName = NAME_None, bool bNotifyActor=true);

    /** Applies repulsion force to all touched components. */
    virtual void ApplyRepulsionForce(float DeltaTime);

    /** Applies momentum accumulated through AddImpulse() and AddForce(), then clears those forces. */
    virtual void ApplyAccumulatedForces(float DeltaTime);

    /** Clears forces accumulated through AddImpulse() and AddForce(), and also pending launch velocity. */
    UFUNCTION(BlueprintCallable, Category="VPawn|Components|VPCMovement")
    virtual void ClearAccumulatedForces();

    /** Update the character state in PerformMovement right before doing the actual position change */
    virtual void UpdateCharacterStateBeforeMovement();

    /** Update the character state in PerformMovement after the position change. Some rotation updates happen after this. */
    virtual void UpdateCharacterStateAfterMovement();

public:

    /** Called by owning Character upon successful teleport from AActor::TeleportTo(). */
    virtual void OnTeleported() override;

    /** Perform rotation over deltaTime */
    virtual void PhysicsRotation(float DeltaTime);

    /** if true, DesiredRotation will be restricted to only Yaw component in PhysicsRotation() */
    virtual bool HasUnlockedRotation() const;

    /** Set movement mode to the default based on the current physics volume. */
    virtual void SetDefaultMovementMode();

    /**
     * Moves along the given movement direction using simple movement rules based on the current movement mode (usually used by simulated proxies).
     *
     * @param InVelocity:           Velocity of movement
     * @param DeltaTime:         Time over which movement occurs
     * @param OutStepDownResult:    [Out] If non-null, and a floor check is performed, this will be updated to reflect that result.
     */
    virtual void MoveSmooth(const FVector& InVelocity, const float DeltaTime);
    
    virtual void SetUpdatedComponent(USceneComponent* NewUpdatedComponent) override;
    
    /** @Return MovementMode string */
    virtual FString GetMovementName() const;

    /** 
     * Add impulse to character. Impulses are accumulated each tick and applied together
     * so multiple calls to this function will accumulate.
     * An impulse is an instantaneous force, usually applied once. If you want to continually apply
     * forces each frame, use AddForce().
     * Note that changing the momentum of characters like this can change the movement mode.
     * 
     * @param   Impulse             Impulse to apply.
     * @param   bVelocityChange     Whether or not the impulse is relative to mass.
     */
    UFUNCTION(BlueprintCallable, Category="VPawn|Components|VPCMovement")
    virtual void AddImpulse( FVector Impulse, bool bVelocityChange = false );

    /** 
     * Add force to character. Forces are accumulated each tick and applied together
     * so multiple calls to this function will accumulate.
     * Forces are scaled depending on timestep, so they can be applied each frame. If you want an
     * instantaneous force, use AddImpulse.
     * Adding a force always takes the actor's mass into account.
     * Note that changing the momentum of characters like this can change the movement mode.
     * 
     * @param   Force           Force to apply.
     */
    UFUNCTION(BlueprintCallable, Category="VPawn|Components|VPCMovement")
    virtual void AddForce( FVector Force );

    /**
     * Draw important variables on canvas.  Character will call DisplayDebug() on the current ViewTarget when the ShowDebug exec is used
     *
     * @param Canvas - Canvas to draw on
     * @param DebugDisplay - Contains information about what debug data to display
     * @param YL - Height of the current font
     * @param YPos - Y position on Canvas. YPos += YL, gives position to draw text for next debug line.
     */
    virtual void DisplayDebug(UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos);

    /**
     * Draw in-world debug information for character movement (called with p.VisualizeMovement > 0).
     */
    virtual void VisualizeMovement() const;

    /** Post-physics tick function for this character */
    UPROPERTY()
    struct FVPCMovementComponentPostPhysicsTickFunction PostPhysicsTickFunction;

    /** Tick function called after physics (sync scene) has finished simulation, before cloth */
    virtual void PostPhysicsTickComponent(float DeltaTime, FVPCMovementComponentPostPhysicsTickFunction& ThisTickFunction);

protected:

    /** @note Movement update functions should only be called through StartNewPhysics()*/
    virtual void PhysFlying(float DeltaTime);

    /** @note Movement update functions should only be called through StartNewPhysics()*/
    virtual void PhysCustom(float DeltaTime);

    /** Notification that the character is stuck in geometry.  Only called during walking movement. */
    virtual void OnCharacterStuckInGeometry(const FHitResult* Hit);

    /** Overridden to enforce max distances based on hit geometry. */
    virtual FVector GetPenetrationAdjustment(const FHitResult& Hit) const override;

    /** Overridden to set bJustTeleported to true, so we don't make incorrect velocity calculations based on adjusted movement. */
    virtual bool ResolvePenetrationImpl(const FVector& Adjustment, const FHitResult& Hit, const FQuat& NewRotation) override;

    /** Handle a blocking impact. Calls ApplyImpactPhysicsForces for the hit, if bEnablePhysicsInteraction is true. */
    virtual void HandleImpact(const FHitResult& Hit, float TimeSlice=0.f, const FVector& MoveDelta = FVector::ZeroVector) override;

    /**
     * Apply physics forces to the impacted component, if bEnablePhysicsInteraction is true.
     * @param Impact                HitResult that resulted in the impact
     * @param ImpactAcceleration    Acceleration of the character at the time of impact
     * @param ImpactVelocity        Velocity of the character at the time of impact
     */
    virtual void ApplyImpactPhysicsForces(const FHitResult& Impact, const FVector& ImpactAcceleration, const FVector& ImpactVelocity);

    /** Custom version of SlideAlongSurface that handles different movement modes separately; namely during walking physics we might not want to slide up slopes. */
    virtual float SlideAlongSurface(const FVector& Delta, float Time, const FVector& Normal, FHitResult& Hit, bool bHandleImpact) override;

    /** Slows towards stop. */
    virtual void ApplyVelocityBraking(float DeltaTime, float Friction, float BrakingDeceleration);

protected:

    /** Called when the collision capsule touches another primitive component */
    UFUNCTION()
    virtual void PrimitiveTouched(UPrimitiveComponent* OverlappedComp, AActor* Other, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

    /** Enforce constraints on input given current state. For instance, don't move upwards if walking and looking up. */
    virtual FVector ConstrainInputAcceleration(const FVector& InputAcceleration) const;

    /** Scale input acceleration, based on movement acceleration rate. */
    virtual FVector ScaleInputAcceleration(const FVector& InputAcceleration) const;

    /**
     * Event triggered at the end of a movement update. If scoped movement updates are enabled (bEnableScopedMovementUpdates), this is within such a scope.
     * If that is not desired, bind to the CharacterOwner's OnMovementUpdated event instead, as that is triggered after the scoped movement update.
     */
    virtual void OnMovementUpdated(float DeltaTime, const FVector& OldLocation, const FVector& OldVelocity);

    /** Internal function to call OnMovementUpdated delegate on CharacterOwner. */
    virtual void CallMovementUpdateDelegate(float DeltaTime, const FVector& OldLocation, const FVector& OldVelocity);

    /**
     * Event triggered when we are moving on a base but we are not able to move the full DeltaPosition because something has blocked us.
     * Note: MoveComponentFlags includes the flag to ignore the movement base while this event is fired.
     * @param DeltaPosition     How far we tried to move with the base.
     * @param OldLocation       Location before we tried to move with the base.
     * @param MoveOnBaseHit     Hit result for the object we hit when trying to move with the base.
     */
    virtual void OnUnableToFollowBaseMove(const FVector& DeltaPosition, const FVector& OldLocation, const FHitResult& MoveOnBaseHit);

public:

    UFUNCTION(BlueprintCallable, Category="VPawn|Components|VPCMovement")
    float GetCurrentTurnRate() const;

    UFUNCTION(BlueprintCallable, Category="VPawn|Components|VPCMovement")
    float GetTurnRate() const;

    UFUNCTION(BlueprintCallable, Category="VPawn|Components|VPCMovement")
    float GetMinTurnRate() const;

    UFUNCTION(BlueprintCallable, Category="VPawn|Components|VPCMovement")
    float GetTurnAcceleration() const;

    UFUNCTION(BlueprintCallable, Category="VPawn|Components|VPCMovement")
    void SetTurnRate(float InTurnRate);

    UFUNCTION(BlueprintCallable, Category="VPawn|Components|VPCMovement")
    void SetMinTurnRate(float InMinTurnRate);

    UFUNCTION(BlueprintCallable, Category="VPawn|Components|VPCMovement")
    void SetTurnAcceleration(float InTurnAcceleration);

    UFUNCTION(BlueprintCallable, Category="VPawn|Components|VPCMovement", meta=(DisplayName = "RefreshTurnRange"))
    void K2_RefreshTurnRange();

    /** Adjust acceleration and orientation towards control input */
    virtual void PhysicsOrientation(float DeltaTime);

protected:

    /**
     * Turning rate, in degrees/s
     */
    UPROPERTY(EditAnywhere, Category="Character Movement (Orientation)", meta=(ClampMin="0", UIMin="0"))
    float TurnRate;

    /**
     * Minimum turning rate. If non-zero positive, turn rate will be interpolated from a turn speed range of
     * MinTurnRate to TurnRate at TurnAcceleration speed.
     */
    UPROPERTY(EditAnywhere, Category="Character Movement (Orientation)")
    float MinTurnRate;

    /**
     * Interpolation speed of turn rate from MinTurnRate to TurnRate in degrees/s
     */
    UPROPERTY(EditAnywhere, Category="Character Movement (Orientation)", meta=(ClampMin="0", UIMin="0"))
    float TurnAcceleration;

    // Turn range properties
    bool bUseTurnRange;
    float CurrentTurnRate;
    float TurnInterpRangeInv;
    FVector LastTurnDelta;

    // Velocity orientation, implicitly updated component orientation
    FQuat VelocityOrientation;

    // Whether current velocity orientation is locked
    bool bLockedOrientation;

    void RefreshTurnRange();
    void PrepareTurnRange(float DeltaTime, const FVector& VelocityNormal, const FVector& InputNormal);
    void ClearTurnRange();
    static void InterpOrientation(FQuat& Orientation, const FVector& Current, const FVector& Target, float DeltaTime, float RotationSpeedDegrees);

protected:

    // Movement functions broken out based on owner's network Role.
    // TickComponent calls the correct version based on the Role.
    // These may be called during move playback and correction during network updates.
    //

    /** Perform movement on an autonomous client */
    virtual void PerformMovement(float DeltaTime);

    /** Special Tick for Simulated Proxies */
    void SimulatedTick(float DeltaTime);

    /** Simulate movement on a non-owning client. Called by SimulatedTick(). */
    virtual void SimulateMovement(float DeltaTime);

public:

    /** Force a client update by making it appear on the server that the client hasn't updated in a long time. */
    virtual void ForceReplicationUpdate();
    
    /**
     * Generate a random angle in degrees that is approximately equal between client and server.
     * Note that in networked games this result changes with low frequency and has a low period,
     * so should not be used for frequent randomization.
     */
    virtual float GetNetworkSafeRandomAngleDegrees() const;

    //--------------------------------
    // INetworkPredictionInterface implementation

    //--------------------------------
    // Server hook
    //--------------------------------
    virtual void SendClientAdjustment() override;
    virtual void ForcePositionUpdate(float DeltaTime) override;

    //--------------------------------
    // Client hook
    //--------------------------------

    /**
     * React to new transform from network update. Sets bNetworkSmoothingComplete to false to ensure future smoothing updates.
     * IMPORTANT: It is expected that this function triggers any movement/transform updates to match the network update if desired.
     */
    virtual void SmoothCorrection(const FVector& OldLocation, const FQuat& OldRotation, const FVector& NewLocation, const FQuat& NewRotation) override;

    /** Get prediction data for a client game. Should not be used if not running as a client. Allocates the data on demand and can be overridden to allocate a custom override if desired. Result must be a FNetworkPredictionData_Client_VPC. */
    virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;
    /** Get prediction data for a server game. Should not be used if not running as a server. Allocates the data on demand and can be overridden to allocate a custom override if desired. Result must be a FNetworkPredictionData_Server_VPC. */
    virtual FNetworkPredictionData_Server* GetPredictionData_Server() const override;

    FNetworkPredictionData_Client_VPC* GetPredictionData_Client_VPC() const;
    FNetworkPredictionData_Server_VPC* GetPredictionData_Server_VPC() const;

    virtual bool HasPredictionData_Client() const override;
    virtual bool HasPredictionData_Server() const override;

    virtual void ResetPredictionData_Client() override;
    virtual void ResetPredictionData_Server() override;

protected:
    FNetworkPredictionData_Client_VPC* ClientPredictionData;
    FNetworkPredictionData_Server_VPC* ServerPredictionData;

    /**
     * Smooth mesh location for network interpolation, based on values set up by SmoothCorrection.
     * Internally this simply calls SmoothClientPosition_Interpolate() then SmoothClientPosition_UpdateVisuals().
     * This function is not called when bNetworkSmoothingComplete is true.
     * @param DeltaTime Time since last update.
     */
    virtual void SmoothClientPosition(float DeltaTime);

    /**
     * Update interpolation values for client smoothing. Does not change actual mesh location.
     * Sets bNetworkSmoothingComplete to true when the interpolation reaches the target.
     */
    void SmoothClientPosition_Interpolate(float DeltaTime);

    /** Update mesh location based on interpolated values. */
    void SmoothClientPosition_UpdateVisuals();

    /*
    ========================================================================
    Here's how player movement prediction, replication and correction works in network games:
    
    Every tick, the TickComponent() function is called.  It figures out the acceleration and rotation change for the frame,
    and then calls PerformMovement() (for locally controlled Characters), or ReplicateMoveToServer() (if it's a network client).
    
    ReplicateMoveToServer() saves the move (in the PendingMove list), calls PerformMovement(), and then replicates the move
    to the server by calling the replicated function ServerMove() - passing the movement parameters, the client's
    resultant position, and a timestamp.
    
    ServerMove() is executed on the server.  It decodes the movement parameters and causes the appropriate movement
    to occur.  It then looks at the resulting position and if enough time has passed since the last response, or the
    position error is significant enough, the server calls ClientAdjustPosition(), a replicated function.
    
    ClientAdjustPosition() is executed on the client.  The client sets its position to the servers version of position,
    and sets the bUpdatePosition flag to true.
    
    When TickComponent() is called on the client again, if bUpdatePosition is true, the client will call
    ClientUpdatePosition() before calling PerformMovement().  ClientUpdatePosition() replays all the moves in the pending
    move list which occurred after the timestamp of the move the server was adjusting.
    */

    /** Ticks the characters pose and accumulates root motion */
    void TickCharacterPose(float DeltaTime);

    /**
     * On the server if we know we are having our replication rate throttled,
     * this method checks if important replicated properties have changed that should cause us to return to the normal replication rate.
     */
    virtual bool ShouldCancelAdaptiveReplication() const;

    /**
     * Whether adaptive replication cancelling is enabled.
     */
    virtual bool IsCancelAdaptiveReplicationEnabled() const;

public:

    // RVO Avoidance

    UFUNCTION(BlueprintCallable, Category=VesselMovementComponent)
    virtual void SetRVOAgentComponent(URVO3DAgentComponent* InRVOAgentComponent);

    /** calculate RVO avoidance and apply it to current velocity */
    virtual void CalcAvoidanceVelocity();

    /** allows modifing avoidance velocity, called when bUseRVOPostProcess is set */
    virtual void PostProcessAvoidanceVelocity(FVector& NewVelocity);

protected:

    // RVO

    /** if set, PostProcessAvoidanceVelocity will be called */
    uint32 bUseRVOPostProcess:1;

    /**
     * The component that control movement input.
     * @see bAutoRegisterRVOAgentComponent, SetRVOAgentComponent()
     */
    UPROPERTY(BlueprintReadOnly, Transient, DuplicateTransient)
    URVO3DAgentComponent* RVOAgentComponent;

public:

    /** Minimum delta time considered when ticking. Delta times below this are not considered. This is a very small non-zero value to avoid potential divide-by-zero in simulation code. */
    static const float MIN_TICK_TIME;

    /** Stop completely when braking and velocity magnitude is lower than this. */
    static const float BRAKE_TO_STOP_VELOCITY;
};

FORCEINLINE AVPawnChar* UVPCMovementComponent::GetCharacterOwner() const
{
    return CharacterOwner;
}

FORCEINLINE float UVPCMovementComponent::GetAxisDeltaRotation(float InAxisRotationRate, float DeltaTime)
{
    return (InAxisRotationRate >= 0.f) ? (InAxisRotationRate * DeltaTime) : 360.f;
}
