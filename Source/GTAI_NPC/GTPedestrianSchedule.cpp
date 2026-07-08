// Copyright GTAI. All Rights Reserved.
// LUMEN — Pedestrian Schedule System (100+ peds, round-robin chunked)

#include "GTPedestrianSchedule.h"
#include "GTPedestrianTypes.h"
#include "GTNPCDefines.h"

void UGTPedestrianSchedule::Initialize(int32 MaxPeds)
{
    Schedules.Reserve(MaxPeds);
    for (int32 i = 0; i < MaxPeds; ++i)
    {
        Schedules.Add(GenerateRandomSchedule(i));
    }
}

void UGTPedestrianSchedule::TickSchedules(float DeltaTime, int32 ChunkSize)
{
    // Round-robin processing: only process ChunkSize peds per frame
    int32 EndIndex = FMath::Min(CurrentChunkOffset + ChunkSize, Schedules.Num());

    for (int32 i = CurrentChunkOffset; i < EndIndex; ++i)
    {
        TickSinglePed(i, DeltaTime);
    }

    CurrentChunkOffset = (CurrentChunkOffset + ChunkSize) % Schedules.Num();
}

void UGTPedestrianSchedule::TickSinglePed(int32 Index, float DeltaTime)
{
    if (!Schedules.IsValidIndex(Index)) return;
    FPedestrianSchedule& Sched = Schedules[Index];

    // Check if current activity is complete
    if (Sched.bAtDestination)
    {
        AdvanceToNextActivity(Sched);
    }

    // Move toward destination
    // TODO: Integrate with MassEntity movement system
}

void UGTPedestrianSchedule::AdvanceToNextActivity(FPedestrianSchedule& Sched)
{
    // Cycle through daily routine
    Sched.CurrentActivityIndex = (Sched.CurrentActivityIndex + 1) % Sched.Activities.Num();
    Sched.bAtDestination = false;

    if (Sched.Activities.IsValidIndex(Sched.CurrentActivityIndex))
    {
        const FPedestrianActivity& Activity = Sched.Activities[Sched.CurrentActivityIndex];
        Sched.CurrentDestination = Activity.Location;
        Sched.CurrentActivity = Activity.Type;
    }
}

FPedestrianSchedule UGTPedestrianSchedule::GenerateRandomSchedule(int32 Seed)
{
    FPedestrianSchedule Sched;
    Sched.NPCId = Seed;

    // Generate daily routine: home → work → lunch → work → home → leisure → home
    // Times are relative to day cycle [0.0 = midnight, 1.0 = 11:59pm]

    // 7:00 AM — Wake at home
    {
        FPedestrianActivity Wake;
        Wake.Location = FVector(FMath::RandRange(-5000.f, 5000.f), FMath::RandRange(-5000.f, 5000.f), 0.f);
        Wake.Type = EPedestrianActivity::Idle;
        Wake.StartTime = 0.29f; // 7:00 AM
        Wake.Duration = 0.02f;  // ~30 min
        Sched.Activities.Add(Wake);
    }

    // 7:30 AM — Commute to work
    {
        FPedestrianActivity Commute;
        Commute.Location = FVector(FMath::RandRange(-3000.f, 3000.f), FMath::RandRange(-3000.f, 3000.f), 0.f);
        Commute.Type = EPedestrianActivity::Walking;
        Commute.StartTime = 0.31f;
        Commute.Duration = 0.04f;
        Sched.Activities.Add(Commute);
    }

    // 8:30 AM — Work (stay at workplace)
    {
        FPedestrianActivity Work;
        Work.Location = Commute.Location;
        Work.Type = EPedestrianActivity::Working;
        Work.StartTime = 0.35f;
        Work.Duration = 0.16f; // 3.8 hours
        Sched.Activities.Add(Work);
    }

    // 12:30 PM — Lunch walk
    {
        FPedestrianActivity Lunch;
        Lunch.Location = Work.Location + FVector(FMath::RandRange(-500.f, 500.f), FMath::RandRange(-500.f, 500.f), 0.f);
        Lunch.Type = EPedestrianActivity::Walking;
        Lunch.StartTime = 0.51f;
        Lunch.Duration = 0.03f;
        Sched.Activities.Add(Lunch);
    }

    // 1:00 PM — Back to work
    {
        FPedestrianActivity Work2;
        Work2.Location = Commute.Location;
        Work2.Type = EPedestrianActivity::Working;
        Work2.StartTime = 0.54f;
        Work2.Duration = 0.17f;
        Sched.Activities.Add(Work2);
    }

    // 5:00 PM — Commute home
    {
        FPedestrianActivity GoHome;
        GoHome.Location = Wake.Location;
        GoHome.Type = EPedestrianActivity::Walking;
        GoHome.StartTime = 0.71f;
        GoHome.Duration = 0.04f;
        Sched.Activities.Add(GoHome);
    }

    // Evening — Leisure or shopping
    if (FMath::FRand() > 0.3f)
    {
        FPedestrianActivity Leisure;
        Leisure.Location = FVector(FMath::RandRange(-3000.f, 3000.f), FMath::RandRange(-3000.f, 3000.f), 0.f);
        Leisure.Type = EPedestrianActivity::Leisure;
        Leisure.StartTime = 0.77f;
        Leisure.Duration = 0.08f;
        Sched.Activities.Add(Leisure);
    }

    // Night — Sleep
    {
        FPedestrianActivity Sleep;
        Sleep.Location = Wake.Location;
        Sleep.Type = EPedestrianActivity::Idle;
        Sleep.StartTime = 0.88f;
        Sleep.Duration = 0.12f;
        Sched.Activities.Add(Sleep);
    }

    return Sched;
}
