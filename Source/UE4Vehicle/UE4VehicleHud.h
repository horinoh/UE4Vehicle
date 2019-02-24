// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#pragma once
#include "GameFramework/HUD.h"
#include "UE4VehicleHud.generated.h"


UCLASS(config = Game)
class AUE4VehicleHud : public AHUD
{
	GENERATED_BODY()

public:
	AUE4VehicleHud();

	/** Font used to render the vehicle info */
	UPROPERTY()
	UFont* HUDFont;

	// Begin AHUD interface
	virtual void DrawHUD() override;
	// End AHUD interface
};
