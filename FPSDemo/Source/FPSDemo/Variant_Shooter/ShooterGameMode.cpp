// Copyright Epic Games, Inc. All Rights Reserved.


#include "Variant_Shooter/ShooterGameMode.h"
#include "ShooterUI.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Engine/Engine.h"

void AShooterGameMode::BeginPlay()
{
	Super::BeginPlay();

	// Initialize replicated scores
	TeamScores.Empty();

	// Create UI for the first player (others will create their own via PlayerController)
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
	{
		if (ShooterUIClass)
		{
			ShooterUI = CreateWidget<UShooterUI>(PC, ShooterUIClass);
			if (ShooterUI)
			{
				ShooterUI->AddToViewport(0);
			}
		}
	}
}

void AShooterGameMode::IncrementTeamScore(uint8 TeamByte)
{
	// Only process on server
	if (!HasAuthority())
	{
		return;
	}

	// Don't increment if game has ended
	if (bGameEnded)
	{
		return;
	}

	// Find or create team score entry
	int32 Score = 0;
	int32 TeamIndex = INDEX_NONE;
	
	for (int32 i = 0; i < TeamScores.Num(); ++i)
	{
		if (TeamScores[i].TeamID == TeamByte)
		{
			TeamIndex = i;
			Score = TeamScores[i].Score;
			break;
		}
	}

	// increment the score for the given team
	++Score;

	if (TeamIndex != INDEX_NONE)
	{
		// Update existing entry
		TeamScores[TeamIndex].Score = Score;
	}
	else
	{
		// Create new entry
		TeamScores.Add(FTeamScoreData(TeamByte, Score));
	}

	// update the UI for all players (scores will be replicated)
	// UI updates will happen via OnRep_TeamScores on clients
	if (ShooterUI)
	{
		ShooterUI->BP_UpdateScore(TeamByte, Score);
	}

	// Check victory condition
	CheckVictoryCondition();
}

void AShooterGameMode::CheckVictoryCondition()
{
	// Only check on server
	if (!HasAuthority())
	{
		return;
	}

	// Check each team's score
	for (const FTeamScoreData& ScoreData : TeamScores)
	{
		if (ScoreData.Score >= TargetScore)
		{
			// A team has won!
			bGameEnded = true;
			WinningTeam = ScoreData.TeamID;

			// Notify Blueprint
			BP_OnTeamVictory(ScoreData.TeamID);

			// Schedule game restart
			GetWorld()->GetTimerManager().SetTimer(VictoryRestartTimer, this, &AShooterGameMode::RestartGameAfterVictory, VictoryRestartDelay, false);

			break;
		}
	}
}

void AShooterGameMode::RestartGameAfterVictory()
{
	// Reset game state
	bGameEnded = false;
	WinningTeam = 255;
	TeamScores.Empty();

	// Restart the game by traveling to the same map
	// This will reset all actors and respawn players
	if (UWorld* World = GetWorld())
	{
		FString CurrentMapName = World->GetMapName();
		// Remove the "UEDPIE_" prefix if present (for PIE)
		CurrentMapName.RemoveFromStart(TEXT("UEDPIE_"));
		World->ServerTravel(CurrentMapName, false);
	}
}

void AShooterGameMode::OnRep_TeamScores()
{
	// Update UI when scores are replicated
	for (const FTeamScoreData& ScoreData : TeamScores)
	{
		if (ShooterUI)
		{
			ShooterUI->BP_UpdateScore(ScoreData.TeamID, ScoreData.Score);
		}
	}
}

void AShooterGameMode::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AShooterGameMode, TeamScores);
	DOREPLIFETIME(AShooterGameMode, bGameEnded);
	DOREPLIFETIME(AShooterGameMode, WinningTeam);
}
