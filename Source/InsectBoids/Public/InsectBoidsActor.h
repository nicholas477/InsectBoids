// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InsectBoidsSimulator.h"
#include "InsectBoidsActor.generated.h"

/**
* Runs a insect boid simulation with instanced static meshes as particles.
*/
UCLASS(BlueprintType)
class INSECTBOIDS_API AInsectBoidsActor : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AInsectBoidsActor();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Insect Boids Actor")
		FInsectBoidsConfig BoidConfig;

	TSharedPtr<class FInsectBoidsSimulator> BoidsSimulation;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Insect Boids Actor")
		class USceneComponent* Root;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Insect Boids Actor")
		FTransform BoidLocalTransform;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Insect Boids Actor")
		class UInstancedStaticMeshComponent* BoidMeshes;

	TArray<FTransform> InstanceTransforms;
};
