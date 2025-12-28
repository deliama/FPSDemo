// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ShooterGameMode.generated.h"

class UShooterUI;

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

	/** Pointer to the UI widget */
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

protected:

	/** Gameplay initialization */
	virtual void BeginPlay() override;

	/** Replication function for TeamScores */
	UFUNCTION()
	void OnRep_TeamScores();

public:

	/** Increases the score for the given team */
	UFUNCTION(BlueprintCallable)
	void IncrementTeamScore(uint8 TeamByte);

	/** Checks if a team has won and handles victory */
	void CheckVictoryCondition();

	/** Called when a team wins */
	UFUNCTION(BlueprintImplementableEvent, Category="Shooter", meta = (DisplayName = "On Team Victory"))
	void BP_OnTeamVictory(uint8 WinningTeamByte);

	/** Restarts the game after victory */
	void RestartGameAfterVictory();

	/** Get the lifetime replicated props */
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
