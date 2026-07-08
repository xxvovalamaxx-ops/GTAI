// GTAI_RadioSystem.h
// Station scheduler that recreates the GTA V radio loop: DJ -> song -> ads ->
// news breaks -> DJ. Supports AI-native twists: LLM DJ banter referencing the
// player, dynamic ads (see GTAI_DynamicAdSubsystem), and news takeovers driven
// by world events. One active station at a time; the player switches source.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GTAI_AudioTypes.h"
#include "GTAI_RadioSystem.generated.h"

class UGTAI_DJSubsystem;
class UGTAI_DynamicAdSubsystem;
class USoundBase;

UCLASS()
class GTAI_AUDIO_API UGTAI_RadioSystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// Power / source ----------------------------------------------------------
	UFUNCTION(BlueprintCallable, Category = "GTAI|Radio")
	void PowerOn(EGTAI_StationId InitialStation = EGTAI_StationId::PulseFM);

	UFUNCTION(BlueprintCallable, Category = "GTAI|Radio")
	void PowerOff();

	UFUNCTION(BlueprintCallable, Category = "GTAI|Radio")
	void SwitchStation(EGTAI_StationId Station);

	UFUNCTION(BlueprintCallable, Category = "GTAI|Radio")
	EGTAI_StationId GetCurrentStation() const { return ActiveStation; }

	UFUNCTION(BlueprintCallable, Category = "GTAI|Radio")
	bool IsOn() const { return bPoweredOn; }

	// News takeover: forces a news break on the active station (or any station
	// if bOverrideAll). Used when a breaking world event fires.
	UFUNCTION(BlueprintCallable, Category = "GTAI|Radio")
	void RequestNewsTakeover(const FString& Headline, bool bOverrideAll = true);

	// DJ / ads ----------------------------------------------------------------
	UGTAI_DJSubsystem*        GetDJSubsystem()  const { return DJSubsystem; }
	UGTAI_DynamicAdSubsystem* GetAdSubsystem()  const { return AdSubsystem; }

	// Queue a one-off DJ line referencing current world/player state.
	UFUNCTION(BlueprintCallable, Category = "GTAI|Radio")
	void QueueDJBanter(const FString& OverrideText = TEXT(""));

	// Tick the scheduler (called from the manager or a world tick).
	void TickScheduler(float DeltaTime);

protected:
	UPROPERTY()
	TObjectPtr<UGTAI_DJSubsystem> DJSubsystem;

	UPROPERTY()
	TObjectPtr<UGTAI_DynamicAdSubsystem> AdSubsystem;

	EGTAI_StationId ActiveStation = EGTAI_StationId::None;
	bool bPoweredOn = false;

	// Current segment playback state.
	TArray<GTAI::Audio::FRadioSegment> CurrentSchedule;
	int32 ScheduleIndex = 0;
	float SegmentElapsed = 0.f;
	bool bInNewsTakeover = false;

	void PlayNextSegment();
	void ScheduleStation(EGTAI_StationId Station);
	void OnSegmentFinished();
};
