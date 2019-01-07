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
#include "GameFramework/Actor.h"
#include "ISteerable.h"
#include "SteeringBehavior.h"
#include "SteeringFormation.h"
#include "VPSteerableComponent.generated.h"

class AVPawn;
class UVPMovementComponent;

/** 
 * Steering behavior done event, triggered when a steering behavior has finished its steering calculation.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVPSteerableBehaviorDoneEvent, FName, SteeringType);

UCLASS(ClassGroup=Movement, meta=(BlueprintSpawnableComponent))
class STEERINGSYSTEMPLUGIN_API UVPSteerableComponent : public UActorComponent, public ISteerable
{
	GENERATED_UCLASS_BODY()

    FBehaviorList Behaviors;

    // Assigned steering formation
    FSteeringFormation* Formation;

public:

	/** If true, search for the owner's movement component as the MovementComponent if there is not one currently assigned. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Component)
	bool bAutoRegisterMovementComponent;

	/**
	 * If true, whenever the updated component is changed, this component will enable or disable its tick dependent on whether it has something to update.
	 * This will NOT enable tick at startup if bAutoActivate is false, because presumably you have a good reason for not wanting it to start ticking initially.
	 **/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Component)
	bool bAutoUpdateTickRegistration;

	/** Steerable priority, higher value means higher priority. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=SteeringBehavior)
	int32 Priority;

	/** Steerable inner bounding radius. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=SteeringBehavior)
	float InnerRadius;

	/** Steerable outer bounding radius. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=SteeringBehavior)
	float OuterRadius;

	/** Maximum velocity magnitude allowed for the controlled Pawn. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=SteeringBehavior, AdvancedDisplay, meta=(ClampMin="0.01", ClampMax="1.0", UIMin="0.01", UIMax="1.0"))
	float MinControlInput;

	/** Multicast delegate for behavior done event. */
	UPROPERTY(BlueprintAssignable, Category=SteeringBehavior)
	FVPSteerableBehaviorDoneEvent BehaviorDoneEvent;

	UFUNCTION(BlueprintCallable, Category=VesselMovementComponent)
	virtual void SetMovementComponent(UVPMovementComponent* InMovementComponent);

	/** Update tick registration state, determined by bAutoUpdateTickRegistration. Called by SetUpdatedComponent. */
	virtual void UpdateTickRegistration();

//BEGIN ActorComponent Interface 
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;
	virtual void RegisterComponentTickFunctions(bool bRegister) override;
	virtual void OnRegister() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
//END ActorComponent Interface 
    
//BEGIN Steerable Interface

// ~ Blueprint Functions

    /**
     * Add a new steering behavior.
     */
	UFUNCTION(BlueprintCallable, Category=SteeringBehavior, meta=(DisplayName="SetFormation"))
	void K2_SetFormation(FSteeringFormationRef& FormationRef, bool bSetAsPrimary = false);

    /**
     * Add a new steering behavior.
     */
	UFUNCTION(BlueprintCallable, Category=SteeringBehavior, meta=(DisplayName="AddSteeringBehavior"))
	void K2_AddSteeringBehavior(FSteeringBehaviorRef& BehaviorRef, bool bPreserveData = false);

    /**
     * Enqueue a new steering behavior.
     */
	UFUNCTION(BlueprintCallable, Category=SteeringBehavior, meta=(DisplayName="EnqueueSteeringBehavior"))
	void K2_EnqueueSteeringBehavior(FSteeringBehaviorRef& BehaviorRef, bool bPreserveData = false);

    /**
     * Remove a steering behavior.
     */
	UFUNCTION(BlueprintCallable, Category=SteeringBehavior, meta=(DisplayName="RemoveSteeringBehavior"))
	void K2_RemoveSteeringBehavior(FSteeringBehaviorRef& BehaviorRef, bool bPreserveData = false);

    /**
     * Clears current steering behaviors.
     */
	UFUNCTION(BlueprintCallable, Category=SteeringBehavior, meta=(DisplayName="ClearSteeringBehaviors"))
	void K2_ClearSteeringBehaviors();

    /**
     * Returns steerable reference struct to the component.
     */
	UFUNCTION(BlueprintCallable, Category=SteeringBehavior, meta=(DisplayName="GetSteerable"))
	FSteerableRef K2_GetSteerable();

	/** Returns the vector indicating the linear velocity of this Steerable. */
	UFUNCTION(BlueprintCallable, Category=SteeringBehavior, meta=(DisplayName="GetSteerableLocation"))
	FVector K2_GetSteerableLocation() const;

	/** Returns the vector indicating the linear velocity of this Steerable. */
	UFUNCTION(BlueprintCallable, Category=SteeringBehavior, meta=(DisplayName="GetForwardVector"))
	FVector K2_GetForwardVector() const;

	/** Returns the vector indicating the linear velocity of this Steerable. */
	UFUNCTION(BlueprintCallable, Category=SteeringBehavior, meta=(DisplayName="GetLinearVelocity"))
	FVector K2_GetLinearVelocity() const;

	/** Returns the inner radius of this Steerable. */
	UFUNCTION(BlueprintCallable, Category=SteeringBehavior, meta=(DisplayName="GetInnerRadius"))
	float K2_GetInnerRadius() const;

	/** Returns the outer radius of this Steerable. */
	UFUNCTION(BlueprintCallable, Category=SteeringBehavior, meta=(DisplayName="GetOuterRadius"))
	float K2_GetOuterRadius() const;

	/** Returns the priority of this Steerable, higher value means higher prioerity. */
	UFUNCTION(BlueprintCallable, Category=SteeringBehavior, meta=(DisplayName="GetPriority"))
	int32 K2_GetPriority() const;

	/** Returns the maximum linear speed. */
	UFUNCTION(BlueprintCallable, Category=SteeringBehavior, meta=(DisplayName="GetMaxLinearSpeed"))
	float K2_GetMaxLinearSpeed() const;

	/** Returns the maximum linear acceleration. */
	UFUNCTION(BlueprintCallable, Category=SteeringBehavior, meta=(DisplayName="GetMaxLinearAcceleration"))
	float K2_GetMaxLinearAcceleration() const;

	/** Returns the maximum angular acceleration. */
	UFUNCTION(BlueprintCallable, Category=SteeringBehavior, meta=(DisplayName="GetMinControlInput"))
	float K2_GetMinControlInput() const;

// ~ Direct Functions

	FORCEINLINE virtual void SetFormation(FSteeringFormation* InFormation, bool bSetAsPrimary=false);
	FORCEINLINE virtual void SetFormationDirect(FSteeringFormation* InFormation);
	FORCEINLINE virtual FSteeringFormation* GetFormation();
	FORCEINLINE virtual bool HasFormation() const;

	FORCEINLINE virtual void AddSteeringBehavior(FPSSteeringBehavior InBehavior);
	FORCEINLINE virtual void EnqueueSteeringBehavior(FPSSteeringBehavior InBehavior);
	FORCEINLINE virtual void RemoveSteeringBehavior(FPSSteeringBehavior InBehavior);
	FORCEINLINE virtual void ClearSteeringBehaviors();

	FORCEINLINE virtual FTickFunction* GetSteerableTickFunction();
	FORCEINLINE virtual FString GetSteerableName() const;
	FORCEINLINE virtual FVector GetSteerableLocation() const;
	FORCEINLINE virtual FQuat GetSteerableOrientation() const;
	FORCEINLINE virtual FVector GetForwardVector() const;
	FORCEINLINE virtual FVector GetLinearVelocity() const;
	FORCEINLINE virtual float GetInnerRadius() const;
	FORCEINLINE virtual float GetOuterRadius() const;
	FORCEINLINE virtual int32 GetPriority() const;

	FORCEINLINE virtual float GetMaxLinearSpeed() const;
	FORCEINLINE virtual float GetMaxLinearAcceleration() const;
	FORCEINLINE virtual float GetMinControlInput() const;

//END Steerable Interface

protected:

	/**
	 * The component that control movement input.
	 * @see bAutoRegisterMovementComponent, SetMovementComponent()
	 */
	UPROPERTY(BlueprintReadOnly, Transient, DuplicateTransient, Category=Component)
	UVPMovementComponent* MovementComponent;

	/** Pawn that owns the movement component. */
	UPROPERTY(Transient, DuplicateTransient)
	AVPawn* PawnOwner;

	FORCEINLINE void ResetRegisteredBehavior(FPSSteeringBehavior& Behavior);

    void UpdateBehavior(float DeltaTime);
    bool UpdateActiveBehavior(float DeltaTime);

    FORCEINLINE USceneComponent* GetUpdatedComponent();
    FORCEINLINE const USceneComponent* GetUpdatedComponent() const;
    FORCEINLINE bool HasValidData() const;

};
