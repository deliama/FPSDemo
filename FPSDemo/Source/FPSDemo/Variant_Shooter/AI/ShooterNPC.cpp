// Copyright Epic Games, Inc. All Rights Reserved.


#include "Variant_Shooter/AI/ShooterNPC.h"
#include "ShooterWeapon.h"
#include "ShooterCharacter.h"  // Needed for AShooterCharacter
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Engine/World.h"
#include "ShooterGameMode.h"
#include "ShooterAIController.h"  // Include the AI controller header
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"
#include "Net/UnrealNetwork.h"

void AShooterNPC::BeginPlay()
{
	Super::BeginPlay();

	//记录NPC出生地点
	StartTransform = GetActorTransform();
	
	// Enable replication
	bReplicates = true;
	SetReplicateMovement(true);

	// spawn the weapon
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.Instigator = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	Weapon = GetWorld()->SpawnActor<AShooterWeapon>(WeaponClass, GetActorTransform(), SpawnParams);

	// Properly attach the weapon and activate it
	if (Weapon)
	{
		AttachWeaponMeshes(Weapon);
		// Add the weapon to the weapon holder system
		AddWeaponClass(Weapon->GetClass());
	}
}

void AShooterNPC::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	// clear the death timer
	GetWorld()->GetTimerManager().ClearTimer(DeathTimer);
	
	// clear the respawn timer
	GetWorld()->GetTimerManager().ClearTimer(RespawnTimer);
}

float AShooterNPC::TakeDamage(float Damage, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	// Only process damage on server
	if (!HasAuthority())
	{
		return 0.0f;
	}

	// ignore if already dead
	if (bIsDead)
	{
		return 0.0f;
	}

	// Store the instigator for kill tracking
	if (EventInstigator)
	{
		LastDamageInstigator = EventInstigator;
	}

	// Reduce HP
	CurrentHP -= Damage;

	// Have we depleted HP?
	if (CurrentHP <= 0.0f)
	{
		Die();
	}

	return Damage;
}

void AShooterNPC::AttachWeaponMeshes(AShooterWeapon* WeaponToAttach)
{
	const FAttachmentTransformRules AttachmentRule(EAttachmentRule::SnapToTarget, false);

	// attach the weapon actor
	WeaponToAttach->AttachToActor(this, AttachmentRule);

	// attach the weapon meshes
	WeaponToAttach->GetFirstPersonMesh()->AttachToComponent(GetFirstPersonMesh(), AttachmentRule, FirstPersonWeaponSocket);
	WeaponToAttach->GetThirdPersonMesh()->AttachToComponent(GetMesh(), AttachmentRule, FirstPersonWeaponSocket);
}

void AShooterNPC::PlayFiringMontage(UAnimMontage* Montage)
{
	// unused
}

void AShooterNPC::AddWeaponRecoil(float Recoil)
{
	// unused
}

void AShooterNPC::UpdateWeaponHUD(int32 CurrentAmmo, int32 MagazineSize)
{
	// unused
}

FVector AShooterNPC::GetWeaponTargetLocation()
{
	// start aiming from the camera location
	const FVector AimSource = GetFirstPersonCameraComponent()->GetComponentLocation();

	FVector AimDir, AimTarget = FVector::ZeroVector;

	// do we have an aim target?
	if (CurrentAimTarget)
	{
		// target the actor location
		AimTarget = CurrentAimTarget->GetActorLocation();

		// apply a vertical offset to target head/feet
		AimTarget.Z += FMath::RandRange(MinAimOffsetZ, MaxAimOffsetZ);

		// get the aim direction and apply randomness in a cone
		AimDir = (AimTarget - AimSource).GetSafeNormal();
		AimDir = UKismetMathLibrary::RandomUnitVectorInConeInDegrees(AimDir, AimVarianceHalfAngle);

		
	} else {

		// no aim target, so just use the camera facing
		AimDir = UKismetMathLibrary::RandomUnitVectorInConeInDegrees(GetFirstPersonCameraComponent()->GetForwardVector(), AimVarianceHalfAngle);

	}

	// calculate the unobstructed aim target location
	AimTarget = AimSource + (AimDir * AimRange);

	// run a visibility trace to see if there's obstructions
	FHitResult OutHit;

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	GetWorld()->LineTraceSingleByChannel(OutHit, AimSource, AimTarget, ECC_Visibility, QueryParams);

	// return either the impact point or the trace end
	return OutHit.bBlockingHit ? OutHit.ImpactPoint : OutHit.TraceEnd;
}

void AShooterNPC::AddWeaponClass(const TSubclassOf<AShooterWeapon>& InWeaponClass)
{
	// If we already have a weapon, deactivate it
	if (Weapon)
	{
		OnWeaponDeactivated(Weapon);
	}

	// Spawn the new weapon if class is different
	if (InWeaponClass && (!Weapon || !Weapon->IsA(InWeaponClass)))
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.Instigator = this;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AShooterWeapon* NewWeapon = GetWorld()->SpawnActor<AShooterWeapon>(InWeaponClass, GetActorTransform(), SpawnParams);
		if (NewWeapon)
		{
			Weapon = NewWeapon;
		}
	}

	// Activate the current weapon
	if (Weapon)
	{
		OnWeaponActivated(Weapon);
	}
}

void AShooterNPC::OnWeaponActivated(AShooterWeapon* InWeapon)
{
	// Attach the weapon meshes
	AttachWeaponMeshes(InWeapon);
	
	// Set up weapon owner
	if (InWeapon)
	{
		InWeapon->SetOwner(this);
	}
}

void AShooterNPC::OnWeaponDeactivated(AShooterWeapon* InWeapon)
{
	// Detach the weapon if needed
	if (InWeapon)
	{
		InWeapon->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	}
}

void AShooterNPC::OnSemiWeaponRefire()
{
	// are we still shooting?
	if (bIsShooting)
	{
		// fire the weapon
		Weapon->StartFiring();
	}
}

void AShooterNPC::Die()
{
	// 如果已经死亡，忽略
	if (bIsDead)
	{
		return;
	}

	// 停止射击（重要：死亡时确保停止所有射击动作）
	StopShooting();

	// 设置死亡标志
	bIsDead = true;

	// Record statistics
	if (AShooterGameMode* GM = Cast<AShooterGameMode>(GetWorld()->GetAuthGameMode()))
	{
		UE_LOG(LogTemp, Warning, TEXT("[ShooterNPC::Die] NPC died - Team: %d"), TeamByte);
		
		// Record kill for the killer (NPCs don't have PlayerController, so only record if killer is a player)
		if (LastDamageInstigator)
		{
			if (APlayerController* KillerPC = Cast<APlayerController>(LastDamageInstigator))
			{
				GM->RecordKill(KillerPC);
			}
		}

		// 增加击杀者团队的得分（不是 NPC 自己的团队得分！）
		// 获取击杀者的团队ID（从造成伤害的控制器获取其控制的角色的团队）
		uint8 KillerTeamByte = 255;
		FString KillerType = TEXT("Unknown");
		
		if (LastDamageInstigator && LastDamageInstigator->GetPawn())
		{
			UE_LOG(LogTemp, Warning, TEXT("[ShooterNPC::Die] LastDamageInstigator: %s, Pawn: %s"), 
				*GetNameSafe(LastDamageInstigator), *GetNameSafe(LastDamageInstigator->GetPawn()));
			
			// 尝试从击杀者角色获取团队ID
			if (AShooterCharacter* KillerCharacter = Cast<AShooterCharacter>(LastDamageInstigator->GetPawn()))
			{
				KillerTeamByte = KillerCharacter->GetTeamByte();
				KillerType = TEXT("ShooterCharacter");
				UE_LOG(LogTemp, Warning, TEXT("[ShooterNPC::Die] Killer is ShooterCharacter, TeamByte: %d"), KillerTeamByte);
			}
			// 如果是另一个 NPC 击杀的，也获取其团队ID
			else if (AShooterNPC* KillerNPC = Cast<AShooterNPC>(LastDamageInstigator->GetPawn()))
			{
				KillerTeamByte = KillerNPC->GetTeamByte();
				KillerType = TEXT("ShooterNPC");
				UE_LOG(LogTemp, Warning, TEXT("[ShooterNPC::Die] Killer is ShooterNPC, TeamByte: %d"), KillerTeamByte);
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[ShooterNPC::Die] LastDamageInstigator or Pawn is null"));
		}

		UE_LOG(LogTemp, Warning, TEXT("[ShooterNPC::Die] ===== NPC KILL SCORE LOGIC ====="));
		UE_LOG(LogTemp, Warning, TEXT("[ShooterNPC::Die] NPC TeamByte: %d, Killer TeamByte: %d, Killer Type: %s"), 
			TeamByte, KillerTeamByte, *KillerType);
		UE_LOG(LogTemp, Warning, TEXT("[ShooterNPC::Die] KillerTeamByte != 255: %d"), (KillerTeamByte != 255));
		UE_LOG(LogTemp, Warning, TEXT("[ShooterNPC::Die] KillerTeamByte != TeamByte: %d"), (KillerTeamByte != TeamByte));

		// 只有当击杀者和被击杀者属于不同团队时才增加得分
		// 注意：这里应该增加击杀者的团队得分，而不是 NPC 自己的团队得分！
		if (KillerTeamByte != 255 && KillerTeamByte != TeamByte)
		{
			UE_LOG(LogTemp, Warning, TEXT("[ShooterNPC::Die] ✓ Incrementing score for KILLER team %d (killer) vs NPC team %d (victim)"), 
				KillerTeamByte, TeamByte);
			GM->IncrementTeamScore(KillerTeamByte);  // 修复：增加击杀者的团队得分
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[ShooterNPC::Die] ✗ Not incrementing score - KillerTeam: %d, VictimTeam: %d"), 
				KillerTeamByte, TeamByte);
		}
		UE_LOG(LogTemp, Warning, TEXT("[ShooterNPC::Die] =================================="));
	}

	// disable capsule collision
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// stop movement
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->StopActiveMovement();

	// enable ragdoll physics on the third person mesh
	GetMesh()->SetCollisionProfileName(RagdollCollisionProfile);
	GetMesh()->SetSimulatePhysics(true);
	GetMesh()->SetPhysicsBlendWeight(1.0f);

	// decide whether to respawn or destroy
	if (bCanRespawn && RespawnTime > 0.0f)
	{
		// schedule respawn
		GetWorld()->GetTimerManager().SetTimer(RespawnTimer, this, &AShooterNPC::Respawn, RespawnTime, false);
	}
	else
	{
		// schedule actor destruction
		GetWorld()->GetTimerManager().SetTimer(DeathTimer, this, &AShooterNPC::DeferredDestruction, DeferredDestructionTime, false);
	}
}

void AShooterNPC::DeferredDestruction()
{
	Destroy();
}

void AShooterNPC::Respawn()
{
	// 将 NPC 传送回出生点
	FTransform RespawnTransform = StartTransform;
	FVector StartLocation = RespawnTransform.GetLocation();
	StartLocation.Z += 10.0f;
	RespawnTransform.SetLocation(StartLocation);
	SetActorTransform(RespawnTransform, false, nullptr, ETeleportType::TeleportPhysics);
	
	// 重置生命值
	CurrentHP = 100.0f; // 或者使用最大生命值
	bIsDead = false;

	// 重置射击相关状态（重要：防止重生后卡在射击状态）
	bIsShooting = false;
	CurrentAimTarget = nullptr;
	
	// 确保武器停止射击（如果武器还在射击状态）
	if (Weapon && Weapon->IsValidLowLevel())
	{
		Weapon->StopFiring();
		// 先停用武器
		OnWeaponDeactivated(Weapon);
	}

	// Reset physics
	GetMesh()->SetSimulatePhysics(false);
	GetMesh()->SetCollisionProfileName(TEXT("Pawn"));
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	
	// 重置物理变换以匹配骨骼网格
	GetMesh()->AttachToComponent(GetCapsuleComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	GetMesh()->SetRelativeLocationAndRotation(FVector(0.0f, 0.0f, -90.0f), FRotator(0.0f, -90.0f, 0.0f));
	GetMesh()->ResetAllBodiesSimulatePhysics();
	
	// 重置移动
	GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_Walking);
	GetCharacterMovement()->bUseControllerDesiredRotation = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->Velocity = FVector::ZeroVector;
	GetCharacterMovement()->Activate();

	// 确保武器正确附加和激活
	if (Weapon && Weapon->IsValidLowLevel())
	{
		// 附加武器网格
		AttachWeaponMeshes(Weapon);
		
		// 激活武器
		OnWeaponActivated(Weapon);
	}
	
	// 重置网络复制设置
	bReplicates = true;
	SetReplicateMovement(true);
	
	// 确保 NPC 正确启用
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
	
	// 通知重生完成
	OnAfterRespawn();
}

void AShooterNPC::OnAfterRespawn()
{
	// 此方法在 NPC 重生后调用，用于设置重生后的状态
	// 尝试让 AI Controller 重新占据这个 Pawn
	
	// 仅在服务器端处理（Controller 相关操作应该是服务器权威的）
	if (!HasAuthority())
	{
		return;
	}
	
	AController* NPCController = GetController();
	
	// 如果没有 Controller，可能需要重新创建（这种情况应该很少见）
	if (!NPCController)
	{
		UE_LOG(LogTemp, Warning, TEXT("NPC respawned without AI controller - this may need manual repossessing"));
		return;
	}
	
	// 如果 Controller 还没有占据这个 Pawn，先占据它
	if (NPCController->GetPawn() != this)
	{
		NPCController->Possess(this);
		UE_LOG(LogTemp, Log, TEXT("AI Controller repossessed NPC after respawn"));
	}
	
	// 如果是 ShooterAIController，调用重新占据方法（这会重置 StateTree 和所有状态）
	if (AShooterAIController* AIController = Cast<AShooterAIController>(NPCController))
	{
		AIController->RequestRepossess(this);
		UE_LOG(LogTemp, Log, TEXT("AI Controller state reset for respawned NPC"));
	}
}

void AShooterNPC::StartShooting(AActor* ActorToShoot)
{
	// 检查 NPC 是否已死亡或无效（防止死亡状态下的 NPC 射击）
	if (bIsDead || !IsValid(this))
	{
		return;
	}
	
	// 检查武器是否有效
	if (!Weapon || !Weapon->IsValidLowLevel())
	{
		return;
	}

	// 保存瞄准目标
	CurrentAimTarget = ActorToShoot;

	// 设置射击标志
	bIsShooting = true;

	// 通知武器开始射击
	Weapon->StartFiring();
}

void AShooterNPC::StopShooting()
{
	// 清除射击标志
	bIsShooting = false;
	
	// 清除瞄准目标
	CurrentAimTarget = nullptr;

	// 通知武器停止射击（如果武器有效）
	if (Weapon && Weapon->IsValidLowLevel())
	{
		Weapon->StopFiring();
	}
}

void AShooterNPC::OnRep_CurrentHP()
{
	// Update visual effects or UI when HP changes on clients
	// This can be extended to show damage indicators, etc.
}

void AShooterNPC::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AShooterNPC, CurrentHP);
	DOREPLIFETIME(AShooterNPC, TeamByte);
	DOREPLIFETIME(AShooterNPC, bIsDead);
}
