// Copyright Epic Games, Inc. All Rights Reserved.


#include "ShooterCharacter.h"
#include "ShooterWeapon.h"
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
	// create the noise emitter component
	PawnNoiseEmitter = CreateDefaultSubobject<UPawnNoiseEmitterComponent>(TEXT("Pawn Noise Emitter"));

	// configure movement
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 600.0f, 0.0f);

	// Enable replication
	bReplicates = true;
	SetReplicateMovement(true);
}

void AShooterCharacter::BeginPlay()
{
	Super::BeginPlay();

	// reset HP to max
	CurrentHP = MaxHP;

	// Start invulnerability period after spawn
	if (HasAuthority())
	{
		bIsInvulnerable = true;
		GetWorld()->GetTimerManager().SetTimer(InvulnerabilityTimer, this, &AShooterCharacter::OnInvulnerabilityExpired, InvulnerabilityDuration, false);
	}

	// update the HUD
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
	// Only process damage on server
	if (!HasAuthority())
	{
		return 0.0f;
	}

	// ignore if already dead
	if (CurrentHP <= 0.0f)
	{
		return 0.0f;
	}

	// ignore damage if invulnerable
	if (bIsInvulnerable)
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

	// update the HUD (will also be called via OnRep_CurrentHP on clients)
	OnDamaged.Broadcast(FMath::Max(0.0f, CurrentHP / MaxHP));

	return Damage;
}

void AShooterCharacter::DoStartFiring()
{
	// Fire locally for immediate feedback
	if (CurrentWeapon)
	{
		CurrentWeapon->StartFiring();
	}
	
	// Server RPC for firing (always call, it will validate on server)
	ServerStartFiring();
}

void AShooterCharacter::DoStopFiring()
{
	// Stop firing locally for immediate feedback
	if (CurrentWeapon)
	{
		CurrentWeapon->StopFiring();
	}
	
	// Server RPC for stopping fire (always call, it will validate on server)
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
	// deactivate the weapon
	if (IsValid(CurrentWeapon))
	{
		CurrentWeapon->DeactivateWeapon();
	}

	// Record statistics and update UI
	if (AShooterGameMode* GM = Cast<AShooterGameMode>(GetWorld()->GetAuthGameMode()))
	{
		APlayerController* VictimPC = Cast<APlayerController>(GetController());
		APlayerController* KillerPC = nullptr;

		// Record death for this player
		if (VictimPC)
		{
			GM->RecordDeath(VictimPC);
		}

		// Record kill for the killer
		if (LastDamageInstigator)
		{
			KillerPC = Cast<APlayerController>(LastDamageInstigator);
			if (KillerPC)
			{
				GM->RecordKill(KillerPC);
			}
		}

		// Show kill feed and death screen
		if (GM && GM->ShooterUI)
		{
			FString KillerName = KillerPC ? KillerPC->GetPlayerState<APlayerState>()->GetPlayerName() : TEXT("Unknown");
			FString VictimName = VictimPC ? VictimPC->GetPlayerState<APlayerState>()->GetPlayerName() : TEXT("Unknown");

			// Show kill feed for all players
			GM->ShooterUI->BP_ShowKillFeed(KillerName, VictimName);

			// Show death screen for the victim
			if (VictimPC && VictimPC->IsLocalController())
			{
				GM->ShooterUI->BP_ShowDeathScreen(KillerName, RespawnTime);
			}
		}

		// increment the team score
		GM->IncrementTeamScore(TeamByte);
	}
		
	// stop character movement
	GetCharacterMovement()->StopMovementImmediately();

	// disable controls
	DisableInput(nullptr);

	// reset the bullet counter UI
	OnBulletCountUpdated.Broadcast(0, 0);

	// call the BP handler
	BP_OnDeath();

	// schedule character respawn
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
	// update the HUD when HP changes
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
	// fire the current weapon on server
	if (CurrentWeapon)
	{
		CurrentWeapon->StartFiring();
	}
}

bool AShooterCharacter::ServerStartFiring_Validate()
{
	return true;
}

void AShooterCharacter::ServerStopFiring_Implementation()
{
	// stop firing the current weapon on server
	if (CurrentWeapon)
	{
		CurrentWeapon->StopFiring();
	}
}

bool AShooterCharacter::ServerStopFiring_Validate()
{
	return true;
}

void AShooterCharacter::ServerReload_Implementation()
{
	// Reload the current weapon on server
	if (CurrentWeapon && CurrentWeapon->CanReload())
	{
		CurrentWeapon->StartReload();
	}
}

bool AShooterCharacter::ServerReload_Validate()
{
	return true;
}
