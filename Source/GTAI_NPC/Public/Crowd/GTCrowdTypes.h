// GTCrowdTypes.h
// Plain types for the crowd behavior system (panic/curiosity/mob on a grid field).
#pragma once

#include "CoreMinimal.h"
#include "NPC/GTNPCDefines.h"

namespace GTAI::NPC
{
	// Coarse 2D scalar field sampled by pedestrians (cell ~5m).
	struct FCrowdFieldCell
	{
		float Panic = 0.f;      // 0..1, propagates (Helbing-style)
		float Curiosity = 0.f;  // 0..1, attractor strength
		float Anger = 0.f;      // 0..1, mob cohesion
	};

	struct FCrowdField
	{
		int32 GridW = 0, GridH = 0;
		float CellSize = 500.f;  // cm
		FVector Origin = FVector::ZeroVector;
		TArray<FCrowdFieldCell> Cells;  // GridW*GridH

		int32 Index(int32 X, int32 Y) const { return Y * GridW + X; }
		FCrowdFieldCell& CellAt(int32 X, int32 Y) { return Cells[Index(X, Y)]; }
		FVector CellCenter(int32 X, int32 Y) const { return Origin + FVector((X + 0.5f) * CellSize, (Y + 0.5f) * CellSize, 0.f); }
		void WorldToCell(const FVector& World, int32& X, int32& Y) const;
	};

	// A transient attractor (point of interest) or repulsor (panic source).
	struct FCrowdSource
	{
		FVector Location = FVector::ZeroVector;
		float Radius = 1000.f;   // cm
		float Strength = 1.f;    // +attract (curiosity/anger), -repel (panic)
		ECrowdSourceKind Kind = ECrowdSourceKind::Curiosity;
		float TTL = 0.f;         // seconds; <=0 means persistent
	};

	enum class ECrowdSourceKind : uint8
	{
		Curiosity,   // attract
		Panic,       // repel + raises panic field
		Anger        // attract + raises anger field (mob seed)
	};
}
