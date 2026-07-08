// Copyright GTAI. All Rights Reserved.
// VISTA — Radial weapon-selection menu.
// A UGTAIUserWidget that builds a custom Slate control (SGTAIWeaponWheel) which
// draws N wedges in OnPaint. Selection follows an analog direction (gamepad
// stick or mouse delta from center); the highlighted slot is committed via the
// OnWeaponSelected delegate when the wheel closes. Gameplay is soft-paused by
// the UI activation stack (design doc 2.6 & 6.3). Under namespace GTAI::UI.

#include "GTAIUWWeaponWheel.h"

#include "Engine/Texture2D.h"
#include "Rendering/DrawElements.h"
#include "Widgets/SCompoundWidget.h"

// ───────────────────────── SGTAIWeaponWheel ─────────────────────────
// Custom Slate leaf that performs all wedge drawing. The UMG wrapper owns it,
// feeds it resolved slot data (name + icon brush + weapon slot) and the
// current highlight / reveal progress each frame.
class SGTAIWeaponWheel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SGTAIWeaponWheel) {}
        SLATE_ARGUMENT(float, InnerRadius)
        SLATE_ARGUMENT(float, OuterRadius)
    SLATE_END_ARGS()

    struct FResolvedSlot
    {
        FText Name;
        const FSlateBrush* Brush = nullptr;   // resolved icon, may be null
        int32 WeaponSlot = 0;
    };

    void Construct(const FArguments& InArgs)
    {
        InnerRadius = InArgs._InnerRadius;
        OuterRadius = InArgs._OuterRadius;
        OpenProgress = 0.f;
        HighlightIndex = 0;
    }

    void SetSlots(const TArray<FResolvedSlot>& InSlots)
    {
        Slots = InSlots;
    }

    void SetHighlight(int32 Index) { HighlightIndex = Index; }
    void SetOpenProgress(float P)  { OpenProgress = FMath::Clamp(P, 0.f, 1.f); }

    int32 GetSlotCount() const { return Slots.Num(); }
    int32 GetHighlightIndex() const { return HighlightIndex; }

    virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
                          const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
                          int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
    {
        if (OpenProgress <= KINDA_SMALL_NUMBER || Slots.Num() == 0)
        {
            return LayerId;
        }

        const FVector2D Center = AllottedGeometry.GetLocalSize() * 0.5f;
        const float Inner = InnerRadius * OpenProgress;
        const float Outer = OuterRadius * OpenProgress;
        const int32 N = Slots.Num();
        const float Step = TWO_PI / (float)N;
        // Start at the top (12 o'clock) and go clockwise.
        const float Base = -HALF_PI;

        for (int32 i = 0; i < N; ++i)
        {
            const float A0 = Base + (float)i * Step - Step * 0.5f;
            const float A1 = A0 + Step;
            const bool bHighlight = (i == HighlightIndex);

            const FLinearColor WedgeCol = bHighlight
                ? FLinearColor(0.95f, 0.78f, 0.25f, 0.92f)   // gold highlight
                : FLinearColor(0.08f, 0.12f, 0.18f, 0.72f);  // base wedge

            DrawWedge(OutDrawElements, LayerId, AllottedGeometry, Center, Inner, Outer, A0, A1, WedgeCol);

            // Icon + name at the mid radius of this wedge.
            const float MidA = (A0 + A1) * 0.5f;
            const float MidR = (Inner + Outer) * 0.5f;
            const FVector2D SlotPos = Center + FVector2D(FMath::Cos(MidA), FMath::Sin(MidA)) * MidR;

            const FResolvedSlot& S = Slots[i];
            if (S.Brush)
            {
                const float IconSz = FMath::Min(48.f, (Outer - Inner) * 0.5f);
                FSlateDrawElement::MakeBox(
                    OutDrawElements, LayerId + 1,
                    AllottedGeometry.ToPaintGeometry(SlotPos - FVector2D(IconSz, IconSz) * 0.5f, FVector2D(IconSz, IconSz)),
                    S.Brush, ESlateDrawEffect::None, FLinearColor::White);
            }

            if (!S.Name.IsEmpty())
            {
                const FVector2D TextPos = SlotPos + FVector2D(0.f, (Outer - Inner) * 0.30f);
                FSlateDrawElement::MakeText(
                    OutDrawElements, LayerId + 2,
                    AllottedGeometry.ToPaintGeometry(TextPos, FVector2D(120.f, 24.f)),
                    S.Name,
                    FCoreStyle::GetDefaultFontStyle("Regular", 11),
                    bHighlight ? FLinearColor::White : FLinearColor(0.8f, 0.85f, 0.9f, 0.85f));
            }
        }

        return LayerId + 3;
    }

private:
    static void DrawWedge(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo,
                          const FVector2D& C, float Inner, float Outer, float A0, float A1, const FLinearColor& Col)
    {
        const int32 ArcSegs = 12;
        TArray<FSlateVertex> V;
        TArray<SlateIndex> Idx;

        // Center vertex (inner hub point).
        V.Add(FSlateVertex::Make<C>(C, Col));

        for (int32 s = 0; s <= ArcSegs; ++s)
        {
            const float A = FMath::Lerp(A0, A1, (float)s / (float)ArcSegs);
            const FVector2D InnerP = C + FVector2D(FMath::Cos(A), FMath::Sin(A)) * Inner;
            const FVector2D OuterP = C + FVector2D(FMath::Cos(A), FMath::Sin(A)) * Outer;
            V.Add(FSlateVertex::Make<C>(InnerP, Col));
            V.Add(FSlateVertex::Make<C>(OuterP, Col));
        }

        // Triangle fan: center, then inner/outer pairs around the arc.
        for (int32 s = 0; s < ArcSegs; ++s)
        {
            const SlateIndex InnerA = (SlateIndex)(1 + s * 2);
            const SlateIndex OuterA = (SlateIndex)(2 + s * 2);
            const SlateIndex InnerB = (SlateIndex)(3 + s * 2);
            const SlateIndex OuterB = (SlateIndex)(4 + s * 2);
            Idx.Append({ 0, InnerA, OuterA });
            Idx.Append({ OuterA, InnerB, OuterB });
        }

        FSlateDrawElement::MakeTriangles(
            Out, Layer, Geo.ToPaintGeometry(), V, Idx, nullptr, ESlateDrawEffect::None);
    }

    float InnerRadius = 60.f;
    float OuterRadius = 220.f;
    float OpenProgress = 0.f;
    int32 HighlightIndex = 0;
    TArray<FResolvedSlot> Slots;
};

// ───────────────────────── UGTAIUWWeaponWheel ─────────────────────────
namespace
{
    float AngleToStep(float AngleRad, int32 Count)
    {
        if (Count <= 0)
        {
            return 0.f;
        }
        // Normalize to [-PI, PI] then map so slot 0 sits at the top.
        float A = FMath::Fmod(AngleRad + HALF_PI, TWO_PI);
        if (A < 0.f) A += TWO_PI;
        const float Step = TWO_PI / (float)Count;
        return FMath::FloorToFloat(A / Step + 0.5f) - (float)Count * 0.5f;
    }
}

TSharedRef<SWidget> UGTAIUWWeaponWheel::RebuildWidget()
{
    MyWheel = SNew(SGTAIWeaponWheel)
        .InnerRadius(70.f)
        .OuterRadius(240.f);

    return MyWheel.ToSharedRef();
}

void UGTAIUWWeaponWheel::NativeConstruct()
{
    Super::NativeConstruct();

    // Resolve soft icon references once so the Slate widget can draw them.
    if (MyWheel)
    {
        TArray<SGTAIWeaponWheel::FResolvedSlot> Resolved;
        Resolved.Reserve(Slots.Num());
        for (const FGTAIWeaponWheelSlot& Slot : Slots)
        {
            SGTAIWeaponWheel::FResolvedSlot R;
            R.Name = Slot.Name;
            R.WeaponSlot = Slot.WeaponSlot;
            if (UTexture2D* Tex = Slot.Icon.LoadSynchronous())
            {
                FSlateBrush Brush;
                Brush.SetResourceObject(Tex);
                Brush.ImageSize = FVector2D(Tex->GetSizeX(), Tex->GetSizeY());
                ResolvedBrushes.Add(Brush);
                R.Brush = &ResolvedBrushes.Last();
            }
            Resolved.Add(R);
        }
        MyWheel->SetSlots(Resolved);
        MyWheel->SetHighlight(HighlightedIndex);
    }
}

void UGTAIUWWeaponWheel::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    if (!MyWheel)
    {
        return;
    }

    // Ease the reveal progress toward its target each frame.
    const float Target = bIsOpen ? 1.f : 0.f;
    float P = MyWheel->GetOpenProgress();
    P = FMath::FInterpTo(P, Target, InDeltaTime, 12.f);
    MyWheel->SetOpenProgress(P);

    if (!bIsOpen && P <= KINDA_SMALL_NUMBER && IsInViewport())
    {
        // Fully closed: drop from the viewport so it stops ticking/drawing.
        RemoveFromParent();
    }
}

void UGTAIUWWeaponWheel::Open()
{
    bIsOpen = true;
    if (!IsInViewport())
    {
        AddToViewport(55);
    }
}

void UGTAIUWWeaponWheel::Close()
{
    bIsOpen = false;
    if (Slots.IsValidIndex(HighlightedIndex))
    {
        OnWeaponSelected.Broadcast(Slots[HighlightedIndex].WeaponSlot);
    }
}

void UGTAIUWWeaponWheel::SetSelectionDirection(const FVector2D& Dir)
{
    if (!MyWheel || Slots.Num() == 0)
    {
        return;
    }

    const float Len = Dir.Size();
    if (Len < KINDA_SMALL_NUMBER)
    {
        return; // dead-zone: keep current highlight
    }

    const float Angle = FMath::Atan2(Dir.Y, Dir.X);
    const int32 Count = Slots.Num();
    const float Step = TWO_PI / (float)Count;
    // Slot 0 centered at top; convert direction angle to a slot index.
    float Rel = FMath::Fmod(Angle + HALF_PI, TWO_PI);
    if (Rel < 0.f) Rel += TWO_PI;
    int32 Idx = FMath::RoundToInt(Rel / Step) % Count;

    if (Idx != HighlightedIndex)
    {
        HighlightedIndex = Idx;
        MyWheel->SetHighlight(HighlightedIndex);
        PlayFocusSound();
    }
}
