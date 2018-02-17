// 

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ControlInputComponent.generated.h"

/** 
 * 
 */
UCLASS(BlueprintType, Blueprintable, ClassGroup=Steering, meta=(BlueprintSpawnableComponent))
class STEERINGSYSTEMPLUGIN_API UControlInputComponent : public UActorComponent
{
	GENERATED_UCLASS_BODY()

public:

	/**
	 * Adds the given vector to the accumulated input in world space. Input vectors are usually between 0 and 1 in magnitude. 
	 * They are accumulated during a frame then applied as acceleration during the movement update.
	 *
	 * @param WorldDirection	Direction in world space to apply input
	 * @param ScaleValue		Scale to apply to input. This can be used for analog input, ie a value of 0.5 applies half the normal value.
	 * @param bForce			If true always add the input, ignoring the result of IsMoveInputIgnored().
	 * @see APawn::AddMovementInput()
	 */
	UFUNCTION(BlueprintCallable, Category=ControlInputComponent)
	virtual void AddInputVector(FVector WorldVector, bool bForce = false);

	/**
     * Set whether control input may affect acceleration movement
	 * @param bInEnableAcceleration     The new acceleration flag
     */
	UFUNCTION(BlueprintCallable, Category=ControlInputComponent)
	virtual void SetEnableAcceleration(bool bInEnableAcceleration);

	/**
	 * Return the pending input vector in world space. This is the most up-to-date value of the input vector, pending ConsumeMovementInputVector() which clears it.
	 * PawnMovementComponents implementing movement usually want to use either this or ConsumeInputVector() as these functions represent the most recent state of input.
	 * @return The pending input vector in world space.
	 * @see AddInputVector(), ConsumeInputVector(), GetLastInputVector()
	 */
	UFUNCTION(BlueprintCallable, Category=ControlInputComponent, meta=(Keywords="GetInput"))
	FVector GetPendingInputVector() const;

	/**
	* Return the last input vector in world space that was processed by ConsumeInputVector(), which is usually done by the Pawn or PawnMovementComponent.
	* Any user that needs to know about the input that last affected movement should use this function.
	* @return The last input vector in world space that was processed by ConsumeInputVector().
	* @see AddInputVector(), ConsumeInputVector(), GetPendingInputVector()
	*/
	UFUNCTION(BlueprintCallable, Category=ControlInputComponent, meta=(Keywords="GetInput"))
	FVector GetLastInputVector() const;

	/* Returns the pending input vector and resets it to zero.
	 * This should be used during a movement update (by the Pawn or PawnMovementComponent) to prevent accumulation of control input between frames.
	 * Copies the pending input vector to the saved input vector (GetLastMovementInputVector()).
	 * @return The pending input vector.
	 */
	UFUNCTION(BlueprintCallable, Category=ControlInputComponent)
	virtual FVector ConsumeInputVector();

	/** Helper to see if move input is ignored. If there is no Pawn or UpdatedComponent, returns true, otherwise defers to the Pawn's implementation of IsMoveInputIgnored(). */
	UFUNCTION(BlueprintCallable, Category=ControlInputComponent)
	virtual bool IsMoveInputIgnored() const;

	/** Check whether control input may affect acceleration movement */
	UFUNCTION(BlueprintCallable, Category=ControlInputComponent)
	virtual bool IsAccelerationEnabled() const;

	FORCEINLINE void AddInputVector_Direct(FVector WorldVector, bool bForce = false);
	FORCEINLINE void SetEnableAcceleration_Direct(bool bInEnableAcceleration);
	FORCEINLINE FVector GetPendingInputVector_Direct() const;
	FORCEINLINE FVector GetLastInputVector_Direct() const;
	FORCEINLINE FVector ConsumeInputVector_Direct();
	FORCEINLINE bool IsMoveInputIgnored_Direct() const;
	FORCEINLINE bool IsAccelerationEnabled_Direct() const;

protected:

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

	/**
	 * Whether move input is ignored.
	 * @see IsMoveInputIgnored()
	 */
	UPROPERTY()
	bool bMoveInputIgnored;

	/**
	 * Whether control input may affect acceleration movement
	 * @see IsAccelerationEnabled(), SetEnableAcceleration()
	 */
	UPROPERTY()
	bool bEnableAcceleration;

};
