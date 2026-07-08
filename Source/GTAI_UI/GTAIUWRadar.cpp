// Copyright GTAI. All Rights Reserved.
// VISTA — GTA V-style circular radar minimap.
// A UMG widget that builds a custom Slate control (SGTAIRadar) which draws the
// circular map in OnPaint: a circular clip mask, a rotating/north-up world, a
// sweep arc, and blips projected from UGTAIViewModel_World (with off-screen
// entities clamped to edge arrows). Under namespace GTAI::UI.

#include "GTAIUWRadar.h"

#include "Rendering/DrawElements.h"
#include "Widgets/SCompoundWidget.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

#include "GTAIViewModel_World.h"
#include "GTAIViewModel_Map.h"

// ───────────────────────── SGTAIRadar ─────────────────────────
// Custom Slate leaf that performs all radar drawing. The UMG wrapper owns it
// and feeds transformed blip data each tick.
class SGTAIRadar : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SGTAIRadar) {}
        SLATE_ARGUMENT(float, Radius)
        SLATE_ARGUMENT(float, SweepPeriod)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs)
    {
        Radius = InArgs._Radius;
        SweepPeriod = InArgs._SweepPeriod;
        SweepAngle = 0.f;
    }

    void SetRadius(float InRadius) { Radius = InRadius; }
    void SetSweepPeriod(float InPeriod) { SweepPeriod = InPeriod; }

    /** Per-tick data from the UMG owner. */
    struct FBlip
    {
        FVector2D ScreenPos;   // already rotated/projected, relative to center
        FLinearColor Color;
        float Scale;
        bool bOffScreen;
        FVector2D EdgePos;     // valid when bOffScreen
        bool bIsPlayer;
    };

    void SetFrameData(float InHeadingRad, float InRange, bool bNorthUp,
                      const TArray<FBlip>& InBlips, float InSweepAngle)
    {
        HeadingRad = InHeadingRad;
        Range = InRange;
        bNorthUp = bNorthUp;
        Blips = InBlips;
        SweepAngle = InSweepAngle;
    }

    virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
                          const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
                          int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
    {
        const FVector2D Center = AllottedGeometry.GetLocalSize() * 0.5f;
        const float R = FMath::Min(Radius, Center.X);

        // Build the circular clip mask so nothing draws outside the disc.
        TArray<FVector2D> ClipRing;
        const int32 Segments = 48;
        for (int32 i = 0; i < Segments; ++i)
        {
            const float A = (float)i / (float)Segments * TWO_PI;
            ClipRing.Add(Center + FVector2D(FMath::Cos(A), FMath::Sin(A)) * R);
        }
        FSlateClippingManager& Clip = OutDrawElements.GetClippingManager();
        const TOptional<FShortRect> ClipBounds = AllottedGeometry.GetClippingRect();
        Clip.PushClippingState(FSlateClippingState(ClipRing, ClipBounds));

        // --- Base disc ---
        FSlateDrawElement::MakeBox(
            OutDrawElements, LayerId,
            AllottedGeometry.ToPaintGeometry(Center - FVector2D(R, R), FVector2D(R * 2.f, R * 2.f)),
            &FSlateBrush::GetDefault(),
            ESlateDrawEffect::None,
            FLinearColor(0.04f, 0.08f, 0.12f, 0.78f));

        // --- Range rings ---
        const FLinearColor RingColor(0.25f, 0.55f, 0.75f, 0.35f);
        for (int32 k = 1; k <= 2; ++k)
        {
            const float rr = R * (float)k / 3.f;
            DrawCircleOutline(OutDrawElements, LayerId, AllottedGeometry, Center, rr, RingColor, 40);
        }

        // --- Crosshair ---
        const FLinearColor CrossColor(0.25f, 0.55f, 0.75f, 0.3f);
        DrawLine(OutDrawElements, LayerId, AllottedGeometry, Center - FVector2D(R, 0.f), Center + FVector2D(R, 0.f), CrossColor);
        DrawLine(OutDrawElements, LayerId, AllottedGeometry, Center - FVector2D(0.f, R), Center + FVector2D(0.f, R), CrossColor);

        // --- Sweep arc (rotating) ---
        if (SweepPeriod > KINDA_SMALL_NUMBER)
        {
            const float HalfArc = 0.5f; // radians of trailing glow
            const int32 Steps = 24;
            for (int32 s = 0; s < Steps; ++s)
            {
                const float t0 = SweepAngle - HalfArc * (float)s / (float)Steps;
                const float t1 = SweepAngle - HalfArc * (float)(s + 1) / (float)Steps;
                const float Alpha = 1.f - (float)s / (float)Steps;
                DrawLine(OutDrawElements, LayerId, AllottedGeometry,
                         Center + FVector2D(FMath::Cos(t0), FMath::Sin(t0)) * R,
                         Center + FVector2D(FMath::Cos(t1), FMath::Sin(t1)) * R,
                         FLinearColor(0.2f, 0.9f, 1.f, 0.5f * Alpha));
            }
        }

        // --- Blips ---
        for (const FBlip& B : Blips)
        {
            if (B.bOffScreen)
            {
                // Edge arrow pointing toward the off-screen entity.
                const FVector2D Dir = B.EdgePos.GetSafeNormal();
                const FVector2D Tip = Center + Dir * (R - 6.f);
                const FVector2D Wing = FVector2D(-Dir.Y, Dir.X);
                const FVector2D P1 = Tip - Dir * 8.f + Wing * 5.f;
                const FVector2D P2 = Tip - Dir * 8.f - Wing * 5.f;
                DrawLine(OutDrawElements, LayerId, AllottedGeometry, Tip, P1, B.Color);
                DrawLine(OutDrawElements, LayerId, AllottedGeometry, Tip, P2, B.Color);
            }
            else
            {
                const float Sz = FMath::Max(3.f, 6.f * B.Scale);
                const FVector2D Pos = Center + B.ScreenPos;
                FSlateDrawElement::MakeBox(
                    OutDrawElements, LayerId,
                    AllottedGeometry.ToPaintGeometry(Pos - FVector2D(Sz, Sz), FVector2D(Sz * 2.f, Sz * 2.f)),
                    &FSlateBrush::GetDefault(),
                    ESlateDrawEffect::None,
                    B.Color);

                if (B.bIsPlayer)
                {
                    // Player heading triangle.
                    const FVector2D Dir = FVector2D(FMath::Cos(HeadingRad), FMath::Sin(HeadingRad));
                    const FVector2D Tip = Pos + Dir * Sz;
                    const FVector2D Wing = FVector2D(-Dir.Y, Dir.X);
                    DrawLine(OutDrawElements, LayerId, AllottedGeometry, Tip, Pos - Dir * Sz + Wing * Sz, FLinearColor::White);
                    DrawLine(OutDrawElements, LayerId, AllottedGeometry, Tip, Pos - Dir * Sz - Wing * Sz, FLinearColor::White);
                }
            }
        }

        Clip.PopClippingState();
        return LayerId + 1;
    }

private:
    static void DrawLine(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo,
                         const FVector2D& A, const FVector2D& B, const FLinearColor& C)
    {
        TArray<FSlateVertex> V;
        TArray<SlateIndex> Idx;
        FSlateDrawElement::MakeLines(Out, Layer, Geo.ToPaintGeometry(), { A, B },
                                     C, ESlateLineBatchType::OnDemandAntiAliased, 1.5f);
    }

    static void DrawCircleOutline(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo,
                                  const FVector2D& C, float R, const FLinearColor& Col, int32 Segs)
    {
        TArray<FVector2D> Pts;
        for (int32 i = 0; i <= Segs; ++i)
        {
            const float A = (float)i / (float)Segs * TWO_PI;
            Pts.Add(C + FVector2D(FMath::Cos(A), FMath::Sin(A)) * R);
        }
        FSlateDrawElement::MakeLines(Out, Layer, Geo.ToPaintGeometry(), Pts,
                                     Col, ESlateLineBatchType::OnDemandAntiAliased, 1.0f);
    }

    float Radius = 160.f;
    float SweepPeriod = 4.f;
    float SweepAngle = 0.f;
    float HeadingRad = 0.f;
    float Range = 30000.f;
    bool bNorthUp = false;
    TArray<FBlip> Blips;
};

// ───────────────────────── UGTAIUWRadar ─────────────────────────
TSharedRef<SWidget> UGTAIUWRadar::RebuildWidget()
{
    MyRadar = SNew(SGTAIRadar)
        .Radius(RadarRadius)
        .SweepPeriod(SweepPeriod);

    return MyRadar.ToSharedRef();
}

void UGTAIUWRadar::NativeConstruct()
{
    Super::NativeConstruct();
    if (MyRadar)
    {
        MyRadar->SetRadius(RadarRadius);
        MyRadar->SetSweepPeriod(SweepPeriod);
    }
}

void UGTAIUWRadar::BindViewModels(UGTAIViewModel_World* InWorldVM, UGTAIViewModel_Map* InMapVM)
{
    WorldVM = InWorldVM;
    MapVM = InMapVM;
}

void UGTAIUWRadar::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    AccumulatedTime += InDeltaTime;
    if (SweepPeriod > KINDA_SMALL_NUMBER)
    {
        SweepAngle += (TWO_PI / SweepPeriod) * InDeltaTime;
        if (SweepAngle > TWO_PI) SweepAngle -= TWO_PI;
    }

    if (!MyRadar || !WorldVM)
    {
        return;
    }

    // Throttle blip recompute to UpdateHz (per-instance accumulator).
    const float Interval = UpdateHz > KINDA_SMALL_NUMBER ? 1.f / UpdateHz : 0.f;
    BlipAccumulator += InDeltaTime;
    if (BlipAccumulator < Interval)
    {
        return;
    }
    BlipAccumulator = 0.f;

    // Resolve player transform for relative projection.
    APlayerController* PC = GetOwningPlayer();
    APawn* Pawn = PC ? PC->GetPawn() : nullptr;
    if (!Pawn)
    {
        return;
    }

    const FVector PlayerLoc = Pawn->GetActorLocation();
    const float HeadingDeg = MapVM ? MapVM->GetPlayerHeadingDeg()
                                   : Pawn->GetActorRotation().Yaw;
    const float HeadingRad = FMath::DegreesToRadians(HeadingDeg);
    const float Range = MapVM ? MapVM->GetRadarRange() : 30000.f;
    const bool bNorthUp = MapVM && MapVM->GetRadarMode() == EGTAIRadarMode::NorthUp;

    SGTAIRadar::FBlip PlayerBlip;
    PlayerBlip.bIsPlayer = true;
    PlayerBlip.ScreenPos = FVector2D::ZeroVector;
    PlayerBlip.Color = FLinearColor::White;
    PlayerBlip.Scale = 1.2f;

    TArray<SGTAIRadar::FBlip> Out;
    Out.Add(PlayerBlip);

    const TArray<FGTAIWorldBlip>& Blips = WorldVM->GetBlips();
    for (const FGTAIWorldBlip& B : Blips)
    {
        // World X/Y delta relative to player (Unreal: X forward, Y right).
        const FVector2D Delta(B.WorldLocation.X - PlayerLoc.X,
                              B.WorldLocation.Y - PlayerLoc.Y);

        // Rotate so player's forward is "up" on the disc (GTA V style).
        const float CS = FMath::Cos(-HeadingRad);
        const float SN = FMath::Sin(-HeadingRad);
        FVector2D Local(Delta.X * CS - Delta.Y * SN,
                        Delta.X * SN + Delta.Y * CS);

        if (!bNorthUp)
        {
            // Rotated mode: world rotates, player fixed pointing up.
        }
        else
        {
            // North-up: negate rotation so map north is up.
            Local = FVector2D(Delta.X, Delta.Y);
        }

        // Project world units -> radar radius.
        const float Scale = RadarRadius / FMath::Max(Range, KINDA_SMALL_NUMBER);
        FVector2D Screen = Local * Scale;

        SGTAIRadar::FBlip OutB;
        OutB.bIsPlayer = false;
        OutB.Color = BlipColor(B.Type);
        OutB.Scale = 1.f;

        const float MaxR = RadarRadius - 6.f;
        if (Screen.SizeSquared() > MaxR * MaxR)
        {
            OutB.bOffScreen = true;
            OutB.EdgePos = Screen.GetSafeNormal() * MaxR;
            OutB.ScreenPos = OutB.EdgePos;
        }
        else
        {
            OutB.bOffScreen = false;
            OutB.ScreenPos = Screen;
        }
        Out.Add(OutB);
    }

    MyRadar->SetFrameData(HeadingRad, Range, bNorthUp, Out, SweepAngle);
}

FLinearColor UGTAIUWRadar::BlipColor(EGTAIBlipType Type) const
{
    switch (Type)
    {
    case EGTAIBlipType::Player:        return FLinearColor::White;
    case EGTAIBlipType::Mission:       return FLinearColor(0.2f, 0.9f, 0.3f);   // green
    case EGTAIBlipType::MissionTarget: return FLinearColor(1.f, 0.85f, 0.1f);   // gold
    case EGTAIBlipType::Shop:          return FLinearColor(0.3f, 0.7f, 1.f);    // blue
    case EGTAIBlipType::WeaponShop:    return FLinearColor(1.f, 0.4f, 0.2f);    // orange
    case EGTAIBlipType::ClothingShop:  return FLinearColor(0.8f, 0.5f, 1.f);    // purple
    case EGTAIBlipType::Police:        return FLinearColor(0.3f, 0.5f, 1.f);    // blue
    case EGTAIBlipType::Enemy:         return FLinearColor(1.f, 0.2f, 0.2f);    // red
    case EGTAIBlipType::Friend:        return FLinearColor(0.4f, 1.f, 0.5f);    // green
    case EGTAIBlipType::Vehicle:       return FLinearColor(0.8f, 0.8f, 0.9f);    // grey
    case EGTAIBlipType::Property:      return FLinearColor(1.f, 0.8f, 0.3f);    // tan
    case EGTAIBlipType::Collectible:   return FLinearColor(1.f, 1.f, 0.4f);     // yellow
    case EGTAIBlipType::Custom:        return FLinearColor(1.f, 1.f, 1.f);
    default:                           return FLinearColor::White;
    }
}
