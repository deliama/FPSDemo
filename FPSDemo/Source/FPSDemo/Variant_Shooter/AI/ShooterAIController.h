// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "ShooterAIController.generated.h"

class AShooterNPC;  // Forward declaration
class UStateTreeAIComponent;
class UAIPerceptionComponent;
struct FAIStimulus;

DECLARE_DELEGATE_TwoParams(FShooterPerceptionUpdatedDelegate, AActor*, const FAIStimulus&);
DECLARE_DELEGATE_OneParam(FShooterPerceptionForgottenDelegate, AActor*);

/**
 *  射击游戏 AI 控制器
 * 功能：
 *  - 通过 StateTree（状态树）管理 NPC 行为（巡逻、追击、攻击等）
 *  - 通过 AI Perception（AI 感知系统）检测玩家（视觉、听觉）
 *  - 管理目标选择（锁定玩家、跟踪目标位置）
 *  - 处理 NPC 死亡和重生后的重新占据
 */
UCLASS(abstract)
class FPSDEMO_API AShooterAIController : public AAIController
{
	GENERATED_BODY()
	
	/** 运行 NPC 行为的 StateTree 组件（状态树：定义 AI 的行为逻辑） */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UStateTreeAIComponent* StateTreeAI;

	/** AI 感知组件：通过视觉、听觉等感知周围环境（检测玩家位置、听到射击声音等） */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UAIPerceptionComponent* AIPerception;

protected:

	/** 团队标签：用于识别友军和敌军（通过感知系统过滤目标） */
	UPROPERTY(EditAnywhere, Category="Shooter")
	FName TeamTag = FName("Enemy");

	/** 当前锁定的敌人目标（通常是玩家） */
	TObjectPtr<AActor> TargetEnemy;

public:

	/** AI 感知更新委托：当 AI 感知到新的目标或目标更新时触发（StateTree 任务可订阅） */
	FShooterPerceptionUpdatedDelegate OnShooterPerceptionUpdated;

	/** AI 感知遗忘委托：当 AI 丢失目标时触发（StateTree 任务可订阅） */
	FShooterPerceptionForgottenDelegate OnShooterPerceptionForgotten;

public:

	/** Constructor */
	AShooterAIController();

protected:

	/** Pawn initialization */
	virtual void OnPossess(APawn* InPawn) override;

protected:

	/** Called when the possessed pawn dies */
	UFUNCTION()
	void OnPawnDeath();

public:

	/** 设置当前目标敌人 */
	void SetCurrentTarget(AActor* Target);

	/** 清除当前目标敌人 */
	void ClearCurrentTarget();

	/** 获取当前目标敌人 */
	AActor* GetCurrentTarget() const { return TargetEnemy; };

	/** 由重生的 NPC 调用，请求重新占据该 NPC（恢复 AI 控制） */
	UFUNCTION(BlueprintCallable, Category="AI")
	void RequestRepossess(AShooterNPC* NPC);

	/** 停止 AI 行为（用于游戏结束等情况） */
	UFUNCTION(BlueprintCallable, Category="AI")
	void StopAIBehavior(const FString& Reason = TEXT(""));

protected:

	/** AI 感知更新回调：当感知到 Actor 或感知信息更新时调用（如看到玩家、听到声音） */
	UFUNCTION()
	void OnPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	/** AI 感知遗忘回调：当丢失对 Actor 的感知时调用（如玩家离开视野、声音消失） */
	UFUNCTION()
	void OnPerceptionForgotten(AActor* Actor);
};
