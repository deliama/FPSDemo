// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FPSDemoCharacter.h"
#include "ShooterWeaponHolder.h"
#include "ShooterTypes.h"
#include "ShooterCharacter.generated.h"

class AShooterWeapon;
class UInputAction;
class UInputComponent;
class UPawnNoiseEmitterComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBulletCountUpdatedDelegate, int32, MagazineSize, int32, Bullets);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDamagedDelegate, float, LifePercent);

/**
 *  可玩家控制的射击游戏角色类
 *  功能包括：
 *  - 武器系统：通过 IShooterWeaponHolder 接口管理武器库存
 *  - 生命值系统：HP 管理、受伤、死亡、重生
 *  - 网络同步：支持多人网络对战（客户端-服务器架构）
 *  - 团队系统：支持团队对战和得分统计
 *  - AI 感知：通过噪音发射器让 AI 敌人能够感知玩家
 */
UCLASS(abstract)
class FPSDEMO_API AShooterCharacter : public AFPSDemoCharacter, public IShooterWeaponHolder
{
	GENERATED_BODY()
	
	/** AI 噪音发射器组件：用于让 AI 感知玩家的位置（如射击、移动产生的噪音） */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UPawnNoiseEmitterComponent* PawnNoiseEmitter;

protected:

	/** 射击输入动作 */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* FireAction;

	/** 切换武器输入动作 */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* SwitchWeaponAction;

	/** 换弹输入动作 */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* ReloadAction;

	/** Name of the first person mesh weapon socket */
	UPROPERTY(EditAnywhere, Category ="Weapons")
	FName FirstPersonWeaponSocket = FName("HandGrip_R");

	/** Name of the third person mesh weapon socket */
	UPROPERTY(EditAnywhere, Category ="Weapons")
	FName ThirdPersonWeaponSocket = FName("HandGrip_R");

	/** Max distance to use for aim traces */
	UPROPERTY(EditAnywhere, Category ="Aim", meta = (ClampMin = 0, ClampMax = 100000, Units = "cm"))
	float MaxAimDistance = 10000.0f;

	/** 角色的最大生命值 */
	UPROPERTY(EditAnywhere, Category="Health")
	float MaxHP = 500.0f;

	/** 当前剩余生命值（网络同步属性，服务器权威，客户端通过 OnRep_CurrentHP 接收更新） */
	UPROPERTY(ReplicatedUsing=OnRep_CurrentHP, BlueprintReadOnly, Category="Health")
	float CurrentHP = 0.0f;

	/** 角色所属的团队 ID（用于团队对战和得分统计） */
	UPROPERTY(EditAnywhere, Replicated, Category="Team")
	uint8 TeamByte = 0;

public:

	/** 获取角色所属的团队 ID */
	UFUNCTION(BlueprintCallable, Category="Team")
	uint8 GetTeamByte() const { return TeamByte; }

protected:

	/** List of weapons picked up by the character */
	TArray<AShooterWeapon*> OwnedWeapons;

	/** Weapon currently equipped and ready to shoot with */
	TObjectPtr<AShooterWeapon> CurrentWeapon;

	UPROPERTY(EditAnywhere, Category ="Destruction", meta = (ClampMin = 0, ClampMax = 10, Units = "s"))
	float RespawnTime = 5.0f;

	FTimerHandle RespawnTimer;

	/** 重生后的无敌时间（秒） */
	UPROPERTY(EditAnywhere, Category="Health", meta = (ClampMin = 0, ClampMax = 10, Units = "s"))
	float InvulnerabilityDuration = 3.0f;

	/** 当前是否处于无敌状态（网络同步） */
	UPROPERTY(ReplicatedUsing=OnRep_IsInvulnerable, BlueprintReadOnly, Category="Health")
	bool bIsInvulnerable = false;

	/** 无敌状态定时器句柄 */
	FTimerHandle InvulnerabilityTimer;

	/** 最后对角色造成伤害的控制器（用于击杀统计） */
	TObjectPtr<AController> LastDamageInstigator;

public:

	/** Bullet count updated delegate */
	FBulletCountUpdatedDelegate OnBulletCountUpdated;

	/** Damaged delegate */
	FDamagedDelegate OnDamaged;

public:

	/** Constructor */
	AShooterCharacter();

protected:

	/** Gameplay initialization */
	virtual void BeginPlay() override;

	/** Gameplay cleanup */
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

	/** Set up input action bindings */
	virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;

public:

	/** 处理受到的伤害（服务器端权威计算） */
	virtual float TakeDamage(float Damage, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

public:

	/** 开始射击（客户端调用，本地立即反馈 + 服务器 RPC） */
	UFUNCTION(BlueprintCallable, Category="Input")
	void DoStartFiring();

	/** 停止射击（客户端调用，本地立即反馈 + 服务器 RPC） */
	UFUNCTION(BlueprintCallable, Category="Input")
	void DoStopFiring();

	/** 切换武器 */
	UFUNCTION(BlueprintCallable, Category="Input")
	void DoSwitchWeapon();

	/** 换弹 */
	UFUNCTION(BlueprintCallable, Category="Input")
	void DoReload();

	/** 服务器 RPC：开始射击（客户端-服务器网络同步） */
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerStartFiring();

	/** 服务器 RPC：停止射击（客户端-服务器网络同步） */
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerStopFiring();

	/** 服务器 RPC：换弹（客户端-服务器网络同步） */
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerReload();

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

	/** Returns true if the character already owns a weapon of the given class */
	AShooterWeapon* FindWeaponOfType(TSubclassOf<AShooterWeapon> WeaponClass) const;

	/** 角色死亡处理：禁用输入、停止移动、记录击杀/死亡统计、触发重生 */
	void Die();

	/** 蓝图可实现的死亡事件（允许在蓝图中添加死亡特效、音效等） */
	UFUNCTION(BlueprintImplementableEvent, Category="Shooter", meta = (DisplayName = "On Death"))
	void BP_OnDeath();

	/** 重生处理：销毁当前角色，强制 PlayerController 重新生成角色 */
	void OnRespawn();

	/** 生命值复制回调函数：当服务器同步 CurrentHP 到客户端时调用（更新 UI、特效等） */
	UFUNCTION()
	void OnRep_CurrentHP();

	/** 无敌状态复制回调函数：当服务器同步 bIsInvulnerable 到客户端时调用 */
	UFUNCTION()
	void OnRep_IsInvulnerable();

	/** 无敌时间结束回调 */
	void OnInvulnerabilityExpired();

public:

	/** Get the lifetime replicated props */
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
