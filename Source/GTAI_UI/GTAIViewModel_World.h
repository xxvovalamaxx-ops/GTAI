// GTAIViewModel_World.h
// MVVM ViewModel: world-level HUD state — wanted level, time/weather, and the
// registry of world blips shared by the HUD radar and the full Map app.
// See design doc sections 2.4, 2.9, 4.2. Under namespace GTAI::UI.
#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "GameplayTagContainer.h"
#include "GTAI_UI.h"
#include "GTAIViewModel_World.generated.h"

UENUM(BlueprintType)
enum class EGTAIBlipType : uint8
{
    Player        UMETA(DisplayName = "Player"),
    Mission       UMETA(DisplayName = "Mission"),
    MissionTarget UMETA(DisplayName = "Mission Target"),
    Shop          UMETA(DisplayName = "Shop"),
    WeaponShop    UMETA(DisplayName = "Weapon Shop"),
    ClothingShop  UMETA(DisplayName = "Clothing Shop"),
    Police        UMETA(DisplayName = "Police"),
    Enemy         UMETA(DisplayName = "Enemy"),
    Friend        UMETA(DisplayName = "Friend"),
    Vehicle       UMETA(DisplayName = "Vehicle"),
    Property      UMETA(DisplayName = "Property"),
    Collectible   UMETA(DisplayName = "Collectible"),
    Custom        UMETA(DisplayName = "Custom")
};

USTRUCT(BlueprintType)
struct GTAIUI_API FGTAIWorldBlip
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector WorldLocation = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EGTAIBlipType Type = EGTAIBlipType::Custom;

    /** Used for layer filtering (show/hide groups of blips). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGameplayTag Category;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FText Label;

    /** Draw an edge arrow when outside the radar radius. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bOffScreenArrow = true;

    /** Optional icon override (otherwise resolved from Type). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSoftObjectPtr<UTexture2D> IconOverride;
};

UCLASS()
class GTAIUI_API UGTAIViewModel_World : public UMVVMViewModelBase
{
    GENERATED_BODY()

public:
    // ---- Wanted ----
    UFUNCTION(BlueprintCallable, FieldNotify, Getter)
    float GetWantedLevel() const { return WantedLevel; }
    UFUNCTION(BlueprintCallable, FieldNotify, Setter)
    void SetWantedLevel(float InValue);

    UFUNCTION(BlueprintCallable, FieldNotify, Getter)
    int32 GetWantedStars() const { return FMath::Clamp(FMath::RoundToInt(WantedLevel), 0, 5); }

    // ---- Time / Weather ----
    UFUNCTION(BlueprintCallable, FieldNotify, Getter)
    FText GetClockString() const { return ClockString; }
    UFUNCTION(BlueprintCallable, FieldNotify, Setter)
    void SetClock(const FText& InText) { ClockString = InText; BroadcastFieldValueChanged(GET_MEMBER_NAME_CHECKED(UGTAIViewModel_World, ClockString)); }

    UFUNCTION(BlueprintCallable, FieldNotify, Getter)
    FText GetWeatherString() const { return WeatherString; }
    UFUNCTION(BlueprintCallable, FieldNotify, Setter)
    void SetWeather(const FText& InText) { WeatherString = InText; BroadcastFieldValueChanged(GET_MEMBER_NAME_CHECKED(UGTAIViewModel_World, WeatherString)); }

    // ---- Blips ----
    UFUNCTION(BlueprintCallable, Category = "GTAI|UI|Map")
    void RegisterBlip(const FGTAIWorldBlip& Blip);

    UFUNCTION(BlueprintCallable, Category = "GTAI|UI|Map")
    void RemoveBlip(const FGTAIWorldBlip& Blip);

    UFUNCTION(BlueprintCallable, Category = "GTAI|UI|Map")
    const TArray<FGTAIWorldBlip>& GetBlips() const { return Blips; }

    /** Filter the blip list by enabled layer tags (Settings -> Map layers). */
    UFUNCTION(BlueprintCallable, Category = "GTAI|UI|Map")
    TArray<FGTAIWorldBlip> GetVisibleBlips(const FGameplayTagContainer& EnabledLayers) const;

    UFUNCTION(BlueprintCallable, FieldNotify, Getter)
    bool AreBlipsDirty() const { return bBlipsDirty; }

protected:
    float WantedLevel = 0.f;
    FText ClockString;
    FText WeatherString;

    UPROPERTY()
    TArray<FGTAIWorldBlip> Blips;

    bool bBlipsDirty = false;
};
