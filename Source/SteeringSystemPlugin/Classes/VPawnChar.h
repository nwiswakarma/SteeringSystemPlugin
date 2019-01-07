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
#include "Animation/AnimationAsset.h"
#include "Components/ActorComponent.h"
#include "Engine/NetSerialization.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "Templates/SubclassOf.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/CoreNet.h"
#include "VPawnChar.generated.h"

// Forward declarations
class AController;
class FDebugDisplayInfo;
class UAnimMontage;
class UArrowComponent;
class UVPMovementComponent;
class UVPCMovementComponent;
class UPrimitiveComponent;
class USkeletalMeshComponent;
struct FAnimMontageInstance;

// Event delegates
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FVPawnCharMovementModeChangedSignature, class AVPawnChar*, Character, EMovementMode, PrevMovementMode, uint8, PreviousCustomMode);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FVPawnCharMovementUpdatedSignature, float, DeltaSeconds, FVector, OldLocation, FVector, OldVelocity);

/**
 * Characters are Pawns that have a collision, built-in movement logic, and optional mesh.
 * They are responsible for all physical interaction between the player or AI and the world, and also implement basic networking and input models.
 *
 * @see AVPawn, UVPCMovementComponent
 */ 

UCLASS(config=Game, BlueprintType, meta=(ShortTooltip="Simplified character with only flying mode."))
class STEERINGSYSTEMPLUGIN_API AVPawnChar : public AVPawn
{
	GENERATED_BODY()
public:
	/**
	 * Default UObject constructor.
	 */
	AVPawnChar(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	/** The mesh attach root. */
	UPROPERTY(Category = Character, VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	USceneComponent* MeshRoot;

	/** The main skeletal mesh associated with this Character (optional sub-object). */
	UPROPERTY(Category = Character, VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* Mesh;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	UArrowComponent* ArrowComponent;
#endif

	/**
     * Movement component used for movement logic in various movement modes (walking, falling, etc),
     * containing relevant settings and functions to control movement.
     */
	UPROPERTY(Category = Character, VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	UVPCMovementComponent* CharacterMovement;

	/**
     * The ShapeComponent being used for movement collision (by CharacterMovement).
     * Always treated as being vertically aligned in simple collision check functions.
     */
	UPROPERTY(Category = Character, VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	UShapeComponent* ShapeComponent;

public:

	/** Name of the MeshRootComponent */
	static FName MeshRootComponentName;

	/** Name of the MeshComponent. Use this name if you want to prevent creation of the component (with ObjectInitializer.DoNotCreateDefaultSubobject). */
	static FName MeshComponentName;

	/** Name of the CharacterMovement component. Use this name if you want to use a different class (with ObjectInitializer.SetDefaultSubobjectClass). */
	static FName CharacterMovementComponentName;

	/** Name of the ShapeComponent. */
	static FName ShapeComponentName;

	/** Sets the component the Character is walking on, used by CharacterMovement walking movement to be able to follow dynamic objects. */
	UFUNCTION(BlueprintCallable, Category="Pawn|Character")
	virtual void SetBase(UPrimitiveComponent* NewBase, const FName BoneName = NAME_None, bool bNotifyActor=true);
	
	/**
	 * Cache mesh offset from capsule. This is used as the target for network smoothing interpolation, when the mesh is offset with lagged smoothing.
	 * This is automatically called during initialization; call this at runtime if you intend to change the default mesh offset from the capsule.
	 * @see GetBaseTranslationOffset(), GetBaseRotationOffset()
	 */
	UFUNCTION(BlueprintCallable, Category="Pawn|Character")
	virtual void CacheInitialMeshOffset(FVector MeshRelativeLocation, FRotator MeshRelativeRotation);

protected:

	/** Info about our current movement base (object we are standing on). */
	UPROPERTY()
	FBasedMovementInfo BasedMovement;

	/** Replicated version of relative movement. Read-only on simulated proxies! */
	UPROPERTY(ReplicatedUsing=OnRep_ReplicatedBasedMovement)
	FBasedMovementInfo ReplicatedBasedMovement;

public:
	/** Rep notify for ReplicatedBasedMovement */
	UFUNCTION()
	virtual void OnRep_ReplicatedBasedMovement();

	/** Set whether this actor's movement replicates to network clients. */
	UFUNCTION(BlueprintCallable, Category = "Replication")
	virtual void SetReplicateMovement(bool bInReplicateMovement) override;

protected:
	/** Saved translation offset of mesh. */
	UPROPERTY()
	FVector BaseTranslationOffset;

	/** Saved rotation offset of mesh. */
	UPROPERTY()
	FQuat BaseRotationOffset;

	/** Event called after actor's base changes (if SetBase was requested to notify us with bNotifyPawn). */
	virtual void BaseChange();

	/** CharacterMovement ServerLastTransformUpdateTimeStamp value, replicated to simulated proxies. */
	UPROPERTY(Replicated)
	float ReplicatedServerLastTransformUpdateTimeStamp;

	/**
     * CharacterMovement MovementMode (and custom mode) replicated for simulated proxies.
     * Use CharacterMovementComponent::UnpackNetworkMovementMode() to translate it.
     */
	UPROPERTY(Replicated)
	uint8 ReplicatedMovementMode;

	/** Flag that we are receiving replication of the based movement. */
	UPROPERTY()
	bool bInBaseReplication;

public:	

	/** Accessor for ReplicatedServerLastTransformUpdateTimeStamp. */
	FORCEINLINE float GetReplicatedServerLastTransformUpdateTimeStamp() const { return ReplicatedServerLastTransformUpdateTimeStamp; }

	/** Accessor for BasedMovement */
	FORCEINLINE const FBasedMovementInfo& GetBasedMovement() const { return BasedMovement; }
	
	/** Accessor for ReplicatedBasedMovement */
	FORCEINLINE const FBasedMovementInfo& GetReplicatedBasedMovement() const { return ReplicatedBasedMovement; }

	/** Save a new relative location in BasedMovement and a new rotation with is either relative or absolute. */
	void SaveRelativeBasedMovement(const FVector& NewRelativeLocation, const FRotator& NewRotation, bool bRelativeRotation);

	/** Returns ReplicatedMovementMode */
	uint8 GetReplicatedMovementMode() const { return ReplicatedMovementMode; }

	/** Get the saved translation offset of mesh. This is how much extra offset is applied from the center of the capsule. */
	UFUNCTION(BlueprintCallable, Category="Pawn|Character")
	FVector GetBaseTranslationOffset() const { return BaseTranslationOffset; }

	/** Get the saved rotation offset of mesh. This is how much extra rotation is applied from the capsule rotation. */
	virtual FQuat GetBaseRotationOffset() const { return BaseRotationOffset; }

	/** Get the saved rotation offset of mesh. This is how much extra rotation is applied from the capsule rotation. */
	UFUNCTION(BlueprintCallable, Category="Pawn|Character", meta=(DisplayName="GetBaseRotationOffset"))
	FRotator GetBaseRotationOffsetRotator() const { return GetBaseRotationOffset().Rotator(); }

	/** When true, applying updates to network client (replaying saved moves for a locally controlled character) */
	UPROPERTY(Transient)
	uint32 bClientUpdating:1;

	//~ Begin AActor Interface.
	virtual void ClearCrossLevelReferences() override;
	virtual void PreNetReceive() override;
	virtual void PostNetReceive() override;
	virtual void PostNetReceiveLocationAndRotation() override;
	virtual UActorComponent* FindComponentByClass(const TSubclassOf<UActorComponent> ComponentClass) const override;
	virtual void TornOff() override;
	//~ End AActor Interface

	template<class T>
	T* FindComponentByClass() const
	{
		return AActor::FindComponentByClass<T>();
	}

	//~ Begin AVPawn Interface.
	virtual void PostInitializeComponents() override;
	virtual UVPMovementComponent* GetMovementComponent() const override;
	virtual UPrimitiveComponent* GetMovementBase() const override final { return BasedMovement.MovementBase; }
	virtual void DisplayDebug(UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos) override;
	//~ End AVPawn Interface

	/** Apply momentum caused by damage. */
	virtual void ApplyDamageMomentum(float DamageTaken, FDamageEvent const& DamageEvent, APawn* PawnInstigator, AActor* DamageCauser);

public:

	/** Play Animation Montage on the character mesh **/
	UFUNCTION(BlueprintCallable, Category=Animation)
	virtual float PlayAnimMontage(UAnimMontage* AnimMontage, float InPlayRate = 1.f, FName StartSectionName = NAME_None);

	/** Stop Animation Montage. If NULL, it will stop what's currently active. The Blend Out Time is taken from the montage asset that is being stopped. **/
	UFUNCTION(BlueprintCallable, Category=Animation)
	virtual void StopAnimMontage(UAnimMontage* AnimMontage = NULL);

	/** Return current playing Montage **/
	UFUNCTION(BlueprintCallable, Category=Animation)
	UAnimMontage * GetCurrentMontage();

public:

	/** Set a pending launch velocity on the Character. This velocity will be processed on the next CharacterMovementComponent tick,
	  * and will set it to the "falling" state. Triggers the OnLaunched event.
	  * @PARAM LaunchVelocity is the velocity to impart to the Character
	  * @PARAM bXYOverride if true replace the XY part of the Character's velocity instead of adding to it.
	  * @PARAM bZOverride if true replace the Z component of the Character's velocity instead of adding to it.
	  */
	//UFUNCTION(BlueprintCallable, Category="Pawn|Character")
	//virtual void LaunchCharacter(FVector LaunchVelocity, bool bXYOverride, bool bZOverride);

	/** Let blueprint know that we were launched */
	UFUNCTION(BlueprintImplementableEvent)
	void OnLaunched(FVector LaunchVelocity, bool bXYOverride, bool bZOverride);

	/** Called when pawn's movement is blocked
		@PARAM Impact describes the blocking hit. */
	virtual void MoveBlockedBy(const FHitResult& Impact) {};

	/**
	 * Called from CharacterMovementComponent to notify the character that the movement mode has changed.
	 * @param	PrevMovementMode	Movement mode before the change
	 * @param	PrevCustomMode		Custom mode before the change (applicable if PrevMovementMode is Custom)
	 */
	virtual void OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode = 0);

	/** Multicast delegate for MovementMode changing. */
	UPROPERTY(BlueprintAssignable, Category="Pawn|Character")
	FVPawnCharMovementModeChangedSignature MovementModeChangedDelegate;

	/**
	 * Called from CharacterMovementComponent to notify the character that the movement mode has changed.
	 * @param	PrevMovementMode	Movement mode before the change
	 * @param	NewMovementMode		New movement mode
	 * @param	PrevCustomMode		Custom mode before the change (applicable if PrevMovementMode is Custom)
	 * @param	NewCustomMode		New custom mode (applicable if NewMovementMode is Custom)
	 */
	UFUNCTION(BlueprintImplementableEvent, meta=(DisplayName = "OnMovementModeChanged"))
	void K2_OnMovementModeChanged(EMovementMode PrevMovementMode, EMovementMode NewMovementMode, uint8 PrevCustomMode, uint8 NewCustomMode);

	/**
	 * Event for implementing custom character movement mode. Called by CharacterMovement if MovementMode is set to Custom.
	 * @note C++ code should override UVPCMovementComponent::PhysCustom() instead.
	 * @see UVPCMovementComponent::PhysCustom()
	 */
	UFUNCTION(BlueprintImplementableEvent, meta=(DisplayName = "UpdateCustomMovement"))
	void K2_UpdateCustomMovement(float DeltaTime);

	/**
	 * Event triggered at the end of a CharacterMovementComponent movement update.
	 * This is the preferred event to use rather than the Tick event when performing custom updates to CharacterMovement properties based on the current state.
	 * This is mainly due to the nature of network updates, where client corrections in position from the server can cause multiple iterations of a movement update,
	 * which allows this event to update as well, while a Tick event would not.
	 *
	 * @param	DeltaSeconds		Delta time in seconds for this update
	 * @param	InitialLocation		Location at the start of the update. May be different than the current location if movement occurred.
	 * @param	InitialVelocity		Velocity at the start of the update. May be different than the current velocity.
	 */
	UPROPERTY(BlueprintAssignable, Category="Pawn|Character")
	FVPawnCharMovementUpdatedSignature OnCharacterMovementUpdated;

public:

	/**
	 * Called on client after position update is received to respond to the new location and rotation.
	 * Actual change in location is expected to occur in CharacterMovement->SmoothCorrection(), after which this occurs.
	 * Default behavior is to check for penetration in a blocking object if bClientCheckEncroachmentOnNetUpdate is enabled, and set bSimGravityDisabled=true if so.
	 */
	virtual void OnUpdateSimulatedPosition(const FVector& OldLocation, const FQuat& OldRotation);

	/** Called on the actor right before replication occurs.
	* Only called on Server, and for autonomous proxies if recording a Client Replay. */
	virtual void PreReplication(IRepChangedPropertyTracker & ChangedPropertyTracker) override;

	/** Called on the actor right before replication occurs.
	* Called for everyone when recording a Client Replay, including Simulated Proxies. */
	virtual void PreReplicationForReplay(IRepChangedPropertyTracker & ChangedPropertyTracker) override;

public:
	/** Returns Mesh subobject **/
	USkeletalMeshComponent* GetMesh() const;
	/** Returns Mesh Root subobject **/
	USceneComponent* GetMeshRoot() const;
#if WITH_EDITORONLY_DATA
	/** Returns ArrowComponent subobject **/
	UArrowComponent* GetArrowComponent() const;
#endif
	/** Returns CharacterMovement subobject **/
	UVPCMovementComponent* GetCharacterMovement() const;
    /** Returns ShapeComponent subobject **/
	UShapeComponent* GetShapeComponent() const;
};

//////////////////////////////////////////////////////////////////////////
// Character inlines

/** Returns MeshRoot subobject **/
FORCEINLINE USceneComponent* AVPawnChar::GetMeshRoot() const { return MeshRoot; }
/** Returns Mesh subobject **/
FORCEINLINE USkeletalMeshComponent* AVPawnChar::GetMesh() const { return Mesh; }
#if WITH_EDITORONLY_DATA
/** Returns ArrowComponent subobject **/
FORCEINLINE UArrowComponent* AVPawnChar::GetArrowComponent() const { return ArrowComponent; }
#endif
/** Returns CharacterMovement subobject **/
FORCEINLINE UVPCMovementComponent* AVPawnChar::GetCharacterMovement() const { return CharacterMovement; }
/** Returns ShapeComponent subobject **/
FORCEINLINE UShapeComponent* AVPawnChar::GetShapeComponent() const { return ShapeComponent; }

UCLASS(config=Game, BlueprintType, meta=(ShortTooltip="Simplified character with only flying mode (without mesh)."))
class STEERINGSYSTEMPLUGIN_API AVPawnCharNoMesh : public AVPawnChar
{
	GENERATED_BODY()

private:

	/**
     * The ShapeComponent being used for movement collision (by CharacterMovement).
     * Always treated as being vertically aligned in simple collision check functions.
     */
	UPROPERTY(Category = Character, VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	class UCapsuleComponent* CapsuleComponent;

public:
	/**
	 * Default UObject constructor.
	 */
	AVPawnCharNoMesh(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

UCLASS(config=Game, BlueprintType, meta=(ShortTooltip="Simplified character with only flying mode (box collision without mesh)."))
class STEERINGSYSTEMPLUGIN_API AVPawnCharNoMeshBox : public AVPawnChar
{
	GENERATED_BODY()

private:

	/**
     * The ShapeComponent being used for movement collision (by CharacterMovement).
     * Always treated as being vertically aligned in simple collision check functions.
     */
	UPROPERTY(Category = Character, VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	class UBoxComponent* BoxComponent;

public:
	/**
	 * Default UObject constructor.
	 */
	AVPawnCharNoMeshBox(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
