// 

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "Templates/SubclassOf.h"
#include "UObject/CoreNet.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Actor.h"
#include "VPawn.generated.h"

class AController;
class APawn;
class UCanvas;
class UDamageType;
class UVPMovementComponent;
class UPrimitiveComponent;
class FDebugDisplayInfo;

/** 
 * Virtual VPawn, used as pawns that require no controller possession.
 */
UCLASS(config=Game, BlueprintType, Blueprintable, hideCategories=(Navigation))
class STEERINGSYSTEMPLUGIN_API AVPawn : public AActor
{
    GENERATED_BODY()
public:
    /**
    * Default UObject constructor.
    */
    AVPawn(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

    /** Return our VPMovementComponent, if we have one.
     * By default, returns the first VPMovementComponent found.
     * Native classes that create their own movement component should override this method for more efficiency. */
    UFUNCTION(BlueprintCallable, meta=(Tooltip="Return our VPMovementComponent, if we have one."), Category="VPawn")
    virtual UVPMovementComponent* GetMovementComponent() const;

    /** Return PrimitiveComponent we are based on (standing on, attached to, and moving on). */
    virtual UPrimitiveComponent* GetMovementBase() const
    {
        return NULL;
    }

private:
    /* Whether this VPawn's input handling is enabled.  VPawn must still be possessed to get input even if this is true */
    uint32 bInputEnabled:1;

public:

	/** Controller of the last Actor that caused us damage. */
	UPROPERTY(transient)
	AController* LastHitBy;

	/** Gets the owning actor of a pawn Movement Base Component. */
	UFUNCTION(BlueprintPure, Category="VPawn")
	static AActor* GetMovementBaseActor(const AVPawn* VPawn);

    virtual bool IsBasedOnActor(const AActor* Other) const override;

    //~ Begin AActor Interface.
    virtual FVector GetVelocity() const override;
    virtual void PostNetReceiveLocationAndRotation() override;
    virtual void PostNetReceiveVelocity(const FVector& NewVelocity) override;
    virtual float TakeDamage(float Damage, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;
    virtual void TeleportSucceeded(bool bIsATest) override;
    //~ End AActor Interface.

	/** 
	 * Subclasses may check this as well if they override TakeDamage
     * and don't want to potentially trigger TakeDamage actions by checking if it returns zero in the super class.
     *
     * @return true if we are in a state to take damage (checked at the start of TakeDamage.
     */
	virtual bool ShouldTakeDamage(float Damage, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) const;

    bool InputEnabled() const
    {
        return bInputEnabled;
    }

protected:

	/**
     * Get the controller instigating the damage. If the damage is caused by the world and the supplied controller is NULL or is this pawn's controller,
     * uses LastHitBy as the instigator.
     */
	virtual AController* GetDamageInstigator(AController* InstigatedBy, const UDamageType& DamageType) const;

public:

    /**
     * Add movement input along the given world direction vector (usually normalized) scaled by 'ScaleValue'.
     * If ScaleValue < 0, movement will be in the opposite direction.
     *
     * Base VPawn classes won't automatically apply movement, it's up to the user to do so in a Tick event.
     * Subclasses such as Character and DefaultPawn automatically handle this input and move.
     *
     * @param WorldDirection    Direction in world space to apply input
     * @param ScaleValue        Scale to apply to input. This can be used for analog input, ie a value of 0.5 applies half the normal value,
     *                          while -1.0 would reverse the direction.
     * @param bForce            If true always add the input, ignoring the result of IsMoveInputIgnored().
     * @see GetPendingMovementInputVector(), GetLastMovementInputVector(), ConsumeMovementInputVector()
     */
    UFUNCTION(BlueprintCallable, Category="VPawn|Input", meta=(Keywords="AddInput"))
    virtual void AddMovementInput(FVector WorldDirection, float ScaleValue = 1.0f, bool bForce = false);

	/**
     * Marks whether current movement input is enabled. Will be reset on the next ConsumeInputVector()
	 */
	UFUNCTION(BlueprintCallable, Category="VPawn|Input", meta=(Keywords="MarkEnabled"))
	virtual void MarkInputEnabled(bool bEnable);

	/**
     * Marks whether current movement input has its orientation locked. Will be reset on the next ConsumeInputVector()
	 */
	UFUNCTION(BlueprintCallable, Category="VPawn|Input", meta=(Keywords="MarkEnabled"))
	virtual void LockOrientation(bool bEnableLock);

    /**
     * Returns the pending input vector and resets it to zero.
     * This should be used during a movement update (by the VPawn or VPMovementComponent) to prevent accumulation of control input between frames.
     * Copies the pending input vector to the saved input vector (GetLastMovementInputVector()).
     * @return The pending input vector.
     */
    UFUNCTION(BlueprintCallable, Category="VPawn|Input", meta=(Keywords="ConsumeInput"))
    virtual FVector ConsumeMovementInputVector();

    /** Helper to see if move input is ignored. If our controller is a PlayerController, checks Controller->IsMoveInputIgnored(). */
    UFUNCTION(BlueprintCallable, Category="VPawn|Input")
    virtual bool IsMoveInputIgnored() const;

    /** Whether current movement input is enabled. */
    UFUNCTION(BlueprintCallable, Category="VPawn|Input")
    virtual bool IsMoveInputEnabled() const;

    /** Whether current movement orientation is locked. */
    UFUNCTION(BlueprintCallable, Category="VPawn|Input")
    virtual bool IsMoveOrientationLocked() const;

    /**
     * Return the pending input vector in world space. This is the most up-to-date value of the input vector, pending ConsumeMovementInputVector() which clears it,
     * Usually only a VPMovementComponent will want to read this value, or the VPawn itself if it is responsible for movement.
     *
     * @return The pending input vector in world space.
     * @see AddMovementInput(), GetLastMovementInputVector(), ConsumeMovementInputVector()
     */
    UFUNCTION(BlueprintCallable, Category="VPawn|Input", meta=(Keywords="GetMovementInput GetInput"))
    FVector GetPendingMovementInputVector() const;

    /**
     * Return the last input vector in world space that was processed by ConsumeMovementInputVector(), which is usually done by the VPawn or VPMovementComponent.
     * Any user that needs to know about the input that last affected movement should use this function.
     * For example an animation update would want to use this, since by default the order of updates in a frame is:
     * PlayerController (device input) -> MovementComponent -> VPawn -> Mesh (animations)
     *
     * @return The last input vector in world space that was processed by ConsumeMovementInputVector().
     * @see AddMovementInput(), GetPendingMovementInputVector(), ConsumeMovementInputVector()
     */
    UFUNCTION(BlueprintCallable, Category="VPawn|Input", meta=(Keywords="GetMovementInput GetInput"))
    FVector GetLastMovementInputVector() const;

protected:

    /**
     * Whether current movement input is ignored.
     */
    UPROPERTY(Transient)
    bool bIsMoveInputIgnored;

    /**
     * Whether current movement input is enabled.
     */
    UPROPERTY(Transient)
    bool bIsMoveInputEnabled;

    /**
     * Whether current movement orientation is locked.
     */
    UPROPERTY(Transient)
    bool bIsMoveOrientationLocked;

    /**
     * Accumulated control input vector, stored in world space. This is the pending input, which is cleared (zeroed) once consumed.
     * @see GetPendingMovementInputVector(), AddMovementInput()
     */
    UPROPERTY(Transient)
    FVector ControlInputVector;

    /**
     * The last control input vector that was processed by ConsumeMovementInputVector().
     * @see GetLastMovementInputVector()
     */
    UPROPERTY(Transient)
    FVector LastControlInputVector;

public:

    /**
     * Internal function meant for use only within VPawn or by a VPMovementComponent.
     * Adds movement input if not ignored, or if forced.
     */
    void Internal_AddMovementInput(FVector WorldAccel, bool bForce = false);

    /**
     * Internal function meant for use only within VPawn or by a VPMovementComponent.
     * Marks whether current movement input is enabled.
     */
    void Internal_MarkInputEnabled(bool bEnable);

    /**
     * Internal function meant for use only within VPawn or by a VPMovementComponent.
     * Marks whether current movement input has its orientation locked.
     */
    void Internal_LockOrientation(bool bEnableLock);

    /**
     * Internal function meant for use only within VPawn or by a VPMovementComponent.
     * LastControlInputVector is updated with initial value of ControlInputVector. Returns ControlInputVector and resets it to zero.
     */
    FVector Internal_ConsumeMovementInputVector();

    /** Internal function meant for use only within VPawn or by a VPMovementComponent. Returns the value of ControlInputVector. */
    inline FVector Internal_GetPendingMovementInputVector() const
    {
        return ControlInputVector;
    }

    /** Internal function meant for use only within VPawn or by a VPMovementComponent. Returns the value of LastControlInputVector. */
    inline FVector Internal_GetLastMovementInputVector() const
    {
        return LastControlInputVector;
    }

public:

    /** Add an Actor to ignore by VPawn's movement collision */
    void MoveIgnoreActorAdd(AActor* ActorToIgnore);

    /** Remove an Actor to ignore by VPawn's movement collision */
    void MoveIgnoreActorRemove(AActor* ActorToIgnore);
};
