// 

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "Templates/SubclassOf.h"
#include "UObject/CoreNet.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Actor.h"
#include "VSmoothDeltaActor.generated.h"

class UVSmoothDeltaMovementComponent;
class UArrowComponent;

/** 
 *
 */
UCLASS(config=Game, BlueprintType, Blueprintable)
class STEERINGSYSTEMPLUGIN_API AVSmoothDeltaActor : public AActor
{
    GENERATED_BODY()

private:

    /**
     * The ShapeComponent being used for movement collision.
     */
    UPROPERTY(Category = SmoothDeltaActor, VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
    USceneComponent* ShapeComponent;

    /** The mesh attach root. */
    UPROPERTY(Category = SmoothDeltaActor, VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
    USceneComponent* MeshRoot;

    /**
     * Movement component used for smooth delta movement.
     */
    UPROPERTY(Category = SmoothDeltaActor, VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
    UVSmoothDeltaMovementComponent* SDMovement;

#if WITH_EDITORONLY_DATA
    UPROPERTY()
    UArrowComponent* ArrowComponent;
#endif

protected:

    /** Last server tick timestamp, replicated to simulated proxies. */
    UPROPERTY(Replicated)
    float ReplicatedUpdateTimeStamp;

    /** Last server simulation time, replicated to simulated proxies. */
    UPROPERTY(Replicated)
    float ReplicatedSimulationTime;

    /** Current movement source. */
    UPROPERTY()
    UPrimitiveComponent* MovementSource;

    /** Replicated version of movement source. Read-only on simulated proxies. */
    UPROPERTY(ReplicatedUsing=OnRep_ReplicatedMovementSource)
    UPrimitiveComponent* ReplicatedMovementSource;

public:

    /** Name of the ShapeComponent. Use this name if you want to use a different class (with ObjectInitializer.SetDefaultSubobjectClass) */
    static FName ShapeComponentName;

    /** Name of the MeshRootComponent */
    static FName MeshRootComponentName;

    /** Name of the movement component. Use this name if you want to use a different class (with ObjectInitializer.SetDefaultSubobjectClass). */
    static FName MovementComponentName;

    AVSmoothDeltaActor(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

    /** Accessor for ReplicatedUpdateTimeStamp. */
    FORCEINLINE float GetReplicatedUpdateTimeStamp() const
    {
        return ReplicatedUpdateTimeStamp;
    }

    /** Accessor for ReplicatedSimulationTime. */
    FORCEINLINE float GetReplicatedSimulationTime() const
    {
        return ReplicatedSimulationTime;
    }

    /** Accessor for MovementSource. */
    FORCEINLINE UPrimitiveComponent* GetMovementSource() const
    {
        return MovementSource;
    }

    /** Cast accessor for MovementSource. */
    template<class T>
    T* GetTypedMovementSource() const
    {
        return Cast<T>(MovementSource);
    }

    /** Event called after actor's movement source changes. */
    virtual void MovementSourceChange();

    /** Sets the movement source component. */
    UFUNCTION(BlueprintCallable, Category="SmoothDeltaActor")
    virtual void SetMovementSource(UPrimitiveComponent* NewMovementSource);

    /**
     * Return owned UVSmoothDeltaMovementComponent.
     */
    UFUNCTION(BlueprintCallable, Category="SmoothDeltaActor")
    virtual UVSmoothDeltaMovementComponent* GetMovementComponent() const;

    //~ Begin AActor Interface.
	virtual void PostInitializeComponents() override;
    void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void PreReplication(IRepChangedPropertyTracker & ChangedPropertyTracker) override;
    //virtual void PreReplicationForReplay(IRepChangedPropertyTracker & ChangedPropertyTracker) override;
    virtual void PostNetReceive() override;
    //virtual void PostNetReceiveLocationAndRotation() override;
    virtual void TornOff() override;
    //~ End AActor Interface.

    /** Rep notify for ReplicatedMovementSource */
    UFUNCTION()
    virtual void OnRep_ReplicatedMovementSource();

    /**
     * Called on client after position update is received to respond to the new location and rotation.
     * Actual change in location is expected to occur in SDMovement->SmoothCorrection(), after which this occurs.
     */
    virtual void OnUpdateSimulatedPosition(const FVector& OldLocation, const FQuat& OldRotation);

public:
    /** Returns ShapeComponent subobject **/
    USceneComponent* GetShapeComponent() const;
    /** Returns Mesh Root subobject **/
    USceneComponent* GetMeshRoot() const;
    /** Returns VSmoothDeltaMovementComponent subobject **/
    UVSmoothDeltaMovementComponent* GetSDMovement() const;
#if WITH_EDITORONLY_DATA
    /** Returns ArrowComponent subobject **/
    UArrowComponent* GetArrowComponent() const;
#endif
};

//////////////////////////////////////////////////////////////////////////
// Inline functions

/** Returns ShapeComponent subobject **/
FORCEINLINE USceneComponent* AVSmoothDeltaActor::GetShapeComponent() const { return ShapeComponent; }
/** Returns MeshRoot subobject **/
FORCEINLINE USceneComponent* AVSmoothDeltaActor::GetMeshRoot() const { return MeshRoot; }
/** Returns VSmoothDeltaMovementComponent subobject **/
FORCEINLINE UVSmoothDeltaMovementComponent* AVSmoothDeltaActor::GetSDMovement() const { return SDMovement; }
#if WITH_EDITORONLY_DATA
/** Returns ArrowComponent subobject **/
FORCEINLINE UArrowComponent* AVSmoothDeltaActor::GetArrowComponent() const { return ArrowComponent; }
#endif

UCLASS(config=Game, BlueprintType)
class STEERINGSYSTEMPLUGIN_API AVSmoothDeltaActor_BC : public AVSmoothDeltaActor
{
    GENERATED_BODY()

private:

    UPROPERTY(Category = Character, VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
    class UBoxComponent* BoxComponent;

public:
    /**
     * Default UObject constructor.
     */
    AVSmoothDeltaActor_BC(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
