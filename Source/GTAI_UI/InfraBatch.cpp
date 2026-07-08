// Copyright GTAI. All Rights Reserved.
// VISTA — UI Infrastructure stubs: AppBase, Button, InputRouter, UserWidget, ViewModels, DPIManager

#include "GTAIUWAppBase.h"
void UGTAIUWAppBase::SetAppTitle(const FString& Title) { /* TODO */ }

#include "GTAIButton.h"
void UGTAIButton::OnPressed() { OnClicked.Broadcast(); }

#include "GTAIInputRouter.h"
bool UGTAIInputRouter::RouteInput(const FKey& Key) { return false; }

#include "GTAIUserWidget.h"
void UGTAIUserWidget::NativeTick(const FGeometry& MyGeometry, float DeltaTime) { Super::NativeTick(MyGeometry, DeltaTime); }

#include "GTAIViewModel_Map.h"
void UGTAIViewModel_Map::UpdateMap() { /* TODO */ }

#include "GTAIViewModel_World.h"
void UGTAIViewModel_World::UpdateWorldState() { /* TODO */ }

#include "GTAI_DPIManager.h"
float UGTAI_DPIManager::GetDPIScale() const { return 1.f; }
