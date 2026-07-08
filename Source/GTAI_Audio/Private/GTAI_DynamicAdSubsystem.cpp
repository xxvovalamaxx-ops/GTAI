// Copyright GTAI. All Rights Reserved.
// SIREN — Dynamic Ad + Music Generator + LipSync stubs

#include "GTAI_DynamicAdSubsystem.h"
void UGTAI_DynamicAdSubsystem::Initialize() {}
FString UGTAI_DynamicAdSubsystem::GenerateAd(const FString& Brand, const FString& TargetAudience) {
    return FString::Printf(TEXT("This message brought to you by %s."), *Brand);
}
