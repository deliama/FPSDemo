// Copyright Epic Games, Inc. All Rights Reserved.


#include "Variant_Shooter/AI/ShooterAIController.h"
#include "ShooterNPC.h"
#include "Components/StateTreeAIComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "AI/Navigation/PathFollowingAgentInterface.h"

AShooterAIController::AShooterAIController()
{
	// create the StateTree component
	StateTreeAI = CreateDefaultSubobject<UStateTreeAIComponent>(TEXT("StateTreeAI"));

	// create the AI perception component. It will be configured in BP
	AIPerception = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("AIPerception"));

	// subscribe to the AI perception delegates
	AIPerception->OnTargetPerceptionUpdated.AddDynamic(this, &AShooterAIController::OnPerceptionUpdated);
	AIPerception->OnTargetPerceptionForgotten.AddDynamic(this, &AShooterAIController::OnPerceptionForgotten);
}

void AShooterAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	// ensure we're possessing an NPC
	if (AShooterNPC* NPC = Cast<AShooterNPC>(InPawn))
	{
		// add the team tag to the pawn
		NPC->Tags.Add(TeamTag);

		// subscribe to the pawn's OnDeath delegate if not already subscribed
		if (!NPC->OnPawnDeath.IsAlreadyBound(this, &AShooterAIController::OnPawnDeath))
		{
			NPC->OnPawnDeath.AddDynamic(this, &AShooterAIController::OnPawnDeath);
		}
	}
}

void AShooterAIController::OnPawnDeath()
{
	// stop movement
	GetPathFollowingComponent()->AbortMove(*this, FPathFollowingResultFlags::UserAbort);

	// stop StateTree logic
	StateTreeAI->StopLogic(FString(""));

	// unpossess the pawn
	UnPossess();

	// Check if the pawn can respawn
	if (AShooterNPC* NPC = Cast<AShooterNPC>(GetPawn()))
	{
		// If the NPC can respawn, keep the controller alive
		if (NPC->bCanRespawn && NPC->RespawnTime > 0.0f)
		{
			// Controller stays alive to possess the respawned NPC
			return;
		}
	}

	// destroy this controller
	Destroy();
}

void AShooterAIController::SetCurrentTarget(AActor* Target)
{
	TargetEnemy = Target;
}

void AShooterAIController::ClearCurrentTarget()
{
	TargetEnemy = nullptr;
}

void AShooterAIController::RequestRepossess(AShooterNPC* NPC)
{
	if(!NPC) return;

	if(GetPawn() != NPC)
	{
		Possess(NPC);
	}else
	{
		ClearCurrentTarget();
		if(StateTreeAI)
		{
			StateTreeAI->StopLogic(TEXT("Respawn"));
			StateTreeAI->StartLogic();
			UE_LOG(LogTemp, Display, TEXT("StateTreeAI Restarted Successfully"));
		}else
		{
			UE_LOG(LogTemp, Display, TEXT("StateTreeAI Restarted Failed"));
		}
	}
}

void AShooterAIController::OnPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	// pass the data to the StateTree delegate hook
	OnShooterPerceptionUpdated.ExecuteIfBound(Actor, Stimulus);
}

void AShooterAIController::OnPerceptionForgotten(AActor* Actor)
{
	// pass the data to the StateTree delegate hook
	OnShooterPerceptionForgotten.ExecuteIfBound(Actor);
}
