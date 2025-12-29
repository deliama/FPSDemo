// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ShooterTypes.h"
#include "ShooterUI.h"  // Needed for ShooterUI
#include "ShooterGameMode.generated.h"

class APlayerController;



/** Structure to hold team score data */
USTRUCT()
struct FTeamScoreData
{
	GENERATED_BODY()

	UPROPERTY()
	uint8 TeamID = 0;

	UPROPERTY()
	int32 Score = 0;

	FTeamScoreData() {}

	FTeamScoreData(uint8 InTeamID, int32 InScore)
		: TeamID(InTeamID), Score(InScore) {}
};

/**
 *  Simple GameMode for a first person shooter game
 *  Manages game UI
 *  Keeps track of team scores
 */
UCLASS(abstract)
class FPSDEMO_API AShooterGameMode : public AGameModeBase
{
	GENERATED_BODY()
	
protected:

	/** Type of UI widget to spawn */
	UPROPERTY(EditAnywhere, Category="Shooter")
	TSubclassOf<UShooterUI> ShooterUIClass;

public:

	/** Pointer to the UI widget */
	UPROPERTY()
	TObjectPtr<UShooterUI> ShooterUI;

	/** Array of team scores */
	UPROPERTY(ReplicatedUsing=OnRep_TeamScores)
	TArray<FTeamScoreData> TeamScores;

	/** Target score to win the game */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shooter", meta = (ClampMin = 1))
	int32 TargetScore = 10;

	/** Time to wait after victory before restarting (in seconds) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shooter", meta = (ClampMin = 0))
	float VictoryRestartDelay = 5.0f;

	/** Timer handle for victory restart */
	FTimerHandle VictoryRestartTimer;

	/** If true, the game has ended */
	UPROPERTY(Replicated, BlueprintReadOnly)
	bool bGameEnded = false;

	/** Winning team ID */
	UPROPERTY(Replicated, BlueprintReadOnly)
	uint8 WinningTeam = 255;

	/** Game time limit in seconds (0 = no time limit) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shooter", meta = (ClampMin = 0, Units = "s"))
	float GameTimeLimit = 300.0f;

	/** Remaining game time in seconds */
	UPROPERTY(ReplicatedUsing=OnRep_RemainingTime, BlueprintReadOnly)
	float RemainingTime = 0.0f;

	/** Time when the game started */
	float GameStartTime = 0.0f;

	/** Timer handle for updating remaining time */
	FTimerHandle TimeUpdateTimer;

	/** Array of player statistics */
	UPROPERTY(ReplicatedUsing=OnRep_PlayerStats)
	TArray<FPlayerStats> PlayerStatsArray;

protected:

	/** Gameplay initialization */
	virtual void BeginPlay() override;

	/** Replication function for TeamScores */
	UFUNCTION()
	void OnRep_TeamScores();

	/** Replication function for RemainingTime */
	UFUNCTION()
	void OnRep_RemainingTime();

	/** Replication function for PlayerStatsMap */
	UFUNCTION()
	void OnRep_PlayerStats();

	/** Updates the remaining game time */
	void UpdateRemainingTime();

	/** Checks if time limit has been reached */
	void CheckTimeLimit();

public:

	/** Increases the score for the given team */
	UFUNCTION(BlueprintCallable)
	void IncrementTeamScore(uint8 TeamByte);

	/** Records a kill for the given player controller */
	UFUNCTION(BlueprintCallable)
	void RecordKill(APlayerController* KillerController);

	/** Records a death for the given player controller */
	UFUNCTION(BlueprintCallable)
	void RecordDeath(APlayerController* VictimController);

	/** Gets player statistics for the given controller */
	UFUNCTION(BlueprintPure, Category="Shooter")
	FPlayerStats GetPlayerStats(APlayerController* PlayerController) const;

	/** Checks if a team has won and handles victory */
	void CheckVictoryCondition();

	/** Returns the remaining game time */
	UFUNCTION(BlueprintPure, Category="Shooter")
	float GetRemainingTime() const { return RemainingTime; }

	/** Called when a team wins */
	UFUNCTION(BlueprintImplementableEvent, Category="Shooter", meta = (DisplayName = "On Team Victory"))
	void BP_OnTeamVictory(uint8 WinningTeamByte);

	/** Restarts the game after victory */
	void RestartGameAfterVictory();

	/** Get the lifetime replicated props */
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
