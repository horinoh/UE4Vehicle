// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "UE4VehicleGameMode.h"
#include "UE4VehiclePawn.h"
#include "UE4VehicleHud.h"

AUE4VehicleGameMode::AUE4VehicleGameMode()
{
	DefaultPawnClass = AUE4VehiclePawn::StaticClass();
	HUDClass = AUE4VehicleHud::StaticClass();
}
