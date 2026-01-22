// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FPSDemoCharacter.h"
#include "ShooterWeaponHolder.h"
#include "ShooterNPC.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPawnDeathDelegate);

class AShooterWeapon;

/**
 *  AI 控制的射击游戏 NPC 敌人
 * 功能：
 *  - 通过 StateTree（状态树）执行 AI 行为（由 AI Controller 管理）
 *  - 持有武器并可以攻击玩家
 *  - 生命值系统：可被玩家击败
 *  - 可选的重生机制
 *  - 网络同步生命值和状态
 */
UCLASS(abstract)
class FPSDEMO_API AShooterNPC : public AFPSDemoCharacter, public IShooterWeaponHolder
{
	GENERATED_BODY()

public:

	/** NPC 当前生命值（降至 0 时死亡，网络同步） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_CurrentHP, Category="Damage")
	float CurrentHP = 100.0f;
	
	/** 是否允许 NPC 死亡后重生 */
	UPROPERTY(EditAnywhere, Category="Respawn")
	bool bCanRespawn = false;

	/** NPC 重生等待时间（秒，0 = 不重生） */
	UPROPERTY(EditAnywhere, Category="Respawn")
	float RespawnTime = 0.0f; // 默认为 0（不重生）以保持当前行为

protected:

	/** Name of the collision profile to use during ragdoll death */
	UPROPERTY(EditAnywhere, Category="Damage")
	FName RagdollCollisionProfile = FName("Ragdoll");

	/** Time to wait after death before destroying this actor */
	UPROPERTY(EditAnywhere, Category="Damage")
	float DeferredDestructionTime = 5.0f;

	/** NPC 所属团队 ID（用于团队识别，默认 1 = 敌人团队） */
	UPROPERTY(EditAnywhere, Replicated, Category="Team")
	uint8 TeamByte = 1;

	/** NPC 装备的武器指针 */
	TObjectPtr<AShooterWeapon> Weapon;

	/** NPC 使用的武器类型（在 BeginPlay 时生成） */
	UPROPERTY(EditAnywhere, Category="Weapon")
	TSubclassOf<AShooterWeapon> WeaponClass;

	/** 第一人称网格武器插槽名称 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category ="Weapons")
	FName FirstPersonWeaponSocket = FName("HandGrip_R");

	/** 第三人称网格武器插槽名称 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category ="Weapons")
	FName ThirdPersonWeaponSocket = FName("HandGrip_R");

	/** 瞄准计算的最大距离（厘米） */
	UPROPERTY(EditAnywhere, Category="Aim")
	float AimRange = 10000.0f;

	/** 瞄准时的散布半角（度数，增加 AI 射击难度） */
	UPROPERTY(EditAnywhere, Category="Aim")
	float AimVarianceHalfAngle = 10.0f;

	/** 瞄准目标时的最小垂直偏移（从目标中心向下，使 NPC 不会总是爆头） */
	UPROPERTY(EditAnywhere, Category="Aim")
	float MinAimOffsetZ = -35.0f;

	/** 瞄准目标时的最大垂直偏移 */
	UPROPERTY(EditAnywhere, Category="Aim")
	float MaxAimOffsetZ = -60.0f;

	/** 当前正在瞄准的目标 Actor（通常是玩家） */
	TObjectPtr<AActor> CurrentAimTarget;

	/** 当前是否正在射击 */
	bool bIsShooting = false;

	/** If true, this character has already died */
	UPROPERTY(Replicated)
	bool bIsDead = false;

	/** Deferred destruction on death timer */
	FTimerHandle DeathTimer;

	/** Timer handle for respawn */
	FTimerHandle RespawnTimer;

	// 保存NPC出生时的Transform
	UPROPERTY(BlueprintReadOnly,Category="Shooter|AI")
	FTransform StartTransform;

	/** Controller that last damaged this NPC (for kill tracking) */
	TObjectPtr<AController> LastDamageInstigator;

public:

	/** Delegate called when this NPC dies */
	FPawnDeathDelegate OnPawnDeath;

protected:

	/** Gameplay initialization */
	virtual void BeginPlay() override;

	/** Gameplay cleanup */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:

	/** Handle incoming damage */
	virtual float TakeDamage(float Damage, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

public:

	//~Begin IShooterWeaponHolder interface

	/** Attaches a weapon's meshes to the owner */
	virtual void AttachWeaponMeshes(AShooterWeapon* Weapon) override;

	/** Plays the firing montage for the weapon */
	virtual void PlayFiringMontage(UAnimMontage* Montage) override;

	/** Applies weapon recoil to the owner */
	virtual void AddWeaponRecoil(float Recoil) override;

	/** Updates the weapon's HUD with the current ammo count */
	virtual void UpdateWeaponHUD(int32 CurrentAmmo, int32 MagazineSize) override;

	/** Calculates and returns the aim location for the weapon */
	virtual FVector GetWeaponTargetLocation() override;

	/** Gives a weapon of this class to the owner */
	virtual void AddWeaponClass(const TSubclassOf<AShooterWeapon>& WeaponClass) override;

	/** Activates the passed weapon */
	virtual void OnWeaponActivated(AShooterWeapon* Weapon) override;

	/** Deactivates the passed weapon */
	virtual void OnWeaponDeactivated(AShooterWeapon* Weapon) override;

	/** Notifies the owner that the weapon cooldown has expired and it's ready to shoot again */
	virtual void OnSemiWeaponRefire() override;

	//~End IShooterWeaponHolder interface

protected:

	/** Called when HP is depleted and the character should die */
	void Die();

	/** Called after death to destroy the actor */
	void DeferredDestruction();

	/** Replication function for CurrentHP */
	UFUNCTION()
	void OnRep_CurrentHP();

public:

	/** Signals this character to start shooting at the passed actor */
	void StartShooting(AActor* ActorToShoot);

	/** Signals this character to stop shooting */
	void StopShooting();

	/** Respawn this NPC */
	void Respawn();

	/** Called after respawn to notify the AI controller */
	void OnAfterRespawn();

	/** 获取 NPC 所属的团队 ID */
	UFUNCTION(BlueprintCallable, Category="Team")
	uint8 GetTeamByte() const { return TeamByte; }

	/** Get the lifetime replicated props */
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
