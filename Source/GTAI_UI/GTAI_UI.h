// GTAI_UI.h
// Module entry + namespace declaration for the GTAI UI module.
// All UI code lives under namespace GTAI::UI.
#pragma once

#include "CoreMinimal.h"

// Module API export macro.
#ifndef GTAIUI_API
#define GTAIUI_API DLLEXPORT
#endif

// Canonical UI namespace. The rest of the GTAI project uses GTA7::; we keep
// GTAI::UI as mandated by the engineering brief and expose a compatibility
// alias so other modules can also reference UI types as GTA7::UI.
namespace GTAI::UI
{
    // Forward declarations of core UI types to keep headers light.
    class UGTAIUIManager;
    class UGTAIUserWidget;
    class UGTAIButton;
    class UGTAIUWRadar;
    class UGTAIUWHud;
    class UGTAIUWPhone;
    class UGTAIUWNotificationLayer;
}

// Compatibility alias: lets the rest of the project reach UI types as GTA7::UI.
namespace GTA7
{
    using namespace GTAI::UI;
}
