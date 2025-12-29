// Copyright Epic Games, Inc. All Rights Reserved.


#include "Variant_Shooter/AI/ShooterNPC.h"
#include "ShooterWeapon.h"
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
	// ignore if already dead
	if (bIsDead)
	{
		return;
	}

	// raise the dead flag
	bIsDead = true;

	// Record statistics
	if (AShooterGameMode* GM = Cast<AShooterGameMode>(GetWorld()->GetAuthGameMode()))
	{
		// Record kill for the killer (NPCs don't have PlayerController, so only record if killer is a player)
		if (LastDamageInstigator)
		{
			if (APlayerController* KillerPC = Cast<APlayerController>(LastDamageInstigator))
			{
				GM->RecordKill(KillerPC);
			}
		}

		// increment the team score
		GM->IncrementTeamScore(TeamByte);
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
	// Reset health
	CurrentHP = 100.0f; // Or whatever max HP should be
	bIsDead = false;

	// Reset physics
	GetMesh()->SetSimulatePhysics(false);
	GetMesh()->SetCollisionProfileName(TEXT("Pawn"));
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	
	// Reset the physics transforms to match the skeletal mesh
	GetMesh()->ResetAllBodiesSimulatePhysics();
	
	// Reset movement
	GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_Walking);
	GetCharacterMovement()->bUseControllerDesiredRotation = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;

	// If we have a weapon, make sure it's properly attached and activated
	if (Weapon)
	{
		// First deactivate the current weapon if it was active
		OnWeaponDeactivated(Weapon);
		
		// Attach weapon meshes
		AttachWeaponMeshes(Weapon);
		
		// Activate the weapon properly
		OnWeaponActivated(Weapon);
	}
	
	// Reset any other state as needed
	bReplicates = true;
	SetReplicateMovement(true);
	
	// Ensure the NPC is properly enabled for input/behavior
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
	
	// Notify that respawn is complete
	OnAfterRespawn();
}

void AShooterNPC::OnAfterRespawn()
{
	// This method is called after the NPC respawns to allow any post-respawn setup
	// Try to get the AI controller to repossess this pawn
	if (AController* NPCController = GetController())
	{
		if (AShooterAIController* AIController = Cast<AShooterAIController>(NPCController))
		{
			AIController->RequestRepossess(this);
		}
	}
	else
	{
		// If we don't have an AI controller yet, try to get one assigned
		// This might happen if the AI controller was destroyed
		UE_LOG(LogTemp, Warning, TEXT("NPC respawned without AI controller - this may need manual repossessing"));
	}
}

void AShooterNPC::StartShooting(AActor* ActorToShoot)
{
	// save the aim target
	CurrentAimTarget = ActorToShoot;

	// raise the flag
	bIsShooting = true;

	// signal the weapon
	Weapon->StartFiring();
}

void AShooterNPC::StopShooting()
{
	// lower the flag
	bIsShooting = false;

	// signal the weapon
	Weapon->StopFiring();
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
