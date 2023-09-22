// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Templates/SharedPointer.h"
#include "Tasks/Task.h"
#include "InsectBoidsSimulator.generated.h"

USTRUCT(BlueprintType)
struct FInsectBoidsConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Insect Boids Config")
		int32 NumParticles = 1024;

	// The maximum distance away a particle will consider another particle for repelling
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Insect Boids Config")
		float ParticleRepelDistance = 64.f;

	// The maximum distance away a particle will consider another particle for attraction
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Insect Boids Config")
		float AttractDistance = 256.f;

	// Center/Steer Velocity multiplier, in cm/second
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Insect Boids Config")
		float VelocityMultiplier = 64.f;

	// Strength of the force repelling insects away from each other
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Insect Boids Config", meta=(ClampMin=0.f, ClampMax=1.f))
		float RepelStrength = 0.144f;

	// Strength of the force pulling insects to the center of the insect swarm
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Insect Boids Config", meta = (ClampMin = 0.f, ClampMax = 1.f))
		float AttractionStrength = 0.05f;

	// Strength of the jitter force. This number is dimensionless
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Insect Boids Config")
		float JitterStrength = 5.f;

	// The size of the noise field, in cms. Smaller numbers means more chaotic noise
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Insect Boids Config", meta = (ClampMin = 0.001f, ClampMax = 2048.f))
		float JitterNoiseSize = 0.132423f;

	// Since jitter isn't temporally stable, it will run in multiple substeps up to deltatime
	// This configures the size of the substep
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Insect Boids Config", meta = (ClampMin = 0.001f, ClampMax = 0.1f, Units = "s"))
		float JitterSubstepSize = 1.f / 300.f;

	// The size of the global noise field, in cms. Smaller numbers means more chaotic noise
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Insect Boids Config", meta = (ClampMin = 0.001f, ClampMax = 2048.f))
		float GlobalNoiseForceSize = 2048.f;

	// Strength of the global noise force (In cm/s)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Insect Boids Config")
		FVector GlobalNoiseForceStrength = FVector(64.f, 64.f, 4.f);
};

struct INSECTBOIDS_API FInsectBoidsParticle
{
	FVector Position = FVector(0.f);
	FVector Velocity = FVector(1.f, 0.f, 0.f);
};

/**
 * Runs a boids simulation using the task system
 */
class INSECTBOIDS_API FInsectBoidsSimulator 
	: public TSharedFromThis<FInsectBoidsSimulator>
{
public:
	FInsectBoidsSimulator() = default;
	FInsectBoidsSimulator(const FInsectBoidsConfig& InConfig, const FVector& ParticleCenter);

	// Waits for last frame's simulation to complete, kicks off the next frame's simulation
	void Simulate(float DeltaTime, const FInsectBoidsConfig& UpdatedConfig);

	const TArray<FInsectBoidsParticle>& GetReadParticles() const { return Particles[CurrentContext ^ 1]; };

protected:
	TArray<UE::Tasks::FTask> SimulationTasks;
	int32 NumParticlesPerTask = 32;

	FInsectBoidsConfig Config;
	TArray<FInsectBoidsParticle> Particles[2];
	int32 CurrentContext; // The current particle context that is being written to

	TArray<FInsectBoidsParticle>& GetWriteParticles() { return Particles[CurrentContext]; };
	void FlipParticleContext();

	// Map of grid coordinates to particle indicies
	static constexpr int32 NeighborGridSize = 32;
	TMap<FIntVector, TArray<int32, TInlineAllocator<NeighborGridSize>>> SpatialHashGrid;
	FVector GridSize = FVector(128.f);
	void IterateOverNeighbors(int32 ParticleIndex, TFunction<void(int32)> Function);

	void SetupNeighborGrid();
	void SimulateParticles(int32 ParticleStartIndex, int32 ParticleEndIndex, float DeltaTime);

	void CalculateSteerAwayVector(const FInsectBoidsParticle& CurrentParticle, const FInsectBoidsParticle& OtherParticle, FVector& OutSteerVector) const;
	void CalculateCenterPosition(const FInsectBoidsParticle& CurrentParticle, const FInsectBoidsParticle& OtherParticle, FVector& OutParticleCenter, int32& OutParticlesSampled) const;
	void ApplyParticleJitter(FInsectBoidsParticle& CurrentParticle, float DeltaSeconds);

	void ApplyParticleGlobalMovementVector(FInsectBoidsParticle& CurrentParticle, float DeltaSeconds);
};
