// GTAIViewModel_Map.h
// MVVM ViewModel for the circular radar (HUD) and the full Map app. Controls
// rotation mode (rotated vs north-up), zoom level, and the player-set waypoint.
// See design doc sections 4.1, 4.4. Under namespace GTAI::UI.
#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "GTAI_UI.h"
#include "GTAIViewModel_Map.generated.h"

UENUM(BlueprintType)
enum class EGTAIRadarMode : uint8
{
    Rotated,   // world rotates around fixed center blip (default, GTA V style)
    NorthUp    // blip rotates, map oriented to world north
};

UCLASS()
class GTAIUI_API UGTAIViewModel_Map : public UMVVMViewModelBase
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, FieldNotify, Getter)
    EGTAIRadarMode GetRadarMode() const { return RadarMode; }
    UFUNCTION(BlueprintCallable, FieldNotify, Setter)
    void SetRadarMode(EGTAIRadarMode InMode);

    UFUNCTION(BlueprintCallable, FieldNotify, Getter)
    int32 GetZoomLevel() const { return ZoomLevel; }
    UFUNCTION(BlueprintCallable, FieldNotify, Setter)
    void SetZoomLevel(int32 InLevel);

    /** Cycle through 2-3 zoom steps (D-pad / scroll). */
    UFUNCTION(BlueprintCallable, Category = "GTAI|UI|Map")
    void CycleZoom();

    UFUNCTION(BlueprintCallable, FieldNotify, Getter)
    bool HasWaypoint() const { return bHasWaypoint; }

    UFUNCTION(BlueprintCallable, FieldNotify, Getter)
    FVector GetWaypoint() const { return Waypoint; }
    UFUNCTION(BlueprintCallable, FieldNotify, Setter)
    void SetWaypoint(const FVector& InLocation);

    UFUNCTION(BlueprintCallable, FieldNotify, Getter)
    float GetPlayerHeadingDeg() const { return PlayerHeadingDeg; }
    UFUNCTION(BlueprintCallable, FieldNotify, Setter)
    void SetPlayerHeading(float Deg) { PlayerHeadingDeg = Deg; BroadcastFieldValueChanged(GET_MEMBER_NAME_CHECKED(UGTAIViewModel_Map, PlayerHeadingDeg)); }

    /** World units revealed by the radar at the current zoom (e.g. 300m). */
    UFUNCTION(BlueprintCallable, FieldNotify, Getter)
    float GetRadarRange() const { return RadarRange; }

protected:
    EGTAIRadarMode RadarMode = EGTAIRadarMode::Rotated;
    int32 ZoomLevel = 1;
    int32 MaxZoomLevels = 3;
    bool bHasWaypoint = false;
    FVector Waypoint = FVector::ZeroVector;
    float PlayerHeadingDeg = 0.f;
    float RadarRange = 30000.f; // cm
};
