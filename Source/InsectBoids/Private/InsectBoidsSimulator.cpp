// Fill out your copyright notice in the Description page of Project Settings.


#include "InsectBoidsSimulator.h"

FInsectBoidsSimulator::FInsectBoidsSimulator(const FInsectBoidsConfig& InConfig, const FVector& ParticleCenter)
	: Config(InConfig)
{
	CurrentContext = 0;
	Particles[0].AddDefaulted(Config.NumParticles);
	Particles[1].AddDefaulted(Config.NumParticles);
	SimulationTasks.Reserve((Config.NumParticles / NumParticlesPerTask) + 1);

	for (int32 i = 0; i < GetWriteParticles().Num(); ++i)
	{
		FInsectBoidsParticle& CurrentParticle = GetWriteParticles()[i];
		CurrentParticle.Position = FMath::RandPointInBox(FBox(ParticleCenter - FVector(32), ParticleCenter + FVector(32)));
		CurrentParticle.Velocity = FVector(0.f);
	}
}

void FInsectBoidsSimulator::FlipParticleContext()
{
	check(IsInGameThread());

	// Outstanding tasks could be reading CurrentContext from another thread,
	// so make sure they're done
	check(SimulationTasks.IsEmpty());

	CurrentContext ^= 1;
}

void FInsectBoidsSimulator::IterateOverNeighbors(int32 ParticleIndex, TFunction<void(int32)> Function)
{
	const FVector ParticleGridIndex = GetReadParticles()[ParticleIndex].Position / GridSize;
	FIntVector ParticleIntIndex;
	ParticleIntIndex.X = FMath::FloorToInt(ParticleGridIndex.X);
	ParticleIntIndex.Y = FMath::FloorToInt(ParticleGridIndex.Y);
	ParticleIntIndex.Z = FMath::FloorToInt(ParticleGridIndex.Z);

	for (int32 x = -1; x <= 1; ++x)
	{
		for (int32 y = -1; y <= 1; ++y)
		{
			for (int32 z = -1; z <= 1; ++z)
			{
				if (auto* NeighborArray = SpatialHashGrid.Find(ParticleIntIndex + FIntVector(x, y, z)))
				{
					for (int32 NeighborIndex : *NeighborArray)
					{
						if (NeighborIndex == ParticleIndex)
							continue;

						Function(NeighborIndex);
					}
				}
			}
		}
	}
}

void FInsectBoidsSimulator::Simulate(float DeltaTime, const FInsectBoidsConfig& UpdatedConfig)
{
	SCOPED_NAMED_EVENT_TEXT("FInsectBoidsSimulator::Simulate", FColor::Magenta);

	auto SharedThis = this->AsShared();

	// Wait for the last frame to catch up, if needed.
	// Once the simulation is done, flip the context so we read from last frame's results
	UE::Tasks::Wait(SimulationTasks);
	SimulationTasks.Empty();

	FlipParticleContext();

	Config = UpdatedConfig;

	// Setup the neighbor grid
	UE::Tasks::FTask SetupNeighborGridTask = UE::Tasks::Launch(UE_SOURCE_LOCATION, [SharedThis] {
		SharedThis->SetupNeighborGrid();
	});
	SimulationTasks.Add(SetupNeighborGridTask);

	// Kick off particle simulation tasks
	const int32 NumParticles = GetReadParticles().Num();
	for (int32 i = 0; i < (NumParticles / NumParticlesPerTask + (NumParticles % NumParticlesPerTask != 0)); ++i)
	{
		UE::Tasks::FTask ParticleTask = UE::Tasks::Launch(UE_SOURCE_LOCATION, [SharedThis, ParticleStartIndex = i * NumParticlesPerTask, DeltaTime] {
			SharedThis->SimulateParticles(ParticleStartIndex, ParticleStartIndex + SharedThis->NumParticlesPerTask, DeltaTime);
		}, SetupNeighborGridTask);
		SimulationTasks.Add(ParticleTask);
	}
}

void FInsectBoidsSimulator::SetupNeighborGrid()
{
	SCOPED_NAMED_EVENT_TEXT("FInsectBoidsSimulator::SetupNeighborGrid", FColor::Magenta);

	SpatialHashGrid.Empty();

	for (int32 i = 0; i < GetReadParticles().Num(); ++i)
	{
		const FVector ParticleIndex = GetReadParticles()[i].Position / GridSize;
		FIntVector ParticleIntIndex;
		ParticleIntIndex.X = FMath::FloorToInt(ParticleIndex.X);
		ParticleIntIndex.Y = FMath::FloorToInt(ParticleIndex.Y);
		ParticleIntIndex.Z = FMath::FloorToInt(ParticleIndex.Z);

		auto& NeighborArray = SpatialHashGrid.FindOrAdd(ParticleIntIndex);
		if (NeighborArray.Num() < NeighborGridSize)
		{
			NeighborArray.AddUnique(i);
		}
	}
}

void FInsectBoidsSimulator::SimulateParticles(int32 ParticleStartIndex, int32 ParticleEndIndex, float DeltaTime)
{
	SCOPED_NAMED_EVENT_TEXT("FInsectBoidsSimulator::SimulateParticles", FColor::Magenta);

	for (int32 i = ParticleStartIndex; i < FMath::Min(ParticleEndIndex, GetReadParticles().Num()); ++i)
	{
		FInsectBoidsParticle& CurrentWriteParticle = GetWriteParticles()[i];
		CurrentWriteParticle = GetReadParticles()[i]; // Copy over the particle

		// Iterate over other particles and try to steer away from them
		FVector SteerAwayDirection = FVector::Zero();

		// Also steer towards the center of the nearby particles
		FVector ParticleCenter = FVector::Zero();
		int32 NumParticlesCenter = 0;

		IterateOverNeighbors(i, [&](int32 Neighbor) {
			const FInsectBoidsParticle& OtherParticle = GetReadParticles()[Neighbor];
			CalculateSteerAwayVector(CurrentWriteParticle, OtherParticle, SteerAwayDirection);
			CalculateCenterPosition(CurrentWriteParticle, OtherParticle, ParticleCenter, NumParticlesCenter);
		});

		// Change velocity to steer away from other particles
		if (const float SteerAwayDirectionLength = SteerAwayDirection.Length() != 0.f)
		{
			SteerAwayDirection = SteerAwayDirection / SteerAwayDirectionLength;
			CurrentWriteParticle.Velocity = FMath::Lerp(CurrentWriteParticle.Velocity, SteerAwayDirection, Config.RepelStrength);
		}

		// Change velocity to steer towards the center of the particles
		if (NumParticlesCenter > 0)
		{
			ParticleCenter /= NumParticlesCenter;
			const FVector ParticleCenterVector = (ParticleCenter - CurrentWriteParticle.Position).GetSafeNormal();
			CurrentWriteParticle.Velocity = FMath::Lerp(CurrentWriteParticle.Velocity, ParticleCenterVector, Config.AttractionStrength);
		}

		// Move the particle along its velocity
		CurrentWriteParticle.Position += CurrentWriteParticle.Velocity * DeltaTime * Config.VelocityMultiplier;

		// Add some position jitter
		ApplyParticleJitter(CurrentWriteParticle, DeltaTime);

		// Move the particles using a global vector noise field
		ApplyParticleGlobalMovementVector(CurrentWriteParticle, DeltaTime);
	}
}

void FInsectBoidsSimulator::CalculateSteerAwayVector(const FInsectBoidsParticle& CurrentParticle, const FInsectBoidsParticle& OtherParticle, FVector& OutSteerVector) const
{
	if (FVector::Dist(CurrentParticle.Position, OtherParticle.Position) <= Config.ParticleRepelDistance)
	{
		FVector SteerAwayVector = (CurrentParticle.Position - OtherParticle.Position);

		// Make the strength inversely proportional to the distance
		SteerAwayVector = SteerAwayVector.GetSafeNormal() * ((Config.ParticleRepelDistance - SteerAwayVector.Length()) / Config.ParticleRepelDistance);
		OutSteerVector += SteerAwayVector;
	}
}

void FInsectBoidsSimulator::CalculateCenterPosition(const FInsectBoidsParticle& CurrentParticle, const FInsectBoidsParticle& OtherParticle, FVector& OutParticleCenter, int32& OutParticlesSampled) const
{
	if (FVector::Dist(CurrentParticle.Position, OtherParticle.Position) <= Config.AttractDistance)
	{
		OutParticleCenter += OtherParticle.Position;
		OutParticlesSampled++;
	}
}

void FInsectBoidsSimulator::ApplyParticleJitter(FInsectBoidsParticle& CurrentParticle, float DeltaSeconds)
{
	float SubstepDelta = 0.f;
	for (float JitTime = 0.f; JitTime < DeltaSeconds; SubstepDelta = FMath::Min(Config.JitterSubstepSize, DeltaSeconds - JitTime), JitTime += SubstepDelta)
	{
		const FVector ParticlePosition = CurrentParticle.Position / Config.JitterNoiseSize;
		FVector Jitter = FVector(FMath::PerlinNoise1D(ParticlePosition.X), FMath::PerlinNoise1D(ParticlePosition.Y), FMath::PerlinNoise1D(ParticlePosition.Z));
		Jitter *= (Config.JitterStrength / Config.JitterSubstepSize) * SubstepDelta;

		CurrentParticle.Position += Jitter;
	}
}

void FInsectBoidsSimulator::ApplyParticleGlobalMovementVector(FInsectBoidsParticle& CurrentParticle, float DeltaSeconds)
{
	const FVector ParticlePosition = CurrentParticle.Position / Config.GlobalNoiseForceSize;
	FVector MovementNoise = FVector(FMath::PerlinNoise1D(ParticlePosition.X), FMath::PerlinNoise1D(ParticlePosition.Y), FMath::PerlinNoise1D(ParticlePosition.Z));
	CurrentParticle.Position += MovementNoise * Config.GlobalNoiseForceStrength * DeltaSeconds;
}
