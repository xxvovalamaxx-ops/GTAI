// GTAI_DynamicAdSubsystem.h
// Generates AI commercials that reference in-game brands and the player's recent
// activity, then synthesizes them with ElevenLabs. Ads are cached per
// (brand, activitySignature) to avoid regeneration spam, and respect sensitivity
// flags so tone-deaf ads never fire (e.g. no luxury ads after a massacre).
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GTAI_AudioTypes.h"
#include "GTAI_DynamicAdSubsystem.generated.h"

class UGTAI_VoiceSynthesis;
class UGTAI_BrandDataAsset;

// A single in-game brand that can be advertised.
USTRUCT(BlueprintType)
struct GTAI_AUDIO_API FGTAI_Brand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ad")
	FString BrandName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ad")
	FGameplayTag Category;                 // e.g. "Brand.Car", "Brand.Clinic"

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ad")
	FString ElevenLabsVoiceId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ad")
	TArray<FString> SloganTemplates;

	// Activity categories that make this brand a good ad target.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ad")
	TArray<FGameplayTag> TriggerActivities;

	// If any of these world states are active, suppress this brand's ads.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ad")
	TArray<FGameplayTag> SensitivityFlags;
};

UCLASS()
class GTAI_AUDIO_API UGTAI_DynamicAdSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// Request a contextual ad. Picks a brand whose triggers match recent player
	// activity, generates the copy via LLM, and synthesizes via ElevenLabs.
	UFUNCTION(BlueprintCallable, Category = "GTAI|Radio|Ads")
	void RequestAd(const TArray<FString>& RecentPlayerActions,
	               TFunction<void(class USoundBase*)> OnAdReady);

	UFUNCTION(BlueprintCallable, Category = "GTAI|Radio|Ads")
	void RegisterBrandAsset(UGTAI_BrandDataAsset* Asset);

	// Builds the ad copy prompt; pure virtual hook so copy can be tuned per ship.
	static FString BuildAdPrompt(const FGTAI_Brand& Brand, const FString& ActivityHook);

protected:
	UPROPERTY()
	TObjectPtr<UGTAI_VoiceSynthesis> VoiceSynthesis;

	UPROPERTY()
	TArray<TObjectPtr<UGTAI_BrandDataAsset>> BrandAssets;

	TMap<FString, TObjectPtr<class USoundBase>> AdCache; // key: brand+activity sig

	FGTAI_Brand PickBrand(const TArray<FString>& RecentActions) const;
	FString ActivitySignature(const TArray<FString>& Actions) const;
};
