// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ShooterTypes.h"
#include "ShooterUI.generated.h"

// Forward declarations
class APlayerController;



/**
 *  Simple scoreboard UI for a first person shooter game
 */
UCLASS(abstract)
class FPSDEMO_API UShooterUI : public UUserWidget
{
	GENERATED_BODY()
	
public:

	/** Allows Blueprint to update score sub-widgets */
	UFUNCTION(BlueprintImplementableEvent, Category="Shooter", meta = (DisplayName = "Update Score"))
	void BP_UpdateScore(uint8 TeamByte, int32 Score);

	/** Allows Blueprint to update remaining time display */
	UFUNCTION(BlueprintImplementableEvent, Category="Shooter", meta = (DisplayName = "Update Remaining Time"))
	void BP_UpdateRemainingTime(float RemainingTime);

	/** Shows a kill feed message (e.g., "Player1 killed Player2") */
	UFUNCTION(BlueprintImplementableEvent, Category="Shooter", meta = (DisplayName = "Show Kill Feed"))
	void BP_ShowKillFeed(const FString& KillerName, const FString& VictimName);

	/** Shows death screen with killer information and respawn timer */
	UFUNCTION(BlueprintImplementableEvent, Category="Shooter", meta = (DisplayName = "Show Death Screen"))
	void BP_ShowDeathScreen(const FString& KillerName, float RespawnTime);

	/** Hides death screen */
	UFUNCTION(BlueprintImplementableEvent, Category="Shooter", meta = (DisplayName = "Hide Death Screen"))
	void BP_HideDeathScreen();

	/** Shows victory screen with winning team and statistics */
	UFUNCTION(BlueprintImplementableEvent, Category="Shooter", meta = (DisplayName = "Show Victory Screen"))
	void BP_ShowVictoryScreen(uint8 WinningTeam, const TArray<FPlayerStats>& PlayerStats);

	/** Shows game end screen with victory/defeat message and restart/quit buttons
	 *  @param bIsVictory - true if the player's team won, false if they lost
	 *  @param WinningTeam - the ID of the winning team
	 *  @param PlayerTeam - the ID of the viewing player's team
	 *  @param PlayerStats - statistics for all players
	 */
	UFUNCTION(BlueprintImplementableEvent, Category="Shooter", meta = (DisplayName = "Show Game End Screen"))
	void BP_ShowGameEndScreen(bool bIsVictory, uint8 WinningTeam, uint8 PlayerTeam, const TArray<FPlayerStats>& PlayerStats);

	/** Hides game end screen (called when UI is initialized to ensure end screen is hidden) */
	UFUNCTION(BlueprintImplementableEvent, Category="Shooter", meta = (DisplayName = "Hide Game End Screen"))
	void BP_HideGameEndScreen();
};
