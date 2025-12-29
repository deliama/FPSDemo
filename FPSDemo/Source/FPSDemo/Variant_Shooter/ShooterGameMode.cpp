// Copyright Epic Games, Inc. All Rights Reserved.


#include "Variant_Shooter/ShooterGameMode.h"
#include "ShooterUI.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"

void AShooterGameMode::BeginPlay()
{
	Super::BeginPlay();

	// Initialize replicated scores
	TeamScores.Empty();

	// Initialize player stats
	PlayerStatsArray.Empty();

	// Initialize game time
	if (HasAuthority())
	{
		GameStartTime = GetWorld()->GetTimeSeconds();
		RemainingTime = GameTimeLimit;

		// Start timer to update remaining time (update every second)
		if (GameTimeLimit > 0.0f)
		{
			GetWorld()->GetTimerManager().SetTimer(TimeUpdateTimer, this, &AShooterGameMode::UpdateRemainingTime, 1.0f, true);
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
	// Check if UI exists, if not create it
	if (!ShooterUI && ShooterUIClass)
	{
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
		{
			ShooterUI = CreateWidget<UShooterUI>(PC, ShooterUIClass);
			if (ShooterUI)
			{
				ShooterUI->AddToViewport(0);
			}
		}
	}
	
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

	// Don't check if game has already ended
	if (bGameEnded)
	{
		return;
	}

	// Check time limit first
	CheckTimeLimit();
	if (bGameEnded)
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

		// Show victory screen
		// Check if UI exists, if not create it
		if (!ShooterUI && ShooterUIClass)
		{
			if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
			{
				ShooterUI = CreateWidget<UShooterUI>(PC, ShooterUIClass);
				if (ShooterUI)
				{
					ShooterUI->AddToViewport(0);
				}
			}
		}
		
		if (ShooterUI)
		{
			ShooterUI->BP_ShowVictoryScreen(ScoreData.TeamID, PlayerStatsArray);
		}

		// Notify Blueprint
		BP_OnTeamVictory(ScoreData.TeamID);

		// Schedule game restart
		GetWorld()->GetTimerManager().SetTimer(VictoryRestartTimer, this, &AShooterGameMode::RestartGameAfterVictory, VictoryRestartDelay, false);

			break;
		}
	}
}

void AShooterGameMode::UpdateRemainingTime()
{
	// Only update on server
	if (!HasAuthority() || bGameEnded)
	{
		return;
	}

	// Calculate remaining time
	const float ElapsedTime = GetWorld()->GetTimeSeconds() - GameStartTime;
	RemainingTime = FMath::Max(0.0f, GameTimeLimit - ElapsedTime);

	// Check if time has run out
	if (RemainingTime <= 0.0f)
	{
		CheckTimeLimit();
	}
}

void AShooterGameMode::CheckTimeLimit()
{
	// Only check on server
	if (!HasAuthority() || bGameEnded)
	{
		return;
	}

	// Only check if time limit is enabled
	if (GameTimeLimit <= 0.0f)
	{
		return;
	}

	// Check if time has run out
	if (RemainingTime <= 0.0f)
	{
		// Time's up! Determine winner by score
		bGameEnded = true;

		// Find team with highest score
		int32 HighestScore = -1;
		uint8 WinningTeamID = 255;

		for (const FTeamScoreData& ScoreData : TeamScores)
		{
			if (ScoreData.Score > HighestScore)
			{
				HighestScore = ScoreData.Score;
				WinningTeamID = ScoreData.TeamID;
			}
		}

		// If there's a tie, no winner (or could use other tie-breaker logic)
		if (HighestScore >= 0)
		{
			WinningTeam = WinningTeamID;
		}

		// Show victory screen
		// Check if UI exists, if not create it
		if (!ShooterUI && ShooterUIClass)
		{
			if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
			{
				ShooterUI = CreateWidget<UShooterUI>(PC, ShooterUIClass);
				if (ShooterUI)
				{
					ShooterUI->AddToViewport(0);
				}
			}
		}
		
		if (ShooterUI)
		{
			ShooterUI->BP_ShowVictoryScreen(WinningTeam, PlayerStatsArray);
		}

		// Notify Blueprint
		BP_OnTeamVictory(WinningTeam);

		// Schedule game restart
		GetWorld()->GetTimerManager().SetTimer(VictoryRestartTimer, this, &AShooterGameMode::RestartGameAfterVictory, VictoryRestartDelay, false);

		// Clear time update timer
		GetWorld()->GetTimerManager().ClearTimer(TimeUpdateTimer);
	}
}

void AShooterGameMode::RestartGameAfterVictory()
{
	// Reset game state
	bGameEnded = false;
	WinningTeam = 255;
	TeamScores.Empty();

	// Clear time update timer
	GetWorld()->GetTimerManager().ClearTimer(TimeUpdateTimer);

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
	// Check if UI exists, if not create it
	if (!ShooterUI && ShooterUIClass)
	{
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
		{
			ShooterUI = CreateWidget<UShooterUI>(PC, ShooterUIClass);
			if (ShooterUI)
			{
				ShooterUI->AddToViewport(0);
			}
		}
	}
	
	for (const FTeamScoreData& ScoreData : TeamScores)
	{
		if (ShooterUI)
		{
			ShooterUI->BP_UpdateScore(ScoreData.TeamID, ScoreData.Score);
		}
	}
}

void AShooterGameMode::OnRep_RemainingTime()
{
	// Update UI when remaining time changes
	// This can be used in Blueprint to update time display
	// Check if UI exists, if not create it
	if (!ShooterUI && ShooterUIClass)
	{
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
		{
			ShooterUI = CreateWidget<UShooterUI>(PC, ShooterUIClass);
			if (ShooterUI)
			{
				ShooterUI->AddToViewport(0);
			}
		}
	}
	
	if (ShooterUI)
	{
		ShooterUI->BP_UpdateRemainingTime(RemainingTime);
	}
}

void AShooterGameMode::RecordKill(APlayerController* KillerController)
{
	// Only process on server
	if (!HasAuthority() || !KillerController)
	{
		return;
	}

	// Find or create stats entry
	int32 StatsIndex = INDEX_NONE;
	for (int32 i = 0; i < PlayerStatsArray.Num(); ++i)
	{
		if (PlayerStatsArray[i].PlayerController == KillerController)
		{
			StatsIndex = i;
			break;
		}
	}

	if (StatsIndex != INDEX_NONE)
	{
		PlayerStatsArray[StatsIndex].Kills++;
	}
	else
	{
		FPlayerStats NewStats(KillerController);
		NewStats.Kills = 1;
		PlayerStatsArray.Add(NewStats);
	}
}

void AShooterGameMode::RecordDeath(APlayerController* VictimController)
{
	// Only process on server
	if (!HasAuthority() || !VictimController)
	{
		return;
	}

	// Find or create stats entry
	int32 StatsIndex = INDEX_NONE;
	for (int32 i = 0; i < PlayerStatsArray.Num(); ++i)
	{
		if (PlayerStatsArray[i].PlayerController == VictimController)
		{
			StatsIndex = i;
			break;
		}
	}

	if (StatsIndex != INDEX_NONE)
	{
		PlayerStatsArray[StatsIndex].Deaths++;
	}
	else
	{
		FPlayerStats NewStats(VictimController);
		NewStats.Deaths = 1;
		PlayerStatsArray.Add(NewStats);
	}
}

FPlayerStats AShooterGameMode::GetPlayerStats(APlayerController* PlayerController) const
{
	for (const FPlayerStats& Stats : PlayerStatsArray)
	{
		if (Stats.PlayerController == PlayerController)
		{
			return Stats;
		}
	}
	return FPlayerStats();
}

void AShooterGameMode::OnRep_PlayerStats()
{
	// Update UI when player stats change
	// This can be used in Blueprint to update statistics display
}

void AShooterGameMode::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AShooterGameMode, TeamScores);
	DOREPLIFETIME(AShooterGameMode, bGameEnded);
	DOREPLIFETIME(AShooterGameMode, WinningTeam);
	DOREPLIFETIME(AShooterGameMode, RemainingTime);
	DOREPLIFETIME(AShooterGameMode, PlayerStatsArray);
}
