// 

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "IFormationProxy.h"
#include "SteeringFormation.h"
#include "SteeringFormationComponent.generated.h"

UCLASS(ClassGroup=Movement, BlueprintType, meta=(BlueprintSpawnableComponent))
class STEERINGSYSTEMPLUGIN_API USteeringFormationComponent : public UActorComponent, public IFormationProxy
{
	GENERATED_UCLASS_BODY()

    FSteeringFormation Formation;

	/**
	 * The component we move and update.
	 * If this is null at startup and bAutoRegisterUpdatedComponent is true,
     * the owning Actor's root component will automatically be set as our UpdatedComponent at startup.
     *
	 * @see bAutoRegisterUpdatedComponent, SetUpdatedComponent(), UpdatedPrimitive
	 */
	UPROPERTY(BlueprintReadOnly, Transient, DuplicateTransient, Category=SteeringFormationComponent)
	USceneComponent* UpdatedComponent;

	/** If true, registers the owner's Root component as the UpdatedComponent if there is not one currently assigned. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=SteeringFormationComponent)
	uint32 bAutoRegisterUpdatedComponent:1;

	/**
	 * If true, whenever the updated component is changed, this component will enable or disable its tick dependent on whether it has something to update.
	 * This will NOT enable tick at startup if bAutoActivate is false, because presumably you have a good reason for not wanting it to start ticking initially.
	 **/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=SteeringFormation)
	uint32 bAutoUpdateTickRegistration:1;

public:

	/** Assign the component we move and update. */
	UFUNCTION(BlueprintCallable, Category="Components|SteeringFormation")
	virtual void SetUpdatedComponent(USceneComponent* NewUpdatedComponent);

	/** Update tick registration state, determined by bAutoUpdateTickRegistration. Called by SetUpdatedComponent. */
	virtual void UpdateTickRegistration();

//BEGIN ActorComponent Interface 
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;
	virtual void InitializeComponent() override;
	virtual void OnRegister() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
//END ActorComponent Interface 

// ~ Blueprint Functions

    /**
     * Add a new formation behavior data.
     */
	UFUNCTION(BlueprintCallable, Category=SteeringFormation, meta=(DisplayName="AddFormationBehavior"))
	void K2_AddFormationBehavior(FFormationBehaviorRef& BehaviorRef, bool bPreserveData = false);

    /**
     * Enqueue a new formation behavior data.
     */
	UFUNCTION(BlueprintCallable, Category=SteeringFormation, meta=(DisplayName="EnqueueFormationBehavior"))
	void K2_EnqueueFormationBehavior(FFormationBehaviorRef& BehaviorRef, bool bPreserveData = false);

    /**
     * Remove formation behavior data.
     */
	UFUNCTION(BlueprintCallable, Category=SteeringFormation, meta=(DisplayName="RemoveFormationBehavior"))
	void K2_RemoveFormationBehavior(FFormationBehaviorRef& BehaviorRef, bool bPreserveData = false);

    /**
     * Clears current steering behaviors.
     */
	UFUNCTION(BlueprintCallable, Category=SteeringFormation, meta=(DisplayName="ClearFormationBehaviors"))
	void K2_ClearFormationBehaviors();

    /**
     * Returns steerable reference struct to the component.
     */
	UFUNCTION(BlueprintCallable, Category=SteeringFormation, meta=(DisplayName="GetFormation"))
	FSteeringFormationRef K2_GetFormation();

//BEGIN IFormationProxy Interface 
	virtual void AddFormationBehavior(FPSFormationBehavior InBehavior) override;
	virtual void EnqueueFormationBehavior(FPSFormationBehavior InBehavior) override;
	virtual void RemoveFormationBehavior(FPSFormationBehavior InBehavior) override;
	virtual void ClearFormationBehaviors() override;
	virtual void AddMemberDependency(ISteerable* Member) override;
	virtual void RemoveMemberDependency(ISteerable* Member) override;

	FORCEINLINE virtual FVector GetAnchorLocation() const override
    {
        return UpdatedComponent ? UpdatedComponent->GetComponentLocation() : FVector();
    }

	FORCEINLINE virtual FQuat GetAnchorOrientation() const override
    {
        return UpdatedComponent ? UpdatedComponent->GetComponentQuat() : FQuat::Identity;
    }

	FORCEINLINE virtual void SetAnchorLocationAndOrientation(const FVector& Location, const FQuat& Orientation) override
    {
        if (IsValid(UpdatedComponent))
        {
            UpdatedComponent->SetWorldLocationAndRotation(Location, Orientation, false);
        }
    }

	FORCEINLINE virtual void SetAnchorLocation(const FVector& Location) override
    {
        if (IsValid(UpdatedComponent))
        {
            UpdatedComponent->SetWorldLocation(Location, false);
        }
    }

	FORCEINLINE virtual void SetAnchorOrientation(const FQuat& Orientation) override
    {
        if (IsValid(UpdatedComponent))
        {
            UpdatedComponent->SetWorldRotation(Orientation, false);
        }
    }
//END IFormationProxy Interface 

protected:

    void UpdateFormation(float DeltaTime);

    FORCEINLINE bool HasValidData() const;
};
