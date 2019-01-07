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
#include "UObject/UObjectGlobals.h"
#include "Engine/NetSerialization.h"
#include "Engine/EngineTypes.h"
#include "Engine/EngineBaseTypes.h"
#include "Interfaces/NetworkPredictionInterface.h"

#include "VPCMovementTypes.generated.h"

class UVPCMovementComponent;
class FNetworkPredictionData_Client_VPC;
class FNetworkPredictionData_Server_VPC;

namespace VPCPackedMovementModeConstants
{
    const uint32 GroundShift = FMath::CeilLogTwo(MOVE_MAX);
    const uint8 GroundMask = (1 << GroundShift) - 1;
    const uint8 CustomModeThr = 2 * (1 << GroundShift);
}

/** 
 * Tick function that calls UVPCMovementComponent::PostPhysicsTickComponent
 **/
USTRUCT()
struct FVPCMovementComponentPostPhysicsTickFunction : public FTickFunction
{
    GENERATED_USTRUCT_BODY()

    /** CharacterMovementComponent that is the target of this tick **/
    UVPCMovementComponent* Target;

    /** 
     * Abstract function actually execute the tick. 
     * @param DeltaTime - frame time to advance, in seconds
     * @param TickType - kind of tick for this frame
     * @param CurrentThread - thread we are executing on, useful to pass along as new tasks are created
     * @param MyCompletionGraphEvent - completion event for this task. Useful for holding the completion of this task until certain child tasks are complete.
     **/
    virtual void ExecuteTick(float DeltaTime, enum ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent) override;

    /** Abstract function to describe this tick. Used to print messages about illegal cycles in the dependency graph **/
    virtual FString DiagnosticMessage() override;
};

template<>
struct TStructOpsTypeTraits<FVPCMovementComponentPostPhysicsTickFunction> : public TStructOpsTypeTraitsBase2<FVPCMovementComponentPostPhysicsTickFunction>
{
    enum
    {
        WithCopy = false
    };
};

class FVPCReplaySample
{
public:
    FVPCReplaySample() : Time(0.0f)
    {
    }

    friend STEERINGSYSTEMPLUGIN_API FArchive& operator<<( FArchive& Ar, FVPCReplaySample& V );

    FVector         Location;
    FRotator        Rotation;
    FVector         Velocity;
    FVector         Acceleration;
    float           Time;                   // This represents time since replay started
};

class STEERINGSYSTEMPLUGIN_API FNetworkPredictionData_Client_VPC : public FNetworkPredictionData_Client, protected FNoncopyable
{
public:

    FNetworkPredictionData_Client_VPC(const UVPCMovementComponent& ClientMovement);
    virtual ~FNetworkPredictionData_Client_VPC();

    /** Current TimeStamp for sending new Moves to the Server. */
    float CurrentTimeStamp;

    // Mesh smoothing variables (for network smoothing)

    /** Used for position smoothing in net games (only used by linear smoothing). */
    FVector OriginalMeshTranslationOffset;

    /** World space offset of the mesh. Target value is zero offset. Used for position smoothing in net games. */
    FVector MeshTranslationOffset;

    /** Used for rotation smoothing in net games (only used by linear smoothing). */
    FQuat OriginalMeshRotationOffset;

    /** Component space offset of the mesh. Used for rotation smoothing in net games. */
    FQuat MeshRotationOffset;

    /** Target for mesh rotation interpolation. */
    FQuat MeshRotationTarget;

    /** Used for remembering how much time has passed between server corrections */
    float LastCorrectionDelta;

    /** Used to track time of last correction */
    float LastCorrectionTime;

    /** Used to track the timestamp of the last server move. */
    double SmoothingServerTimeStamp;

    /** Used to track the client time as we try to match the server.*/
    double SmoothingClientTimeStamp;

    /**
     * Copied value from UVPCMovementComponent::NetworkMaxSmoothUpdateDistance.
     * @see UVPCMovementComponent::NetworkMaxSmoothUpdateDistance
     */
    float MaxSmoothNetUpdateDist;

    /**
     * Copied value from UVPCMovementComponent::NetworkNoSmoothUpdateDistance.
     * @see UVPCMovementComponent::NetworkNoSmoothUpdateDistance
     */
    float NoSmoothNetUpdateDist;

    /**
     * How long to take to smoothly interpolate from the old pawn position on the client to the corrected one sent by the server.
     * Must be >= 0. Not used for linear smoothing.
     */
    float SmoothNetUpdateTime;

    /**
     * How long to take to smoothly interpolate from the old pawn rotation on the client to the corrected one sent by the server.
     * Must be >= 0. Not used for linear smoothing.
     */
    float SmoothNetUpdateRotationTime;
    
    /** 
     * Max delta time for a given move, in real seconds
     * Based off of AGameNetworkManager::MaxMoveDeltaTime config setting, but can be modified per actor if needed.
     *
     * This value is mirrored in FNetworkPredictionData_Server, which is what server logic runs off of.
     * Client needs to know this in order to not send move deltas that are going to get clamped anyway (meaning
     * they'll be rejected/corrected).
     */
    float MaxMoveDeltaTime;

    /** Values used for visualization and debugging of simulated net corrections */
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
    FVector LastSmoothLocation_Debug;
    FVector LastServerLocation_Debug;
    float   SimulatedDebugDrawTime_Debug;
#endif

    /** Array of replay samples that we use to interpolate between to get smooth location/rotation/velocity/ect */
    TArray< FVPCReplaySample > ReplaySamples;
};

class STEERINGSYSTEMPLUGIN_API FNetworkPredictionData_Server_VPC : public FNetworkPredictionData_Server, protected FNoncopyable
{
public:

    FNetworkPredictionData_Server_VPC(const UVPCMovementComponent& ServerMovement);
    virtual ~FNetworkPredictionData_Server_VPC();

    /** Last time server updated client with a move correction */
    float LastUpdateTime;
};
