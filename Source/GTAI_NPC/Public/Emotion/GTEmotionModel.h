// GTEmotionModel.h
// Per-NPC affective state: affinity, trust, fear, respect, anger, mood.
// Drives dialogue tone, crowd susceptibility, and memory salience.
#pragma once

#include "CoreMinimal.h"
#include "NPC/GTNPCDefines.h"

namespace GTAI::NPC
{
	// A discrete event that shifts affective state.
	struct FEmotionEvent
	{
		float dAffinity = 0.f;
		float dTrust = 0.f;
		float dFear = 0.f;
		float dRespect = 0.f;
		float dAnger = 0.f;
		float dMood = 0.f;
		FString Cause;                 // debug/logging
	};

	class GTAI_NPC_API FEmotionModel
	{
	public:
		FEmotionModel() = default;

		void ApplyEvent(const FEmotionEvent& Ev);

		// Per city-hour decay toward baselines.
		void TickDecay(float CityHoursDelta);

		// Crowd field contribution (see GTCrowdBehavior): raises panic/fear.
		void AbsorbCrowdPanic(float PanicLevel, float Delta);

		// Accessors (clamped).
		float GetAffinity() const { return Affinity; }
		float GetTrust()   const { return Trust; }
		float GetFear()    const { return Fear; }
		float GetRespect() const { return Respect; }
		float GetAnger()   const { return Anger; }
		float GetMood()    const { return Mood; }

		// 0..1 panic susceptibility used by crowd system.
		float PanicSusceptibility() const { return FMath::Clamp(Fear / 100.f, 0.f, 1.f); }

		void Serialize(FArchive& Ar);

	private:
		float Affinity = 0.f;    // -100..100
		float Trust    = 50.f;   // 0..100
		float Fear     = 0.f;    // 0..100
		float Respect  = 0.f;    // -100..100
		float Anger    = 0.f;    // 0..100
		float Mood     = 0.f;    // -1..1 (transient)

		// Baselines the scalars drift toward.
		float AffinityBase = 0.f, TrustBase = 50.f, FearBase = 0.f;
		float RespectBase = 0.f, AngerBase = 0.f, MoodBase = 0.f;
	};
}
