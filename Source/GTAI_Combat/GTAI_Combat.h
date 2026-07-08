// GTAI_Combat.h
// Module header for the GTAI_Combat gameplay module.
#pragma once

#include "Modules/ModuleManager.h"

class FGTAI_CombatModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
};
