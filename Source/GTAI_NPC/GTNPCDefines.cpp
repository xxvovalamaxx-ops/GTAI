// Copyright GTAI. All Rights Reserved.
// LUMEN — NPC Defines + LLM Types + Crowd Types (shared type registrations)

#include "GTNPCDefines.h"
FString GetActivityName(EPedestrianActivity Activity) {
    static const TArray<FString> Names = {TEXT("Idle"), TEXT("Walking"), TEXT("Working"), TEXT("Shopping"), TEXT("Leisure"), TEXT("Sleeping"), TEXT("Commuting"), TEXT("Panicking")};
    int32 Idx = static_cast<int32>(Activity);
    return Names.IsValidIndex(Idx) ? Names[Idx] : TEXT("Unknown");
}
