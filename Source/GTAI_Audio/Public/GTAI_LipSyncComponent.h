// GTAI_LipSyncComponent.h
// Drives a speaking actor's facial morph targets from a viseme track produced
// by UGTAI_VoiceSynthesis::GenerateLipSync (UE5.8 Runtime MetaHuman Lip Sync or
// OVRLipSync). For dynamic (runtime TTS) lines the phoneme model runs in parallel
// with ElevenLabs streaming so the mouth moves the instant audio starts.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GTAI_LipSyncComponent.generated.h"

class USkeletalMeshComponent;
class UAssetUserData;

UCLASS(ClassGroup = (GTAI), meta = (BlueprintSpawnableComponent))
class GTAI_AUDIO_API UGTAI_LipSyncComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGTAI_LipSyncComponent();

	// Begin playback of a viseme track mapped to this actor's morph targets.
	UFUNCTION(BlueprintCallable, Category = "GTAI|LipSync")
	void PlayVisemes(UAssetUserData* Track);

	// Stop and reset mouth to neutral.
	UFUNCTION(BlueprintCallable, Category = "GTAI|LipSync")
	void Stop();

	// Morph target names per viseme (ARKit set). Override per character rig.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GTAI|LipSync")
	TMap<FName, FName> VisemeToMorphTarget;

protected:
	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> SkinnedMesh;

	virtual void BeginPlay() override;
	void TickComponent(float DeltaTime, ELevelTick TickType,
	                   FActorComponentTickFunction* ThisTickFunction) override;
};
