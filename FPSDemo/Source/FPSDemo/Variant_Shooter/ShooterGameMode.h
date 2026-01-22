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
 *  射击游戏 GameMode
 *  功能：
 *  - 管理游戏 UI（得分板、击杀提示等）
 *  - 跟踪团队得分和玩家统计数据（击杀/死亡/助攻）
 *  - 处理游戏胜利条件和游戏时间限制
 *  - 网络同步得分和游戏状态
 */
UCLASS(abstract)
class FPSDEMO_API AShooterGameMode : public AGameModeBase
{
	GENERATED_BODY()
	
protected:

	/** UI 组件类型（用于创建游戏 UI 实例） */
	UPROPERTY(EditAnywhere, Category="Shooter")
	TSubclassOf<UShooterUI> ShooterUIClass;

public:

	/** UI 组件实例指针（共享 UI，用于得分更新等） */
	UPROPERTY()
	TObjectPtr<UShooterUI> ShooterUI;

	/** 每个玩家的 UI widget 映射（用于游戏结算界面） */
	UPROPERTY()
	TMap<APlayerController*, TObjectPtr<UShooterUI>> PlayerUIMap;

	/** 团队得分数组（每个团队一个得分数据） */
	UPROPERTY(ReplicatedUsing=OnRep_TeamScores)
	TArray<FTeamScoreData> TeamScores;

	/** 获胜所需的目标得分（任一团队达到此分数即获胜） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shooter", meta = (ClampMin = 1))
	int32 TargetScore = 10;

	/** 胜利后重启游戏的等待时间（秒） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shooter", meta = (ClampMin = 0))
	float VictoryRestartDelay = 5.0f;

	/** 胜利重启定时器句柄 */
	FTimerHandle VictoryRestartTimer;

	/** 游戏是否已结束 */
	UPROPERTY(Replicated, BlueprintReadOnly)
	bool bGameEnded = false;

	/** 获胜团队 ID */
	UPROPERTY(Replicated, BlueprintReadOnly)
	uint8 WinningTeam = 255;

	/** 游戏时间限制（秒，0 = 无时间限制） */
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

	/** 增加指定团队的得分（当团队成员击杀敌方时调用） */
	UFUNCTION(BlueprintCallable)
	void IncrementTeamScore(uint8 TeamByte);

	/** 记录玩家击杀统计（增加击杀数） */
	UFUNCTION(BlueprintCallable)
	void RecordKill(APlayerController* KillerController);

	/** 记录玩家死亡统计（增加死亡数） */
	UFUNCTION(BlueprintCallable)
	void RecordDeath(APlayerController* VictimController);

	/** 获取指定玩家的统计数据（击杀/死亡/助攻） */
	UFUNCTION(BlueprintPure, Category="Shooter")
	FPlayerStats GetPlayerStats(APlayerController* PlayerController) const;

	/** 检查是否有团队达到获胜条件，如有则处理胜利逻辑 */
	void CheckVictoryCondition();

	/** Returns the remaining game time */
	UFUNCTION(BlueprintPure, Category="Shooter")
	float GetRemainingTime() const { return RemainingTime; }

protected:

	/** 为所有玩家显示游戏结束界面（内部函数） */
	void ShowGameEndScreenForAllPlayers(uint8 WinningTeamID);

	/** 为指定玩家获取或创建 UI widget（内部函数） */
	UShooterUI* GetOrCreateUIForPlayer(APlayerController* PC);

	/** 停止所有AI NPC的行为（游戏结束时调用） */
	void StopAllAIBehavior();

	/** Called when a team wins */
	UFUNCTION(BlueprintImplementableEvent, Category="Shooter", meta = (DisplayName = "On Team Victory"))
	void BP_OnTeamVictory(uint8 WinningTeamByte);

	/** 禁用所有玩家的输入（游戏结束时调用） */
	UFUNCTION(BlueprintCallable, Category="Shooter")
	void DisableAllPlayerInput();

	/** 重启游戏（手动调用，用于重来按钮） */
	UFUNCTION(BlueprintCallable, Category="Shooter")
	void RestartGame();

	/** 退出游戏（返回主菜单或关闭游戏） */
	UFUNCTION(BlueprintCallable, Category="Shooter")
	void QuitGame();

	/** Restarts the game after victory (deprecated - replaced by RestartGame) */
	void RestartGameAfterVictory();

	/** Get the lifetime replicated props */
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
