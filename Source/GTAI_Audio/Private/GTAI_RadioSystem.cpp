// Copyright GTAI. All Rights Reserved.
// AUDIO — GTAI_RadioSystem
// Station scheduler that recreates the GTA V radio loop: DJ -> song -> ads ->
// news breaks -> DJ. One active station at a time. The schedule is a rolling
// timeline of FRadioSegment entries; "audio" segments with a valid
// TSoftObjectPtr<USoundBase> play directly, while DJ/ad/news segments are
// generated (LLM + TTS) on demand and always fall back to pre-authored lines
// so the radio is never silent offline.
//
// Scheduling model:
//   - PowerOn()        -> picks a station and builds a rolling schedule.
//   - TickScheduler()  -> advanced by the AudioManager (or a world tick);
//                         accumulates SegmentElapsed and rolls to the next
//                         segment when the current one elapses.
//   - SwitchStation()  -> hard-cut to another station (interrupts current).
//   - RequestNewsTakeover() -> injects a news break at the next interruptible
//                         boundary, or immediately if the current segment allows.

#include "GTAI_RadioSystem.h"

#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

#include "GTAI_DJSubsystem.h"
#include "GTAI_DynamicAdSubsystem.h"
#include "GTAI_VoiceSynthesis.h"

// File-local weak handle to the currently playing radio audio component so we
// can stop it on a hard switch/takeover. Single GameInstance scenario; radio is
// a singleton subsystem per GameInstance so a file-static is acceptable here.
namespace
{
	TWeakObjectPtr<UAudioComponent> GActiveRadioComponent;

	// Logical default durations (seconds) for segment types that have no
	// authored audio (LLM/TTS driven or pure-placeholder). Keeps the
	// scheduler advancing even when assets are missing.
	float DefaultSegmentDuration(GTAI::Audio::FRadioSegment& Segment)
	{
		const FGameplayTag& Type = Segment.SegmentType;
		if (Type.MatchesTag(FGameplayTag::RequestGameplayTag(FName("Radio.Song"))))
		{
			return 180.f;
		}
		if (Type.MatchesTag(FGameplayTag::RequestGameplayTag(FName("Radio.DJBanter"))))
		{
			return 9.f;
		}
		if (Type.MatchesTag(FGameplayTag::RequestGameplayTag(FName("Radio.Ad"))))
		{
			return 18.f;
		}
		if (Type.MatchesTag(FGameplayTag::RequestGameplayTag(FName("Radio.NewsBreak"))))
		{
			return 22.f;
		}
		if (Type.MatchesTag(FGameplayTag::RequestGameplayTag(FName("Radio.StationID"))))
		{
			return 3.f;
		}
		return Segment.DurationSec > 0.f ? Segment.DurationSec : 10.f;
	}

	void StopActiveRadio()
	{
		if (UAudioComponent* Comp = GActiveRadioComponent.Get())
		{
			Comp->Stop();
		}
		GActiveRadioComponent.Reset();
	}

	// EGTAI_StationId is a plain (non-reflected) enum, so there is no UEnum to
	// query for a display name — map it manually for use as a DJ show identity.
	FString StationName(GTAI::Audio::EGTAI_StationId Station)
	{
		switch (Station)
		{
		case GTAI::Audio::EGTAI_StationId::PulseFM:    return TEXT("Pulse FM");
		case GTAI::Audio::EGTAI_StationId::NeonDrive:  return TEXT("Neon Drive");
		case GTAI::Audio::EGTAI_StationId::TheForum:   return TEXT("The Forum");
		case GTAI::Audio::EGTAI_StationId::NYCNow:     return TEXT("NYC Now");
		case GTAI::Audio::EGTAI_StationId::Airbrands:  return TEXT("Airbrands");
		case GTAI::Audio::EGTAI_StationId::ClassicNY:  return TEXT("Classic NY");
		case GTAI::Audio::EGTAI_StationId::Latido:     return TEXT("Latido");
		default:                                        return TEXT("Radio");
		}
	}
}

void UGTAI_RadioSystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Acquire sibling sub-systems (UE auto-creates UGameInstanceSubsystems).
	if (UGameInstance* GI = GetGameInstance())
	{
		DJSubsystem = GI->GetSubsystem<UGTAI_DJSubsystem>();
		AdSubsystem = GI->GetSubsystem<UGTAI_DynamicAdSubsystem>();
	}

	ActiveStation = EGTAI_StationId::None;
	bPoweredOn = false;
	ScheduleIndex = 0;
	SegmentElapsed = 0.f;
	bInNewsTakeover = false;
	CurrentSchedule.Empty();

	UE_LOG(LogTemp, Log, TEXT("GTAI_RadioSystem initialized (DJ=%s, Ad=%s)."),
		DJSubsystem ? TEXT("ok") : TEXT("missing"),
		AdSubsystem ? TEXT("ok") : TEXT("missing"));
}

void UGTAI_RadioSystem::Deinitialize()
{
	StopActiveRadio();
	CurrentSchedule.Empty();
	ScheduleIndex = 0;
	SegmentElapsed = 0.f;
	bInNewsTakeover = false;

	DJSubsystem = nullptr;
	AdSubsystem = nullptr;

	Super::Deinitialize();
}

// ---------------------------------------------------------------------------
// Power / source
// ---------------------------------------------------------------------------

void UGTAI_RadioSystem::PowerOn(EGTAI_StationId InitialStation)
{
	if (bPoweredOn)
	{
		// Already on — treat as a switch.
		SwitchStation(InitialStation);
		return;
	}

	bPoweredOn = true;
	bInNewsTakeover = false;
	ScheduleStation(InitialStation);
	PlayNextSegment();

	UE_LOG(LogTemp, Log, TEXT("GTAI_RadioSystem powered ON, station %d."),
		static_cast<uint8>(ActiveStation));
}

void UGTAI_RadioSystem::PowerOff()
{
	if (!bPoweredOn)
	{
		return;
	}

	StopActiveRadio();
	bPoweredOn = false;
	ActiveStation = EGTAI_StationId::None;
	CurrentSchedule.Empty();
	ScheduleIndex = 0;
	SegmentElapsed = 0.f;

	UE_LOG(LogTemp, Log, TEXT("GTAI_RadioSystem powered OFF."));
}

void UGTAI_RadioSystem::SwitchStation(EGTAI_StationId Station)
{
	if (Station == EGTAI_StationId::None)
	{
		return;
	}

	StopActiveRadio();
	ScheduleStation(Station);
	PlayNextSegment();

	UE_LOG(LogTemp, Log, TEXT("GTAI_RadioSystem switched to station %d."),
		static_cast<uint8>(ActiveStation));
}

void UGTAI_RadioSystem::RequestNewsTakeover(const FString& Headline, bool bOverrideAll)
{
	if (!bPoweredOn)
	{
		return;
	}

	bInNewsTakeover = true;

	// Build a news break segment for the active (or any) station.
	GTAI::Audio::FRadioSegment News;
	News.Station = ActiveStation;
	News.SegmentType = FGameplayTag::RequestGameplayTag(FName("Radio.NewsBreak"));
	News.DurationSec = 22.f;
	News.LLMPromptPackKey = Headline;
	News.bInterruptible = false; // news breaks are non-interruptible themselves

	if (bOverrideAll || CurrentSchedule.IsEmpty() ||
		(CurrentSchedule.IsValidIndex(ScheduleIndex) && CurrentSchedule[ScheduleIndex].bInterruptible))
	{
		// Inject immediately: hard-cut the current segment.
		StopActiveRadio();
		CurrentSchedule.Insert(News, ScheduleIndex);
		PlayNextSegment();

		UE_LOG(LogTemp, Log, TEXT("GTAI_RadioSystem news takeover (overrideAll=%d): %s"),
			bOverrideAll ? 1 : 0, *Headline);
	}
	else
	{
		// Queue it right after the current segment.
		CurrentSchedule.Insert(News, ScheduleIndex + 1);
		UE_LOG(LogTemp, VeryVerbose, TEXT("GTAI_RadioSystem news queued after current segment."));
	}
}

void UGTAI_RadioSystem::QueueDJBanter(const FString& OverrideText)
{
	if (!bPoweredOn || !DJSubsystem)
	{
		return;
	}

	// A one-off DJ line referencing current world/player state. We inject it as
	// an interruptible banter segment right after the current one (or play now
	// if the current segment is interruptible and quiet).
	GTAI::Audio::FRadioSegment Banter;
	Banter.Station = ActiveStation;
	Banter.SegmentType = FGameplayTag::RequestGameplayTag(FName("Radio.DJBanter"));
	Banter.DurationSec = 9.f;
	Banter.LLMPromptPackKey = OverrideText;
	Banter.bInterruptible = true;

	if (CurrentSchedule.IsValidIndex(ScheduleIndex) && CurrentSchedule[ScheduleIndex].bInterruptible)
	{
		StopActiveRadio();
		CurrentSchedule.Insert(Banter, ScheduleIndex);
		PlayNextSegment();
	}
	else
	{
		CurrentSchedule.Insert(Banter, ScheduleIndex + 1);
	}
}

void UGTAI_RadioSystem::TickScheduler(float DeltaTime)
{
	if (!bPoweredOn || CurrentSchedule.IsEmpty())
	{
		return;
	}

	// If we're waiting on async-generated audio for the current segment, the
	// duration is unknown; the completion callback drives advancement instead.
	SegmentElapsed += DeltaTime;

	const float Expected = DefaultSegmentDuration(CurrentSchedule[ScheduleIndex]);
	if (SegmentElapsed >= Expected)
	{
		OnSegmentFinished();
	}
}

// ---------------------------------------------------------------------------
// Scheduling internals
// ---------------------------------------------------------------------------

void UGTAI_RadioSystem::ScheduleStation(EGTAI_StationId Station)
{
	ActiveStation = Station;
	CurrentSchedule.Empty();
	ScheduleIndex = 0;
	SegmentElapsed = 0.f;

	// Build a rolling station timeline. Audio soft-pointers are intentionally
	// null here (no baked content yet) — music/song segments become placeholders
	// and DJ/ad/news segments are generated on demand. Designer wires real
	// USoundBase assets into these slots via a StationDataAsset later.
	auto AddSeg = [&](const TCHAR* TypeTag, float Dur, bool bInterruptible = true)
	{
		GTAI::Audio::FRadioSegment Seg;
		Seg.Station = Station;
		Seg.SegmentType = FGameplayTag::RequestGameplayTag(FName(TypeTag));
		Seg.DurationSec = Dur;
		Seg.bInterruptible = bInterruptible;
		CurrentSchedule.Add(Seg);
	};

	switch (Station)
	{
	case EGTAI_StationId::TheForum:
		// Talk radio: monologues back-to-back with station IDs.
		AddSeg(TEXT("Radio.StationID"), 3.f);
		AddSeg(TEXT("Radio.DJBanter"), 45.f); // monologue via DJ subsystem
		AddSeg(TEXT("Radio.Ad"), 18.f);
		AddSeg(TEXT("Radio.DJBanter"), 40.f);
		break;

	case EGTAI_StationId::NYCNow:
		// News: news breaks + ads.
		AddSeg(TEXT("Radio.StationID"), 3.f);
		AddSeg(TEXT("Radio.NewsBreak"), 22.f);
		AddSeg(TEXT("Radio.Ad"), 18.f);
		AddSeg(TEXT("Radio.NewsBreak"), 20.f);
		break;

	case EGTAI_StationId::Airbrands:
		// Commercials-only station.
		AddSeg(TEXT("Radio.StationID"), 3.f);
		AddSeg(TEXT("Radio.Ad"), 18.f);
		AddSeg(TEXT("Radio.Ad"), 18.f);
		AddSeg(TEXT("Radio.Ad"), 15.f);
		break;

	default:
		// Music stations (PulseFM, NeonDrive, ClassicNY, Latido): the classic
		// GTA loop — jingle -> song -> DJ -> ad -> song -> DJ ...
		AddSeg(TEXT("Radio.StationID"), 3.f);
		AddSeg(TEXT("Radio.Song"), 180.f);
		AddSeg(TEXT("Radio.DJBanter"), 9.f);
		AddSeg(TEXT("Radio.Ad"), 18.f);
		AddSeg(TEXT("Radio.Song"), 180.f);
		AddSeg(TEXT("Radio.DJBanter"), 9.f);
		// Occasional news break in the loop.
		AddSeg(TEXT("Radio.NewsBreak"), 20.f, /*bInterruptible=*/true);
		break;
	}
}

void UGTAI_RadioSystem::PlayNextSegment()
{
	if (!bPoweredOn || CurrentSchedule.IsEmpty())
	{
		return;
	}

	ScheduleIndex = ScheduleIndex % CurrentSchedule.Num();
	GTAI::Audio::FRadioSegment& Seg = CurrentSchedule[ScheduleIndex];
	SegmentElapsed = 0.f;

	const FGameplayTag& Type = Seg.SegmentType;
	const bool bIsSong     = Type.MatchesTag(FGameplayTag::RequestGameplayTag(FName("Radio.Song")));
	const bool bIsDJ       = Type.MatchesTag(FGameplayTag::RequestGameplayTag(FName("Radio.DJBanter")));
	const bool bIsAd       = Type.MatchesTag(FGameplayTag::RequestGameplayTag(FName("Radio.Ad")));
	const bool bIsNews     = Type.MatchesTag(FGameplayTag::RequestGameplayTag(FName("Radio.NewsBreak")));
	const bool bIsID       = Type.MatchesTag(FGameplayTag::RequestGameplayTag(FName("Radio.StationID")));

	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;

	// Authored audio (if a designer wired a USoundBase into the segment).
	if (Seg.Audio.IsValid())
	{
		if (USoundBase* Sound = Seg.Audio.Get())
		{
			if (World)
			{
				StopActiveRadio();
				UAudioComponent* Comp = UGameplayStatics::PlaySound2D(World, Sound);
				GActiveRadioComponent = Comp;
			}
			return;
		}
	}

	// Generated / placeholder segments route to the appropriate subsystem.
	if (bIsDJ && DJSubsystem)
	{
		// Request banter. The DJ subsystem synthesizes (LLM + TTS) and falls
		// back to pre-authored lines; we listen for completion to (re)play and
		// to correct the segment duration once audio length is known.
		GTAI::Audio::FDLLPromptPack Pack;
		Pack.ShowIdentity = FName(*StationName(ActiveStation));
		Pack.MaxTokens = 120;
		Pack.Temperature = 0.8f;
		if (!Seg.LLMPromptPackKey.IsEmpty())
		{
			Pack.RecentActions.Add(Seg.LLMPromptPackKey);
		}

		// Use the dynamic-delegate wrapper-friendly path. We don't block on the
		// async result — the default duration keeps the loop moving; if audio
		// arrives we play it (it may overlap the next placeholder slightly, which
		// is acceptable for a talk segment).
		DJSubsystem->RequestBanter(ActiveStation, Pack, NAME_None);
	}
	else if (bIsAd && AdSubsystem)
	{
		TArray<FString> Recent;
		if (!Seg.LLMPromptPackKey.IsEmpty())
		{
			Recent.Add(Seg.LLMPromptPackKey);
		}
		AdSubsystem->RequestAd(Recent, [this](USoundBase* AdAudio)
		{
			if (AdAudio)
			{
				if (UWorld* W = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr)
				{
					UGameplayStatics::PlaySound2D(W, AdAudio);
				}
			}
		});
	}
	else if (bIsNews)
	{
		// No dedicated news subsystem; route the headline as a DJ monologue so
		// the voice layer still fires. Falls back to silence (logged) if DJ
		// subsystem is unavailable.
		if (DJSubsystem)
		{
			GTAI::Audio::FDLLPromptPack Pack;
			Pack.ShowIdentity = FName("GTAI_News");
			Pack.MaxTokens = 160;
			Pack.Temperature = 0.5f;
			if (!Seg.LLMPromptPackKey.IsEmpty())
			{
				Pack.RecentActions.Add(Seg.LLMPromptPackKey);
			}
			DJSubsystem->RequestMonologue(ActiveStation, Pack, [](const FString&) {});
		}
		else
		{
			UE_LOG(LogTemp, VeryVerbose,
				TEXT("GTAI_RadioSystem: news break with no voice subsystem (silent placeholder)."));
		}
	}
	else if (bIsID || bIsSong)
	{
		// Station ID / song placeholder: nothing to play yet (no baked assets).
		// The default duration advances the schedule.
	}
}

void UGTAI_RadioSystem::OnSegmentFinished()
{
	// A news takeover consumes exactly one injected break — clear it once the
	// news segment we just finished has elapsed.
	const FGameplayTag FinishedType = CurrentSchedule[ScheduleIndex].SegmentType;
	if (bInNewsTakeover &&
		FinishedType.MatchesTag(FGameplayTag::RequestGameplayTag(FName("Radio.NewsBreak"))))
	{
		bInNewsTakeover = false;
	}

	// Advance to the next segment, rolling around the schedule.
	ScheduleIndex = (ScheduleIndex + 1) % FMath::Max(CurrentSchedule.Num(), 1);
	SegmentElapsed = 0.f;

	PlayNextSegment();
}
