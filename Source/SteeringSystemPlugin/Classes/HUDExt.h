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
#include "GameFramework/Actor.h"
#include "GameFramework/HUD.h"
#include "UObject/ObjectMacros.h"
#include "HUDExt.generated.h"

UCLASS(notplaceable, transient, BlueprintType, Blueprintable)
class STEERINGSYSTEMPLUGIN_API AHUDExt : public AHUD 
{
    GENERATED_BODY()

    /**
     * Finds the array of actors inside a selection rectangle from a list of its owned components.
     */
    UFUNCTION(Category=HUD)
    void FindFilteredActorsInSelectionRectangle(const TArray<USceneComponent*>& Components, const FVector2D& FirstPoint, const FVector2D& SecondPoint, TArray<AActor*>& OutActors, bool bActorMustBeFullyEnclosed = false);

    /**
     * Finds the array of actors inside a selection rectangle from a list of its owned components.
     */
    UFUNCTION(Category=HUD)
    TArray<AActor*> GetFilteredActorsInSelectionRectangle(const TArray<USceneComponent*>& Components, const FVector2D& FirstPoint, const FVector2D& SecondPoint, bool bActorMustBeFullyEnclosed = false);
};
