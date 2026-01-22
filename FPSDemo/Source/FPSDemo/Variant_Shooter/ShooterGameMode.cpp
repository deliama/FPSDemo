// Copyright Epic Games, Inc. All Rights Reserved.


#include "Variant_Shooter/ShooterGameMode.h"
#include "ShooterUI.h"
#include "ShooterCharacter.h"
#include "Variant_Shooter/AI/ShooterAIController.h"
#include "Variant_Shooter/AI/ShooterNPC.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "AIController.h"
#include "Components/StateTreeAIComponent.h"
#include "EngineUtils.h"

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

	UE_LOG(LogTemp, Warning, TEXT("[IncrementTeamScore] Team %d score incremented to %d"), TeamByte, Score);

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
	// 注意：比分更新由 UI_Shooter widget 负责，这里不需要创建 WBP_ShooterUI
	// WBP_ShooterUI 只用于结算面板，应该在游戏结束时才创建和显示
	// 如果 ShooterUI 存在（可能是之前创建的），可以尝试更新，但通常不应该在这里创建
	if (ShooterUI)
	{
		UE_LOG(LogTemp, Warning, TEXT("[IncrementTeamScore] Updating score in UI (bGameEnded: %d)"), bGameEnded);
		// 注意：如果 ShooterUI 存在，可能是用于其他目的，但通常比分更新应该由 UI_Shooter 处理
		ShooterUI->BP_UpdateScore(TeamByte, Score);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[IncrementTeamScore] ShooterUI is null - score update should be handled by UI_Shooter widget"));
	}

	// Check victory condition
	UE_LOG(LogTemp, Warning, TEXT("[IncrementTeamScore] Calling CheckVictoryCondition"));
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
		UE_LOG(LogTemp, Warning, TEXT("[CheckVictoryCondition] Game already ended, skipping check"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[CheckVictoryCondition] Checking victory condition - TargetScore: %d"), TargetScore);

	// Check time limit first
	CheckTimeLimit();
	if (bGameEnded)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CheckVictoryCondition] Game ended due to time limit"));
		return;
	}

	// Check each team's score
	for (const FTeamScoreData& ScoreData : TeamScores)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CheckVictoryCondition] Team %d score: %d / %d"), 
			ScoreData.TeamID, ScoreData.Score, TargetScore);

		if (ScoreData.Score >= TargetScore)
		{
			UE_LOG(LogTemp, Warning, TEXT("[CheckVictoryCondition] VICTORY! Team %d reached target score %d"), 
				ScoreData.TeamID, TargetScore);

			// A team has won!
			bGameEnded = true;
			WinningTeam = ScoreData.TeamID;

			// 禁用所有玩家的输入
			DisableAllPlayerInput();

			// 为所有玩家显示游戏结束界面（胜利/失败）
			ShowGameEndScreenForAllPlayers(ScoreData.TeamID);

			// Notify Blueprint
			UE_LOG(LogTemp, Warning, TEXT("[CheckVictoryCondition] Calling BP_OnTeamVictory"));
			BP_OnTeamVictory(ScoreData.TeamID);

			// 不再自动重启游戏，改为玩家手动控制（移除自动重启定时器）

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

		// 禁用所有玩家的输入
		DisableAllPlayerInput();

		// 为所有玩家显示游戏结束界面（胜利/失败）
		ShowGameEndScreenForAllPlayers(WinningTeam);

		// Notify Blueprint
		BP_OnTeamVictory(WinningTeam);

		// 不再自动重启游戏，改为玩家手动控制（移除自动重启定时器）

		// Clear time update timer
		GetWorld()->GetTimerManager().ClearTimer(TimeUpdateTimer);
	}
}

void AShooterGameMode::DisableAllPlayerInput()
{
	// 遍历所有玩家控制器并禁用输入
	if (!HasAuthority())
	{
		return;
	}

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (APlayerController* PC = It->Get())
		{
			// 如果玩家控制着一个角色，禁用角色的输入
			if (APawn* ControlledPawn = PC->GetPawn())
			{
				ControlledPawn->DisableInput(PC);
			}

			// 显示鼠标光标并设置为UI模式（允许点击UI按钮）
			PC->SetShowMouseCursor(true);
			FInputModeUIOnly InputMode;
			InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			PC->SetInputMode(InputMode);
		}
	}

	// 停止所有AI NPC的行为
	StopAllAIBehavior();
}

void AShooterGameMode::ShowGameEndScreenForAllPlayers(uint8 WinningTeamID)
{
	// 为所有玩家显示游戏结束界面
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[ShowGameEndScreenForAllPlayers] Called on client, ignoring"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[ShowGameEndScreenForAllPlayers] Called - WinningTeamID: %d, bGameEnded: %d"), 
		WinningTeamID, bGameEnded);

	// 先为所有玩家创建 UI（如果还没有创建）
	int32 PlayerCount = 0;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (APlayerController* PC = It->Get())
		{
			// 确保为每个玩家创建 UI
			if (!PC->IsLocalController())
			{
				continue; // 只为本地玩家创建 UI
			}

			PlayerCount++;
			UE_LOG(LogTemp, Warning, TEXT("[ShowGameEndScreenForAllPlayers] Processing player %d: %s"), 
				PlayerCount, *GetNameSafe(PC));

			// 检查玩家所属的团队
			// 注意：游戏结束时玩家可能已经死亡，GetPawn() 可能返回 nullptr
			// 需要尝试多种方式获取团队 ID
			uint8 PlayerTeam = 255;
			
			// 方法1：从当前控制的 Pawn 获取团队 ID
			if (APawn* ControlledPawn = PC->GetPawn())
			{
				UE_LOG(LogTemp, Warning, TEXT("[ShowGameEndScreenForAllPlayers] Player has Pawn: %s"), *GetNameSafe(ControlledPawn));
				// 尝试从角色获取团队 ID（需要转换为 ShooterCharacter）
				if (AShooterCharacter* ShooterChar = Cast<AShooterCharacter>(ControlledPawn))
				{
					PlayerTeam = ShooterChar->GetTeamByte();
					UE_LOG(LogTemp, Warning, TEXT("[ShowGameEndScreenForAllPlayers] Got team from Pawn: %d (Character: %s)"), 
						PlayerTeam, *GetNameSafe(ShooterChar));
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("[ShowGameEndScreenForAllPlayers] Pawn is not a ShooterCharacter"));
				}
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("[ShowGameEndScreenForAllPlayers] Player has no Pawn"));
			}
			
			// 方法2：如果无法从 Pawn 获取，尝试从所有已存在的角色中查找该玩家的角色
			// 这适用于玩家已死亡但角色还在世界中的情况
			if (PlayerTeam == 255)
			{
				UE_LOG(LogTemp, Warning, TEXT("[ShowGameEndScreenForAllPlayers] Could not get team from current Pawn, searching for player's character in world"));
				
				// 遍历世界中的所有 ShooterCharacter，查找属于这个玩家的角色
				// 使用不同的变量名 CharIt 避免与外部循环的 It 冲突
				for (TActorIterator<AShooterCharacter> CharIt(GetWorld()); CharIt; ++CharIt)
				{
					if (AShooterCharacter* ShooterChar = *CharIt)
					{
						// 检查这个角色是否属于当前玩家控制器
						if (ShooterChar->GetController() == PC)
						{
							PlayerTeam = ShooterChar->GetTeamByte();
							UE_LOG(LogTemp, Warning, TEXT("[ShowGameEndScreenForAllPlayers] Found player's character in world, Team: %d"), PlayerTeam);
							break;
						}
					}
				}
			}
			
			// 如果仍然无法获取团队 ID，记录警告
			if (PlayerTeam == 255)
			{
				UE_LOG(LogTemp, Error, TEXT("[ShowGameEndScreenForAllPlayers] WARNING: Could not determine player team! PlayerTeam remains 255, bIsVictory will be false"));
			}

			// 判断玩家是否胜利（玩家团队与获胜团队相同）
			// 注意：如果 PlayerTeam 是 255（未获取到），bIsVictory 将始终为 false
			bool bIsVictory = (PlayerTeam != 255) && (PlayerTeam == WinningTeamID);

			UE_LOG(LogTemp, Warning, TEXT("[ShowGameEndScreenForAllPlayers] ===== VICTORY CHECK ====="));
			UE_LOG(LogTemp, Warning, TEXT("[ShowGameEndScreenForAllPlayers] PlayerTeam: %d, WinningTeamID: %d"), PlayerTeam, WinningTeamID);
			UE_LOG(LogTemp, Warning, TEXT("[ShowGameEndScreenForAllPlayers] PlayerTeam != 255: %d"), (PlayerTeam != 255));
			UE_LOG(LogTemp, Warning, TEXT("[ShowGameEndScreenForAllPlayers] PlayerTeam == WinningTeamID: %d"), (PlayerTeam == WinningTeamID));
			UE_LOG(LogTemp, Warning, TEXT("[ShowGameEndScreenForAllPlayers] Final bIsVictory: %d"), bIsVictory);
			UE_LOG(LogTemp, Warning, TEXT("[ShowGameEndScreenForAllPlayers] ========================="));

			// 优先使用已存在的共享 ShooterUI（在 IncrementTeamScore 中创建的）
			// 这样可以避免创建多个 UI widget 导致重叠
			UShooterUI* PlayerUI = nullptr;
			
			// 检查共享的 ShooterUI 是否存在且有效
			if (ShooterUI && IsValid(ShooterUI))
			{
				// 获取创建 ShooterUI 时使用的玩家控制器
				APlayerController* ShooterUIOwner = ShooterUI->GetOwningPlayer();
				
				// 检查 ShooterUI 是否属于当前玩家控制器
				// 在单玩家游戏中，ShooterUI 通常是为第一个玩家创建的
				// 在多人游戏中，每个客户端都有自己的本地玩家控制器
				if (ShooterUIOwner == PC)
				{
					// 如果 ShooterUI 属于当前玩家，直接使用
					PlayerUI = ShooterUI;
					UE_LOG(LogTemp, Warning, TEXT("[ShowGameEndScreenForAllPlayers] Using existing shared ShooterUI for player: %s"), 
						*GetNameSafe(PC));
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("[ShowGameEndScreenForAllPlayers] ShooterUI exists but belongs to different player. ShooterUI owner: %s, Current player: %s"), 
						*GetNameSafe(ShooterUIOwner), *GetNameSafe(PC));
				}
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("[ShowGameEndScreenForAllPlayers] ShooterUI is null or invalid"));
			}
			
			// 如果共享 UI 不可用，为这个玩家获取或创建独立的 UI
			if (!PlayerUI)
			{
				UE_LOG(LogTemp, Warning, TEXT("[ShowGameEndScreenForAllPlayers] Getting/creating UI for player: %s"), 
					*GetNameSafe(PC));
				PlayerUI = GetOrCreateUIForPlayer(PC);
			}

			if (PlayerUI)
			{
				// 三重检查：确保游戏确实已经结束
				if (bGameEnded && WinningTeamID != 255)
				{
					UE_LOG(LogTemp, Warning, TEXT("[ShowGameEndScreenForAllPlayers] Calling BP_ShowGameEndScreen for player"));
					UE_LOG(LogTemp, Warning, TEXT("[ShowGameEndScreenForAllPlayers] Parameters - bIsVictory: %d, WinningTeamID: %d, PlayerTeam: %d"), 
						bIsVictory, WinningTeamID, PlayerTeam);
					// 显示游戏结束界面（仅在游戏真正结束时调用）
					// 注意：这是唯一应该调用 BP_ShowGameEndScreen 的地方
					// 如果蓝图中 BP_UpdateScore 或其他地方也调用了显示结算面板，那是错误的
					PlayerUI->BP_ShowGameEndScreen(bIsVictory, WinningTeamID, PlayerTeam, PlayerStatsArray);
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("[ShowGameEndScreenForAllPlayers] ERROR: Cannot show game end screen! bGameEnded: %d, WinningTeamID: %d"), 
						bGameEnded, WinningTeamID);
				}
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("[ShowGameEndScreenForAllPlayers] Failed to get/create UI for player"));
			}
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("[ShowGameEndScreenForAllPlayers] Completed - Processed %d players"), PlayerCount);
}

UShooterUI* AShooterGameMode::GetOrCreateUIForPlayer(APlayerController* PC)
{
	if (!PC || !ShooterUIClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GetOrCreateUIForPlayer] Invalid PC or ShooterUIClass"));
		return nullptr;
	}

	// 检查是否已经为这个玩家创建了 UI
	if (TObjectPtr<UShooterUI>* ExistingUI = PlayerUIMap.Find(PC))
	{
		if (IsValid(*ExistingUI))
		{
			UE_LOG(LogTemp, Warning, TEXT("[GetOrCreateUIForPlayer] Reusing existing UI widget for player: %s"), 
				*GetNameSafe(PC));
			return *ExistingUI;
		}
		else
		{
			// UI 已被销毁，从 Map 中移除
			PlayerUIMap.Remove(PC);
		}
	}

	// 如果不存在，创建新的 UI widget
	UE_LOG(LogTemp, Warning, TEXT("[GetOrCreateUIForPlayer] Creating new UI widget for player: %s"), 
		*GetNameSafe(PC));

	UShooterUI* PlayerUI = CreateWidget<UShooterUI>(PC, ShooterUIClass);
	if (PlayerUI)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GetOrCreateUIForPlayer] UI widget created and added to viewport"));
		PlayerUI->AddToViewport(0);
		
		// 确保结算面板默认隐藏（只在游戏结束时显示）
		// 注意：这需要在蓝图中实现 BP_HideGameEndScreen 来隐藏结算面板
		PlayerUI->BP_HideGameEndScreen();
		UE_LOG(LogTemp, Warning, TEXT("[GetOrCreateUIForPlayer] Called BP_HideGameEndScreen to ensure end screen is hidden"));
		
		// 将 UI 存储到 Map 中，避免重复创建
		PlayerUIMap.Add(PC, PlayerUI);
		
		return PlayerUI;
	}

	UE_LOG(LogTemp, Error, TEXT("[GetOrCreateUIForPlayer] Failed to create UI widget"));
	return nullptr;
}

void AShooterGameMode::StopAllAIBehavior()
{
	// 遍历所有 AI Controller 并停止它们的行为
	if (!HasAuthority())
	{
		return;
	}

	for (TActorIterator<AShooterAIController> It(GetWorld()); It; ++It)
	{
		if (AShooterAIController* ShooterAIC = *It)
		{
			// 调用 AI Controller 的公共方法来停止所有行为
			ShooterAIC->StopAIBehavior(TEXT("GameEnded"));
		}
	}
}

void AShooterGameMode::RestartGame()
{
	// 仅在服务器端执行
	if (!HasAuthority())
	{
		return;
	}

	// 重置游戏状态
	bGameEnded = false;
	WinningTeam = 255;
	TeamScores.Empty();
	PlayerStatsArray.Empty();
	
	// 清理玩家 UI Map（游戏重启时清除所有玩家的 UI 引用）
	PlayerUIMap.Empty();

	// 清除所有定时器
	GetWorld()->GetTimerManager().ClearTimer(TimeUpdateTimer);
	GetWorld()->GetTimerManager().ClearTimer(VictoryRestartTimer);

	// 重新初始化游戏时间
	GameStartTime = GetWorld()->GetTimeSeconds();
	RemainingTime = GameTimeLimit;

	// 重新启用所有玩家的输入
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (APlayerController* PC = It->Get())
		{
			if (APawn* ControlledPawn = PC->GetPawn())
			{
				ControlledPawn->EnableInput(PC);
			}
		}
	}

	// 重启游戏：重新加载当前地图
	if (UWorld* World = GetWorld())
	{
		FString CurrentMapName = World->GetMapName();
		// Remove the "UEDPIE_" prefix if present (for PIE)
		CurrentMapName.RemoveFromStart(TEXT("UEDPIE_"));
		World->ServerTravel(CurrentMapName, false);
	}
}

void AShooterGameMode::QuitGame()
{
	// 退出游戏：返回主菜单或关闭游戏
	if (UWorld* World = GetWorld())
	{
		// 如果有主菜单地图，可以跳转到主菜单
		// World->ServerTravel(TEXT("/Game/Maps/MainMenu"), false);
		
		// 或者直接关闭游戏（在编辑器/PIE 中会停止 PIE）
		APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
		if (PC)
		{
			PC->ConsoleCommand(TEXT("quit"));
		}
	}
}

void AShooterGameMode::RestartGameAfterVictory()
{
	// 旧函数，保留用于向后兼容，现在直接调用 RestartGame
	RestartGame();
}

void AShooterGameMode::OnRep_TeamScores()
{
	// Update UI when scores are replicated
	// 注意：比分更新应该由 UI_Shooter widget 处理，这里不需要创建 WBP_ShooterUI
	// WBP_ShooterUI 只用于结算面板，应该在游戏结束时才创建
	// 如果 ShooterUI 存在，可以尝试更新，但通常比分更新应该由 UI_Shooter 处理
	if (ShooterUI)
	{
		for (const FTeamScoreData& ScoreData : TeamScores)
		{
			ShooterUI->BP_UpdateScore(ScoreData.TeamID, ScoreData.Score);
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[OnRep_TeamScores] ShooterUI is null - score update should be handled by UI_Shooter widget"));
	}
}

void AShooterGameMode::OnRep_RemainingTime()
{
	// Update UI when remaining time changes
	// 注意：时间更新应该由 UI_Shooter widget 处理，这里不需要创建 WBP_ShooterUI
	// WBP_ShooterUI 只用于结算面板，应该在游戏结束时才创建
	if (ShooterUI)
	{
		ShooterUI->BP_UpdateRemainingTime(RemainingTime);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[OnRep_RemainingTime] ShooterUI is null - time update should be handled by UI_Shooter widget"));
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
