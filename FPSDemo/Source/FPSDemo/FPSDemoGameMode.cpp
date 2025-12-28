// Copyright Epic Games, Inc. All Rights Reserved.

#include "FPSDemoGameMode.h"
#include "Engine/World.h"

AFPSDemoGameMode::AFPSDemoGameMode()
{
	// Enable replication for multiplayer
	bReplicates = true;
}

void AFPSDemoGameMode::BeginPlay()
{
	Super::BeginPlay();
}
