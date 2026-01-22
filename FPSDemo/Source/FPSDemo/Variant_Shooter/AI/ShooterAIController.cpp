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
	// 获取 Pawn 引用（在 UnPossess 之前）
	AShooterNPC* NPC = Cast<AShooterNPC>(GetPawn());
	
	// 检查 NPC 是否可以重生（在 UnPossess 之前检查）
	bool bShouldKeepControllerAlive = false;
	if (NPC && NPC->IsValidLowLevel())
	{
		// 如果 NPC 可以重生，保持 Controller 存活
		if (NPC->bCanRespawn && NPC->RespawnTime > 0.0f)
		{
			bShouldKeepControllerAlive = true;
		}
	}

	// 停止移动
	if (GetPathFollowingComponent())
	{
		GetPathFollowingComponent()->AbortMove(*this, FPathFollowingResultFlags::UserAbort);
	}

	// 停止 StateTree 逻辑（这会触发所有状态的 ExitState）
	if (StateTreeAI)
	{
		StateTreeAI->StopLogic(FString("Death"));
	}

	// 清除目标
	ClearCurrentTarget();

	// 确保 NPC 停止射击（在 UnPossess 之前）
	if (NPC && NPC->IsValidLowLevel())
	{
		NPC->StopShooting();
	}

	// 取消占据 Pawn
	UnPossess();

	// 如果 NPC 可以重生，保持 Controller 存活，等待重生
	if (bShouldKeepControllerAlive)
	{
		// Controller 保持存活，等待 NPC 重生的 RequestRepossess 调用
		return;
	}

	// 如果 NPC 不会重生，销毁 Controller
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

void AShooterAIController::StopAIBehavior(const FString& Reason)
{
	// 停止 AI 移动
	if (GetPathFollowingComponent())
	{
		GetPathFollowingComponent()->AbortMove(*this, FPathFollowingResultFlags::UserAbort);
	}

	// 停止 StateTree 逻辑
	if (StateTreeAI)
	{
		StateTreeAI->StopLogic(Reason.IsEmpty() ? FString("GameEnded") : Reason);
	}

	// 清除目标
	ClearCurrentTarget();

	// 停止 NPC 射击（如果有）
	if (APawn* ControlledPawn = GetPawn())
	{
		if (AShooterNPC* NPC = Cast<AShooterNPC>(ControlledPawn))
		{
			NPC->StopShooting();
		}
	}
}

void AShooterAIController::RequestRepossess(AShooterNPC* NPC)
{
	if(!NPC) return;

	if(GetPawn() != NPC)
	{
		// 如果 Controller 还没有占据这个 Pawn，先占据它
		Possess(NPC);
	}
	else
	{
		// 如果已经占据了这个 Pawn，重置状态并重启 StateTree
		
		// 清除当前目标
		ClearCurrentTarget();
		
		// 确保 NPC 停止射击（重要：防止重生后卡在射击状态）
		if (NPC && NPC->IsValidLowLevel())
		{
			NPC->StopShooting();
		}
		
		// 停止并重启 StateTree（这会重置所有 StateTree 状态）
		if(StateTreeAI)
		{
			StateTreeAI->StopLogic(TEXT("Respawn"));
			StateTreeAI->StartLogic();
			UE_LOG(LogTemp, Display, TEXT("StateTreeAI Restarted Successfully"));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("StateTreeAI is null - cannot restart"));
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
