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

#include "SplineFollowingMovementComponent.h"
#include "VSmoothDeltaActor.h"
#include "Components/SplineComponent.h"

void USplineFollowingMovementComponent::MovementSourceChange()
{
    // Acquire new spline source
    if (SDOwner)
    {
        SplineSource = SDOwner->GetTypedMovementSource<USplineComponent>();
    }
    else
    {
        SplineSource = nullptr;
    }

    Super::MovementSourceChange();
}

void USplineFollowingMovementComponent::MovementUpdate(float DeltaTime)
{
    // Perform movement update with impact handling
    MovementUpdateImpl(DeltaTime, true);
}

void USplineFollowingMovementComponent::MovementUpdateImpl(float DeltaTime, bool bHandleImpact)
{
    check(HasValidData());

    // No valid spline simulation data, abort
    if (! HasValidSource())
    {
        return;
    }

    bJustTeleported = false;

    const float SplineTime = GetSplineTime(SplineSource->Duration);
    const FVector OldLocation = UpdatedComponent->GetComponentLocation();
    const FVector NewLocation = SplineSource->GetLocationAtTime(SplineTime, ESplineCoordinateSpace::World, true);
    const FVector DeltaLocation = NewLocation - OldLocation;
    const FQuat NewRotation = SplineSource->GetQuaternionAtTime(SplineTime, ESplineCoordinateSpace::World, true);

    FHitResult Hit(1.f);
    SafeMoveUpdatedComponent(DeltaLocation, NewRotation, true, Hit);

    if (bHandleImpact && Hit.Time < 1.f)
    {
        HandleImpact(Hit, DeltaTime, DeltaLocation);
    }

    if (!bJustTeleported)
    {
        Velocity = (UpdatedComponent->GetComponentLocation() - OldLocation) / DeltaTime;
    }
}

void USplineFollowingMovementComponent::MoveSmooth(float DeltaTime)
{
    // Perform movement update without handling hit impact on client proxies
    MovementUpdateImpl(DeltaTime, false);
}

void USplineFollowingMovementComponent::MoveSmooth_Visual()
{
    check(HasValidData());

    USceneComponent* Mesh = SDOwner->GetMeshRoot();

    // No valid simulation data, abort
    if (! HasValidSource())
    {
        return;
    }

    if (NetworkSmoothingMode == ENetworkSmoothingMode::Linear)
    {
    //    // Adjust capsule rotation and mesh location. Optimized to trigger only one transform chain update.
    //    // If we know the rotation is changing that will update children, so it's sufficient to set RelativeLocation directly on the mesh.
    //    const FVector NewRelLocation = ClientData->MeshRotationOffset.UnrotateVector(ClientData->MeshTranslationOffset) + SDOwner->GetBaseTranslationOffset();
    //    if (!UpdatedComponent->GetComponentQuat().Equals(ClientData->MeshRotationOffset, SCENECOMPONENT_QUAT_TOLERANCE))
    //    {
    //        const FVector OldLocation = Mesh->RelativeLocation;
    //        const FRotator OldRotation = UpdatedComponent->RelativeRotation;
    //        Mesh->RelativeLocation = NewRelLocation;
    //        UpdatedComponent->SetWorldRotation(ClientData->MeshRotationOffset);

    //        // If we did not move from SetWorldRotation, we need to at least call SetRelativeLocation
    //        // since we were relying on the UpdatedComponent to update the transform of the mesh
    //        if (UpdatedComponent->RelativeRotation == OldRotation)
    //        {
    //            Mesh->RelativeLocation = OldLocation;
    //            Mesh->SetRelativeLocation(NewRelLocation);
    //        }
    //    }
    //    else
    //    {
    //        Mesh->SetRelativeLocation(NewRelLocation);
    //    }
    }
    else if (NetworkSmoothingMode == ENetworkSmoothingMode::Exponential)
    {
        float OffsetTime = CurrentSimulationTime + SmoothSimulationOffset;

        if (OffsetTime > 0.f)
        {
            OffsetTime = GetClampedSimulationTime(OffsetTime);
        }
        else
        {
            OffsetTime = GetClampedSimulationTime(Duration-OffsetTime);
        }

        const float SplineTime = GetSplineTime(OffsetTime, SplineSource->Duration);
        Mesh->SetWorldTransform(SplineSource->GetTransformAtTime(SplineTime, ESplineCoordinateSpace::World, true));
    }
    else if (NetworkSmoothingMode == ENetworkSmoothingMode::Replay)
    {
    //    if (!UpdatedComponent->GetComponentQuat().Equals(ClientData->MeshRotationOffset, SCENECOMPONENT_QUAT_TOLERANCE) || !UpdatedComponent->GetComponentLocation().Equals(ClientData->MeshTranslationOffset, KINDA_SMALL_NUMBER))
    //    {
    //        UpdatedComponent->SetWorldLocationAndRotation(ClientData->MeshTranslationOffset, ClientData->MeshRotationOffset);
    //    }
    }
    else
    {
        // Unhandled mode
    }
}

void USplineFollowingMovementComponent::SnapToCurrentTime()
{
    check(HasValidData());

    // No valid spline simulation data, abort
    if (! HasValidSource())
    {
        return;
    }

    const float SplineTime = GetSplineTime(SplineSource->Duration);
    UpdatedComponent->SetWorldTransform(SplineSource->GetTransformAtTime(SplineTime, ESplineCoordinateSpace::World, true));

    bJustTeleported = true;
}

float USplineFollowingMovementComponent::GetClampedSmoothSimulationOffset(float InSimulationTimeDelta) const
{
    if (HasValidSource())
    {
        const float SplineLength = SplineSource->GetSplineLength();

        if (SplineLength > 0.f)
        {
            const float InvSplineLength = 1.f/SplineLength;
            const float AbsSmoothDelta = FMath::Abs(InSimulationTimeDelta);
            const float MaxSmoothDelta = NetworkMaxSmoothUpdateDistance * InvSplineLength;

            if (AbsSmoothDelta < MaxSmoothDelta)
            {
                return InSimulationTimeDelta;
            }
            else
            {
                const float SmoothDeltaSign = InSimulationTimeDelta>0.f ? 1.f : -1.f;
                const float NoSmoothDelta = NetworkNoSmoothUpdateDistance * InvSplineLength;
                return (AbsSmoothDelta < NoSmoothDelta) ? (MaxSmoothDelta*SmoothDeltaSign) : 0.f;
            }
        }
    }

    // Invalid required data, return zero offset
    return 0.f;
}

float USplineFollowingMovementComponent::GetSplineTime(float InSplineDuration) const
{
    return GetSplineTime(CurrentSimulationTime, InSplineDuration);
}

float USplineFollowingMovementComponent::GetSplineTime(float InSimulationTime, float InSplineDuration) const
{
    return GetSplineTime(CurrentSimulationTime, Duration, InSplineDuration);
}

float USplineFollowingMovementComponent::GetSplineTime(float InSimulationTime, float InSimulationDuration, float InSplineDuration) const
{
    return InSimulationTime * (InSplineDuration / InSimulationDuration);
}

bool USplineFollowingMovementComponent::HasValidSource() const
{
    return SplineSource && Duration > MIN_TICK_TIME && SplineSource->Duration > MIN_TICK_TIME;
}
