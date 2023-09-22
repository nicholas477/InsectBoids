// Fill out your copyright notice in the Description page of Project Settings.


#include "InsectBoidsActor.h"

#include "Components/InstancedStaticMeshComponent.h"

AInsectBoidsActor::AInsectBoidsActor()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>("Root");
	SetRootComponent(Root);

	BoidMeshes = CreateDefaultSubobject<UInstancedStaticMeshComponent>("Boid Meshes");
	BoidMeshes->SetupAttachment(Root);
	BoidMeshes->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AInsectBoidsActor::BeginPlay()
{
	Super::BeginPlay();
	
	BoidsSimulation = MakeShared<FInsectBoidsSimulator>(BoidConfig, GetActorLocation());

	InstanceTransforms.Init(FTransform::Identity, BoidConfig.NumParticles);
	BoidMeshes->ClearInstances();
	BoidMeshes->AddInstances(InstanceTransforms, false, true);
}

void AInsectBoidsActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	BoidsSimulation->Simulate(DeltaTime, BoidConfig);

	int32 i = 0;
	for (const FInsectBoidsParticle& Particle : BoidsSimulation->GetReadParticles())
	{
		FTransform WorldTransform(Particle.Velocity.ToOrientationRotator().Quaternion(), Particle.Position);
		InstanceTransforms[i] = BoidLocalTransform * WorldTransform;
		i++;
	}

	BoidMeshes->BatchUpdateInstancesTransforms(0, InstanceTransforms, true, true);
}

