/*
 
 -----
 
*/

#pragma once

#include "ModuleManager.h"

class FSteeringSystemPlugin : public IModuleInterface
{
public:

    /** IModuleInterface implementation */
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
};

DECLARE_STATS_GROUP(TEXT("Steering"), STATGROUP_Steering, STATCAT_Advanced);
