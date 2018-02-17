// 

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"
#include "UObject/ObjectMacros.h"
#include "SteeringTypes.generated.h"

typedef TSharedPtr<class FSteeringBehavior>     FPSSteeringBehavior;
typedef TDoubleLinkedList<FPSSteeringBehavior>  FBehaviorList;
typedef FBehaviorList::TDoubleLinkedListNode    FBehaviorListNode;

typedef TSharedPtr<class FFormationBehavior>            FPSFormationBehavior;
typedef TDoubleLinkedList<FPSFormationBehavior>         FFormationBehaviorList;
typedef FFormationBehaviorList::TDoubleLinkedListNode   FFormationBehaviorListNode;

class ISteeringBehaviorBase
{
protected:

    // Whether the behavior has been initialized
    bool bActivated = false;

    virtual void OnActivated()
    {
        // Blank implementation
    }

    virtual void OnDeactivated()
    {
        // Blank implementation
    }

public:

    void Activate()
    {
        if (! bActivated && HasValidData())
        {
            bActivated = true;
            OnActivated();
        }
    }

    void Deactivate()
    {
        if (bActivated)
        {
            bActivated = false;
            OnDeactivated();
        }
    }

    FORCEINLINE bool IsActive() const
    {
        return bActivated;
    }

    virtual FName GetType() const
    {
        return FName();
    }

    virtual bool HasValidData() const = 0;
};

USTRUCT(BlueprintType)
struct STEERINGSYSTEMPLUGIN_API FSteeringTarget
{
    GENERATED_USTRUCT_BODY()

    UPROPERTY(BlueprintReadWrite, VisibleAnywhere)
    FVector Location;

    UPROPERTY(BlueprintReadWrite, VisibleAnywhere)
    FQuat Rotation;

    UPROPERTY(BlueprintReadWrite, VisibleAnywhere)
    USceneComponent* TargetComponent = nullptr;

    FSteeringTarget() = default;

    FSteeringTarget(const FVector& InLocation) : Location(InLocation)
    {
    }

    FSteeringTarget(const FQuat& InRotation) : Rotation(InRotation)
    {
    }

    FSteeringTarget(const FRotator& InRotation) : Rotation(InRotation)
    {
    }

    FORCEINLINE FVector GetLocation() const
    {
        return IsValid(TargetComponent) ? TargetComponent->GetComponentLocation() : Location;
    }

    FORCEINLINE FVector GetForwardVector() const
    {
        return IsValid(TargetComponent) ? TargetComponent->GetForwardVector() : Rotation.GetForwardVector();
    }

    FORCEINLINE FQuat GetRotation() const
    {
        return IsValid(TargetComponent) ? TargetComponent->GetComponentQuat() : Rotation;
    }
};

struct FSteeringAcceleration
{
    /** The linear component of this steering acceleration */
    FVector Linear;

    /** Whether acceleration is enabled in the resulting movement */
    bool bEnableAcceleration;

    /** Whether acceleration is enabled in the resulting movement */
    bool bLockOrientation;

    /**
     * Default constructor
     */
    FSteeringAcceleration()
        : Linear(ForceInitToZero)
        , bEnableAcceleration(false)
        , bLockOrientation(false)
    {
    }


    /**
     * Creates a {@code SteeringAcceleration} with the given linear and angular components.
     */
    FSteeringAcceleration(const FVector& InLinear)
        : Linear(InLinear)
        , bEnableAcceleration(false)
        , bLockOrientation(false)
    {
    }

    /**
     * Creates a {@code SteeringAcceleration} with the given linear and angular components.
     */
    explicit FSteeringAcceleration(const FVector& InLinear, bool bInEnableAcceleration, bool bInLockOrientation)
        : Linear(InLinear)
        , bEnableAcceleration(bInEnableAcceleration)
        , bLockOrientation(bInLockOrientation)
    {
    }
};
