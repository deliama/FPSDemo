// Copyright Epic Games, Inc. All Rights Reserved.


#include "ShooterCharacter.h"
#include "ShooterWeapon.h"
#include "Variant_Shooter/AI/ShooterNPC.h"
#include "EnhancedInputComponent.h"
#include "Components/InputComponent.h"
#include "Components/PawnNoiseEmitterComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Camera/CameraComponent.h"
#include "TimerManager.h"
#include "ShooterGameMode.h"
#include "ShooterUI.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/PlayerState.h"

AShooterCharacter::AShooterCharacter()
{
	// 创建噪音发射器组件：用于让 AI 感知玩家的位置（射击、移动等动作会产生噪音）
	PawnNoiseEmitter = CreateDefaultSubobject<UPawnNoiseEmitterComponent>(TEXT("Pawn Noise Emitter"));

	// 配置角色移动：设置旋转速率（每秒 600 度）
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 600.0f, 0.0f);

	// 启用网络复制：允许角色在多人游戏中同步
	bReplicates = true;
	SetReplicateMovement(true);  // 复制角色移动
}

void AShooterCharacter::BeginPlay()
{
	Super::BeginPlay();

	// 初始化生命值为最大值
	CurrentHP = MaxHP;

	// 服务器端：启动重生后的无敌时间（防止刚重生就被秒杀）
	if (HasAuthority())
	{
		bIsInvulnerable = true;
		GetWorld()->GetTimerManager().SetTimer(InvulnerabilityTimer, this, &AShooterCharacter::OnInvulnerabilityExpired, InvulnerabilityDuration, false);
	}

	// 调试：记录角色的团队 ID
	UE_LOG(LogTemp, Warning, TEXT("[ShooterCharacter::BeginPlay] Character spawned - TeamByte: %d, Character: %s"), 
		TeamByte, *GetNameSafe(this));
	if (AController* Ctrl = GetController())
	{
		UE_LOG(LogTemp, Warning, TEXT("[ShooterCharacter::BeginPlay] Character controller: %s"), *GetNameSafe(Ctrl));
		if (APlayerController* PC = Cast<APlayerController>(Ctrl))
		{
			UE_LOG(LogTemp, Warning, TEXT("[ShooterCharacter::BeginPlay] Character is controlled by PlayerController"));
		}
	}

	// 更新 HUD：通知 UI 更新生命值显示（1.0 = 100% 生命值）
	OnDamaged.Broadcast(1.0f);
}

void AShooterCharacter::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	// clear the respawn timer
	GetWorld()->GetTimerManager().ClearTimer(RespawnTimer);

	// clear the invulnerability timer
	GetWorld()->GetTimerManager().ClearTimer(InvulnerabilityTimer);
}

void AShooterCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// base class handles move, aim and jump inputs
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// Firing
		EnhancedInputComponent->BindAction(FireAction, ETriggerEvent::Started, this, &AShooterCharacter::DoStartFiring);
		EnhancedInputComponent->BindAction(FireAction, ETriggerEvent::Completed, this, &AShooterCharacter::DoStopFiring);

		// Switch weapon
		EnhancedInputComponent->BindAction(SwitchWeaponAction, ETriggerEvent::Triggered, this, &AShooterCharacter::DoSwitchWeapon);

		// Reload
		EnhancedInputComponent->BindAction(ReloadAction, ETriggerEvent::Triggered, this, &AShooterCharacter::DoReload);
	}

}

float AShooterCharacter::TakeDamage(float Damage, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	// 仅在服务器端处理伤害（服务器权威，防止作弊）
	if (!HasAuthority())
	{
		return 0.0f;
	}

	// 如果已经死亡，忽略伤害
	if (CurrentHP <= 0.0f)
	{
		return 0.0f;
	}

	// 如果处于无敌状态，忽略伤害（如重生后的无敌时间）
	if (bIsInvulnerable)
	{
		return 0.0f;
	}

	// 记录最后造成伤害的控制器（用于击杀统计）
	if (EventInstigator)
	{
		LastDamageInstigator = EventInstigator;
	}

	// 减少生命值
	CurrentHP -= Damage;

	// 检查是否生命值耗尽，触发死亡
	if (CurrentHP <= 0.0f)
	{
		Die();
	}

	// 更新 HUD（服务器端立即更新，客户端通过 OnRep_CurrentHP 接收更新）
	OnDamaged.Broadcast(FMath::Max(0.0f, CurrentHP / MaxHP));

	return Damage;
}

void AShooterCharacter::DoStartFiring()
{
	// 客户端预测：立即本地执行射击（提供即时反馈，避免延迟感）
	if (CurrentWeapon)
	{
		CurrentWeapon->StartFiring();
	}
	
	// 服务器 RPC：同步射击到服务器（服务器会验证并执行，确保游戏逻辑一致性）
	ServerStartFiring();
}

void AShooterCharacter::DoStopFiring()
{
	// 客户端预测：立即本地停止射击
	if (CurrentWeapon)
	{
		CurrentWeapon->StopFiring();
	}
	
	// 服务器 RPC：同步停止射击到服务器
	ServerStopFiring();
}

void AShooterCharacter::DoSwitchWeapon()
{
	// ensure we have at least two weapons two switch between
	if (OwnedWeapons.Num() > 1)
	{
		// deactivate the old weapon
		CurrentWeapon->DeactivateWeapon();

		// find the index of the current weapon in the owned list
		int32 WeaponIndex = OwnedWeapons.Find(CurrentWeapon);

		// is this the last weapon?
		if (WeaponIndex == OwnedWeapons.Num() - 1)
		{
			// loop back to the beginning of the array
			WeaponIndex = 0;
		}
		else {
			// select the next weapon index
			++WeaponIndex;
		}

		// set the new weapon as current
		CurrentWeapon = OwnedWeapons[WeaponIndex];

		// activate the new weapon
		CurrentWeapon->ActivateWeapon();
	}
}

void AShooterCharacter::DoReload()
{
	// Try to reload locally for immediate feedback (will be validated on server)
	if (CurrentWeapon && CurrentWeapon->CanReload())
	{
		CurrentWeapon->StartReload();
	}

	// Server RPC for reloading
	ServerReload();
}

void AShooterCharacter::AttachWeaponMeshes(AShooterWeapon* Weapon)
{
	const FAttachmentTransformRules AttachmentRule(EAttachmentRule::SnapToTarget, false);

	// attach the weapon actor
	Weapon->AttachToActor(this, AttachmentRule);

	// attach the weapon meshes
	Weapon->GetFirstPersonMesh()->AttachToComponent(GetFirstPersonMesh(), AttachmentRule, FirstPersonWeaponSocket);
	Weapon->GetThirdPersonMesh()->AttachToComponent(GetMesh(), AttachmentRule, FirstPersonWeaponSocket);
	
}

void AShooterCharacter::PlayFiringMontage(UAnimMontage* Montage)
{
	
}

void AShooterCharacter::AddWeaponRecoil(float Recoil)
{
	// apply the recoil as pitch input
	AddControllerPitchInput(Recoil);
}

void AShooterCharacter::UpdateWeaponHUD(int32 CurrentAmmo, int32 MagazineSize)
{
	OnBulletCountUpdated.Broadcast(MagazineSize, CurrentAmmo);
}

FVector AShooterCharacter::GetWeaponTargetLocation()
{
	// trace ahead from the camera viewpoint
	FHitResult OutHit;

	const FVector Start = GetFirstPersonCameraComponent()->GetComponentLocation();
	const FVector End = Start + (GetFirstPersonCameraComponent()->GetForwardVector() * MaxAimDistance);

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	GetWorld()->LineTraceSingleByChannel(OutHit, Start, End, ECC_Visibility, QueryParams);

	// return either the impact point or the trace end
	return OutHit.bBlockingHit ? OutHit.ImpactPoint : OutHit.TraceEnd;
}

void AShooterCharacter::AddWeaponClass(const TSubclassOf<AShooterWeapon>& WeaponClass)
{
	// do we already own this weapon?
	AShooterWeapon* OwnedWeapon = FindWeaponOfType(WeaponClass);

	if (!OwnedWeapon)
	{
		// spawn the new weapon
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.Instigator = this;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnParams.TransformScaleMethod = ESpawnActorScaleMethod::MultiplyWithRoot;

		AShooterWeapon* AddedWeapon = GetWorld()->SpawnActor<AShooterWeapon>(WeaponClass, GetActorTransform(), SpawnParams);

		if (AddedWeapon)
		{
			// add the weapon to the owned list
			OwnedWeapons.Add(AddedWeapon);

			// if we have an existing weapon, deactivate it
			if (CurrentWeapon)
			{
				CurrentWeapon->DeactivateWeapon();
			}

			// switch to the new weapon
			CurrentWeapon = AddedWeapon;
			CurrentWeapon->ActivateWeapon();
		}
	}
}

void AShooterCharacter::OnWeaponActivated(AShooterWeapon* Weapon)
{
	// update the bullet counter
	OnBulletCountUpdated.Broadcast(Weapon->GetMagazineSize(), Weapon->GetBulletCount());

	// set the character mesh AnimInstances
	GetFirstPersonMesh()->SetAnimInstanceClass(Weapon->GetFirstPersonAnimInstanceClass());
	GetMesh()->SetAnimInstanceClass(Weapon->GetThirdPersonAnimInstanceClass());
}

void AShooterCharacter::OnWeaponDeactivated(AShooterWeapon* Weapon)
{
	// unused
}

void AShooterCharacter::OnSemiWeaponRefire()
{
	// unused
}

AShooterWeapon* AShooterCharacter::FindWeaponOfType(TSubclassOf<AShooterWeapon> WeaponClass) const
{
	// check each owned weapon
	for (AShooterWeapon* Weapon : OwnedWeapons)
	{
		if (Weapon->IsA(WeaponClass))
		{
			return Weapon;
		}
	}

	// weapon not found
	return nullptr;

}

void AShooterCharacter::Die()
{
	UE_LOG(LogTemp, Warning, TEXT("[ShooterCharacter::Die] Character died - Team: %d"), TeamByte);

	// 停用当前武器
	if (IsValid(CurrentWeapon))
	{
		CurrentWeapon->DeactivateWeapon();
	}

	// 记录击杀/死亡统计并更新 UI
	if (AShooterGameMode* GM = Cast<AShooterGameMode>(GetWorld()->GetAuthGameMode()))
	{
		UE_LOG(LogTemp, Warning, TEXT("[ShooterCharacter::Die] GameMode found, processing kill/death"));
		APlayerController* VictimPC = Cast<APlayerController>(GetController());
		APlayerController* KillerPC = nullptr;

		// 记录死亡统计（增加死亡数）
		if (VictimPC)
		{
			GM->RecordDeath(VictimPC);
		}

		// 记录击杀统计（增加击杀数）
		if (LastDamageInstigator)
		{
			KillerPC = Cast<APlayerController>(LastDamageInstigator);
			if (KillerPC)
			{
				GM->RecordKill(KillerPC);
			}
		}

		// 显示击杀信息和死亡界面
		if (GM && GM->ShooterUI)
		{
			FString KillerName = KillerPC ? KillerPC->GetPlayerState<APlayerState>()->GetPlayerName() : TEXT("Unknown");
			FString VictimName = VictimPC ? VictimPC->GetPlayerState<APlayerState>()->GetPlayerName() : TEXT("Unknown");

			// 向所有玩家显示击杀信息（击杀提示）
			GM->ShooterUI->BP_ShowKillFeed(KillerName, VictimName);

			// 仅向被击杀的玩家显示死亡界面
			if (VictimPC && VictimPC->IsLocalController())
			{
				GM->ShooterUI->BP_ShowDeathScreen(KillerName, RespawnTime);
			}
		}

		// 增加击杀者团队的得分
		// 获取击杀者的团队ID（从造成伤害的控制器获取其控制的角色的团队）
		uint8 KillerTeamByte = 255;
		FString KillerType = TEXT("Unknown");
		
		if (LastDamageInstigator && LastDamageInstigator->GetPawn())
		{
			UE_LOG(LogTemp, Warning, TEXT("[ShooterCharacter::Die] LastDamageInstigator: %s, Pawn: %s"), 
				*GetNameSafe(LastDamageInstigator), *GetNameSafe(LastDamageInstigator->GetPawn()));
			
			// 尝试从击杀者角色获取团队ID
			if (AShooterCharacter* KillerCharacter = Cast<AShooterCharacter>(LastDamageInstigator->GetPawn()))
			{
				KillerTeamByte = KillerCharacter->GetTeamByte();
				KillerType = TEXT("ShooterCharacter");
				UE_LOG(LogTemp, Warning, TEXT("[ShooterCharacter::Die] Killer is ShooterCharacter, TeamByte: %d"), KillerTeamByte);
			}
			// 如果是AI NPC击杀的，也获取其团队ID
			else if (AShooterNPC* KillerNPC = Cast<AShooterNPC>(LastDamageInstigator->GetPawn()))
			{
				KillerTeamByte = KillerNPC->GetTeamByte();
				KillerType = TEXT("ShooterNPC");
				UE_LOG(LogTemp, Warning, TEXT("[ShooterCharacter::Die] Killer is ShooterNPC, TeamByte: %d"), KillerTeamByte);
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[ShooterCharacter::Die] LastDamageInstigator or Pawn is null"));
		}

		UE_LOG(LogTemp, Warning, TEXT("[ShooterCharacter::Die] ===== KILL SCORE LOGIC ====="));
		UE_LOG(LogTemp, Warning, TEXT("[ShooterCharacter::Die] Victim TeamByte: %d, Killer TeamByte: %d, Killer Type: %s"), 
			TeamByte, KillerTeamByte, *KillerType);
		UE_LOG(LogTemp, Warning, TEXT("[ShooterCharacter::Die] KillerTeamByte != 255: %d"), (KillerTeamByte != 255));
		UE_LOG(LogTemp, Warning, TEXT("[ShooterCharacter::Die] KillerTeamByte != TeamByte: %d"), (KillerTeamByte != TeamByte));

		// 只有当击杀者和被击杀者属于不同团队时才增加得分
		if (KillerTeamByte != 255 && KillerTeamByte != TeamByte)
		{
			UE_LOG(LogTemp, Warning, TEXT("[ShooterCharacter::Die] ✓ Incrementing score for team %d (killer) vs team %d (victim)"), 
				KillerTeamByte, TeamByte);
			GM->IncrementTeamScore(KillerTeamByte);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[ShooterCharacter::Die] ✗ Not incrementing score - KillerTeam: %d, VictimTeam: %d"), 
				KillerTeamByte, TeamByte);
		}
		UE_LOG(LogTemp, Warning, TEXT("[ShooterCharacter::Die] ============================="));
	}
		
	// 立即停止角色移动
	GetCharacterMovement()->StopMovementImmediately();

	// 禁用输入控制
	DisableInput(nullptr);

	// 重置弹药计数器 UI
	OnBulletCountUpdated.Broadcast(0, 0);

	// 调用蓝图死亡处理（可在蓝图中添加死亡特效、音效等）
	BP_OnDeath();

	// 设置重生定时器：等待 RespawnTime 秒后重生
	GetWorld()->GetTimerManager().SetTimer(RespawnTimer, this, &AShooterCharacter::OnRespawn, RespawnTime, false);
}

void AShooterCharacter::OnRespawn()
{
	// Hide death screen before respawning
	if (AShooterGameMode* GM = Cast<AShooterGameMode>(GetWorld()->GetAuthGameMode()))
	{
		if (GM && GM->ShooterUI)
		{
			if (APlayerController* PC = Cast<APlayerController>(GetController()))
			{
				if (PC->IsLocalController())
				{
					GM->ShooterUI->BP_HideDeathScreen();
				}
			}
		}
	}

	// destroy the character to force the PC to respawn
	Destroy();
}

void AShooterCharacter::OnInvulnerabilityExpired()
{
	// Only process on server
	if (!HasAuthority())
	{
		return;
	}

	// Clear invulnerability flag
	bIsInvulnerable = false;
}

void AShooterCharacter::OnRep_IsInvulnerable()
{
	// Update visual feedback when invulnerability state changes
	// This can be used in Blueprint to show invulnerability effects (e.g., flashing)
}

void AShooterCharacter::OnRep_CurrentHP()
{
	// 客户端接收生命值更新后，更新 HUD（生命值条、伤害效果等）
	// 计算生命值百分比（0.0 - 1.0），通知 UI 更新
	OnDamaged.Broadcast(FMath::Max(0.0f, CurrentHP / MaxHP));
}

void AShooterCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AShooterCharacter, CurrentHP);
	DOREPLIFETIME(AShooterCharacter, TeamByte);
	DOREPLIFETIME(AShooterCharacter, bIsInvulnerable);
}

void AShooterCharacter::ServerStartFiring_Implementation()
{
	// 服务器端执行射击（服务器权威，确保所有客户端看到一致的射击行为）
	if (CurrentWeapon)
	{
		CurrentWeapon->StartFiring();
	}
}

bool AShooterCharacter::ServerStartFiring_Validate()
{
	// RPC 验证函数：可以在这里添加反作弊检查（如射速限制、距离检查等）
	return true;
}

void AShooterCharacter::ServerStopFiring_Implementation()
{
	// 服务器端停止射击
	if (CurrentWeapon)
	{
		CurrentWeapon->StopFiring();
	}
}

bool AShooterCharacter::ServerStopFiring_Validate()
{
	// RPC 验证函数
	return true;
}

void AShooterCharacter::ServerReload_Implementation()
{
	// 服务器端执行换弹（服务器验证是否可以换弹）
	if (CurrentWeapon && CurrentWeapon->CanReload())
	{
		CurrentWeapon->StartReload();
	}
}

bool AShooterCharacter::ServerReload_Validate()
{
	// RPC 验证函数：可以添加换弹频率限制等反作弊检查
	return true;
}
