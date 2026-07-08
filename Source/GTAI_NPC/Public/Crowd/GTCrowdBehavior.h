// GTCrowdBehavior.h
// Emotional-contagion crowd layer over UE5 Mass Avoidance.
// Implements: avoidance (via MassAvoidance), curiosity, panic propagation,
// mob formation. Updates a coarse field + per-entity crowd flags.
#pragma once

#include "CoreMinimal.h"
#include "NPC/GTNPCDefines.h"
#include "Crowd/GTCrowdTypes.h"
#include "Schedule/GTPedestrianTypes.h"
#include "Emotion/GTEmotionModel.h"

namespace GTAI::NPC
{
	class GTAI_NPC_API FCrowdBehaviorSystem
	{
	public:
		FCrowdBehaviorSystem() = default;

		void InitField(int32 GridW, int32 GridH, float CellSize, const FVector& Origin);

		// Add/remove transient sources (accident, gunshot, protest).
		void AddSource(const FCrowdSource& Src);
		void RemoveSource(int32 Index);

		// Per crowd tick (every ~0.5s game-time):
		//  1) decay + diffuse the field,
		//  2) sample field per pedestrian, update Fragments + EmotionModels.
		void Tick(float DeltaSeconds,
		          TMap<FNPCId, FPedestrianFragment>& Fragments,
		          TMap<FNPCId, FEmotionModel>* Emotions = nullptr);

		const FCrowdField& GetField() const { return Field; }

		// Tunables.
		float PanicPropagateK = 0.35f;   // neighbor influence weight
		float PanicCalmRate  = 0.05f;    // per-second decay
		float MobDensityThreshold = 0.6f;// normalized local density for mob
		float CuriosityRadius = 1200.f;  // cm

	private:
		void DiffuseField(float DeltaSeconds);
		void SamplePedestrian(FNPCId NPC, FPedestrianFragment& Frag, FEmotionModel* Emo);
		int32 WorldCellIndex(const FVector& World) const;

		FCrowdField Field;
		TArray<FCrowdSource> Sources;
	};
}
