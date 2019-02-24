// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "UE4VehiclePawn.h"
#include "UE4VehicleWheelFront.h"
#include "UE4VehicleWheelRear.h"
#include "UE4VehicleHud.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "WheeledVehicleMovementComponent4W.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Engine.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/TextRenderComponent.h"
#include "Materials/Material.h"
#include "GameFramework/Controller.h"

#ifndef HMD_MODULE_INCLUDED
#define HMD_MODULE_INCLUDED 0
#endif

// Needed for VR Headset
#if HMD_MODULE_INCLUDED
#include "IXRTrackingSystem.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#endif // HMD_MODULE_INCLUDED

const FName AUE4VehiclePawn::LookUpBinding("LookUp");
const FName AUE4VehiclePawn::LookRightBinding("LookRight");

#define LOCTEXT_NAMESPACE "VehiclePawn"

AUE4VehiclePawn::AUE4VehiclePawn()
{
	// Car mesh
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> CarMesh(TEXT("/Game/Vehicle/Sedan/Sedan_SkelMesh.Sedan_SkelMesh"));
	GetMesh()->SetSkeletalMesh(CarMesh.Object);

	static ConstructorHelpers::FClassFinder<UObject> AnimBPClass(TEXT("/Game/Vehicle/Sedan/Sedan_AnimBP"));
	GetMesh()->SetAnimInstanceClass(AnimBPClass.Class);
	
	// Simulation
	UWheeledVehicleMovementComponent4W* Vehicle4W = CastChecked<UWheeledVehicleMovementComponent4W>(GetVehicleMovement());

	check(Vehicle4W->WheelSetups.Num() == 4);

	Vehicle4W->WheelSetups[0].WheelClass = UUE4VehicleWheelFront::StaticClass();
	Vehicle4W->WheelSetups[0].BoneName = FName("Wheel_Front_Left");
	Vehicle4W->WheelSetups[0].AdditionalOffset = FVector(0.f, -12.f, 0.f);

	Vehicle4W->WheelSetups[1].WheelClass = UUE4VehicleWheelFront::StaticClass();
	Vehicle4W->WheelSetups[1].BoneName = FName("Wheel_Front_Right");
	Vehicle4W->WheelSetups[1].AdditionalOffset = FVector(0.f, 12.f, 0.f);

	Vehicle4W->WheelSetups[2].WheelClass = UUE4VehicleWheelRear::StaticClass();
	Vehicle4W->WheelSetups[2].BoneName = FName("Wheel_Rear_Left");
	Vehicle4W->WheelSetups[2].AdditionalOffset = FVector(0.f, -12.f, 0.f);

	Vehicle4W->WheelSetups[3].WheelClass = UUE4VehicleWheelRear::StaticClass();
	Vehicle4W->WheelSetups[3].BoneName = FName("Wheel_Rear_Right");
	Vehicle4W->WheelSetups[3].AdditionalOffset = FVector(0.f, 12.f, 0.f);

	// Create a spring arm component
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm0"));
	SpringArm->TargetOffset = FVector(0.f, 0.f, 200.f);
	SpringArm->SetRelativeRotation(FRotator(-15.f, 0.f, 0.f));
	SpringArm->SetupAttachment(RootComponent);
	SpringArm->TargetArmLength = 600.0f;
	SpringArm->bEnableCameraRotationLag = true;
	SpringArm->CameraRotationLagSpeed = 7.f;
	SpringArm->bInheritPitch = false;
	SpringArm->bInheritRoll = false;

	// Create camera component 
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera0"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
	Camera->bUsePawnControlRotation = false;
	Camera->FieldOfView = 90.f;

	// Create In-Car camera component 
	InternalCameraOrigin = FVector(0.0f, -40.0f, 120.0f);

	InternalCameraBase = CreateDefaultSubobject<USceneComponent>(TEXT("InternalCameraBase"));
	InternalCameraBase->SetRelativeLocation(InternalCameraOrigin);
	InternalCameraBase->SetupAttachment(GetMesh());

	InternalCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("InternalCamera"));
	InternalCamera->bUsePawnControlRotation = false;
	InternalCamera->FieldOfView = 90.f;
	InternalCamera->SetupAttachment(InternalCameraBase);

	//Setup TextRenderMaterial
	static ConstructorHelpers::FObjectFinder<UMaterial> TextMaterial(TEXT("Material'/Engine/EngineMaterials/AntiAliasedTextMaterialTranslucent.AntiAliasedTextMaterialTranslucent'"));
	
	UMaterialInterface* Material = TextMaterial.Object;

	// Create text render component for in car speed display
	InCarSpeed = CreateDefaultSubobject<UTextRenderComponent>(TEXT("IncarSpeed"));
	InCarSpeed->SetTextMaterial(Material);
	InCarSpeed->SetRelativeLocation(FVector(70.0f, -75.0f, 99.0f));
	InCarSpeed->SetRelativeRotation(FRotator(18.0f, 180.0f, 0.0f));
	InCarSpeed->SetupAttachment(GetMesh());
	InCarSpeed->SetRelativeScale3D(FVector(1.0f, 0.4f, 0.4f));

	// Create text render component for in car gear display
	InCarGear = CreateDefaultSubobject<UTextRenderComponent>(TEXT("IncarGear"));
	InCarGear->SetTextMaterial(Material);
	InCarGear->SetRelativeLocation(FVector(66.0f, -9.0f, 95.0f));	
	InCarGear->SetRelativeRotation(FRotator(25.0f, 180.0f,0.0f));
	InCarGear->SetRelativeScale3D(FVector(1.0f, 0.4f, 0.4f));
	InCarGear->SetupAttachment(GetMesh());
	
	// Colors for the incar gear display. One for normal one for reverse
	GearDisplayReverseColor = FColor(255, 0, 0, 255);
	GearDisplayColor = FColor(255, 255, 255, 255);

	// Colors for the in-car gear display. One for normal one for reverse
	GearDisplayReverseColor = FColor(255, 0, 0, 255);
	GearDisplayColor = FColor(255, 255, 255, 255);

	bInReverseGear = false;

#pragma region AddedCode
	//!< トランスミッションをマニュアルにする Change transmission to manual
	Vehicle4W->TransmissionSetup.bUseGearAutoBox = false;
	Vehicle4W->TransmissionSetup.GearSwitchTime = 0.15f;
	Vehicle4W->TransmissionSetup.GearAutoBoxLatency = 1.0f;

	//!< タイヤ負荷
	//Vehicle4W->MinNormalizedTireLoad = 0.0f;
	//Vehicle4W->MinNormalizedTireLoadFiltered = 0.2f;
	//Vehicle4W->MaxNormalizedTireLoad = 2.0f;
	//Vehicle4W->MaxNormalizedTireLoadFiltered = 2.0f;

	//!< エンジントルク
	//Vehicle4W->MaxEngineRPM = 5700.0f;
	//const auto EngineCurve = Vehicle4W->EngineSetup.TorqueCurve.GetRichCurve();
	//if (nullptr != EngineCurve)
	//{
	//	EngineCurve->Reset();
	//	EngineCurve->AddKey(0.0f, 400.0f);
	//	EngineCurve->AddKey(1890.0f, 500.0f);
	//	EngineCurve->AddKey(5730.0f, 400.0f);
	//}

	//!< ステアリング 
	//const auto SteerCurve = Vehicle4W->SteeringCurve.GetRichCurve();
	//if (nullptr != SteerCurve)
	//{
	//	SteerCurve->Reset();
	//	SteerCurve->AddKey(0.0f, 1.0f);
	//	SteerCurve->AddKey(40.0f, 0.7f);
	//	SteerCurve->AddKey(120.0f, 0.6f);
	//}	

	//!< 駆動輪
#if 1
	//Vehicle4W->DifferentialSetup.DifferentialType = EVehicleDifferential4W::LimitedSlip_4W;
	//Vehicle4W->DifferentialSetup.FrontRearSplit = 0.65; //!< 前輪を後輪よりも少しだけ駆動させる Drive the front wheels a little more than the rear
#elif 1
	//Vehicle4W->DifferentialSetup.DifferentialType = EVehicleDifferential4W::LimitedSlip_RearDrive;
#else
	//Vehicle4W->DifferentialSetup.DifferentialType = EVehicleDifferential4W::LimitedSlip_FrontDrive;
#endif

	//!< 重心のオフセット
	//const auto UpdatedPrimitive = Cast<UPrimitiveComponent>(Vehicle4W->UpdatedComponent);
	//if (UpdatedPrimitive)
	//{
	//	UpdatedPrimitive->BodyInstance.COMNudge = FVector(8.0f, 0.0f, 0.0f);
	//}

	//!< 慣性 質量の分散度合い
	//Vehicle4W->InertiaTensorScale = FVector(1.0f, 1.333f, 1.2f);
#pragma endregion
}

void AUE4VehiclePawn::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// set up gameplay key bindings
	check(PlayerInputComponent);

	PlayerInputComponent->BindAxis("MoveForward", this, &AUE4VehiclePawn::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &AUE4VehiclePawn::MoveRight);
	PlayerInputComponent->BindAxis("LookUp");
	PlayerInputComponent->BindAxis("LookRight");

	PlayerInputComponent->BindAction("Handbrake", IE_Pressed, this, &AUE4VehiclePawn::OnHandbrakePressed);
	PlayerInputComponent->BindAction("Handbrake", IE_Released, this, &AUE4VehiclePawn::OnHandbrakeReleased);
	PlayerInputComponent->BindAction("SwitchCamera", IE_Pressed, this, &AUE4VehiclePawn::OnToggleCamera);

	PlayerInputComponent->BindAction("ResetVR", IE_Pressed, this, &AUE4VehiclePawn::OnResetVR); 

#pragma region AddedCode
	PlayerInputComponent->BindAction("GearUp", IE_Pressed, this, &AUE4VehiclePawn::OnGearUp);
	PlayerInputComponent->BindAction("GearDown", IE_Pressed, this, &AUE4VehiclePawn::OnGearDown);
	PlayerInputComponent->BindAction("GearR", IE_Pressed, this, &AUE4VehiclePawn::OnGearR);
	PlayerInputComponent->BindAction("GearN", IE_Pressed, this, &AUE4VehiclePawn::OnGearN);
	PlayerInputComponent->BindAction("Gear1", IE_Pressed, this, &AUE4VehiclePawn::OnGear1);
	PlayerInputComponent->BindAction("Gear2", IE_Pressed, this, &AUE4VehiclePawn::OnGear2);
	PlayerInputComponent->BindAction("Gear3", IE_Pressed, this, &AUE4VehiclePawn::OnGear3);
	PlayerInputComponent->BindAction("Gear4", IE_Pressed, this, &AUE4VehiclePawn::OnGear4);
	PlayerInputComponent->BindAction("Gear5", IE_Pressed, this, &AUE4VehiclePawn::OnGear5);
	PlayerInputComponent->BindAction("Gear6", IE_Pressed, this, &AUE4VehiclePawn::OnGear6);
	PlayerInputComponent->BindAction("Gear7", IE_Pressed, this, &AUE4VehiclePawn::OnGear7);
#pragma  endregion
}

void AUE4VehiclePawn::MoveForward(float Val)
{
	GetVehicleMovementComponent()->SetThrottleInput(Val);
}

void AUE4VehiclePawn::MoveRight(float Val)
{
	GetVehicleMovementComponent()->SetSteeringInput(Val);
}

void AUE4VehiclePawn::OnHandbrakePressed()
{
	GetVehicleMovementComponent()->SetHandbrakeInput(true);
}

void AUE4VehiclePawn::OnHandbrakeReleased()
{
	GetVehicleMovementComponent()->SetHandbrakeInput(false);
}

void AUE4VehiclePawn::OnToggleCamera()
{
	EnableIncarView(!bInCarCameraActive);
}

void AUE4VehiclePawn::EnableIncarView(const bool bState, const bool bForce)
{
	if ((bState != bInCarCameraActive) || ( bForce == true ))
	{
		bInCarCameraActive = bState;
		
		if (bState == true)
		{
			OnResetVR();
			Camera->Deactivate();
			InternalCamera->Activate();
		}
		else
		{
			InternalCamera->Deactivate();
			Camera->Activate();
		}
		
		InCarSpeed->SetVisibility(bInCarCameraActive);
		InCarGear->SetVisibility(bInCarCameraActive);
	}
}


void AUE4VehiclePawn::Tick(float Delta)
{
	Super::Tick(Delta);

	// Setup the flag to say we are in reverse gear
	bInReverseGear = GetVehicleMovement()->GetCurrentGear() < 0;
	
	// Update the strings used in the hud (incar and onscreen)
	UpdateHUDStrings();

	// Set the string in the incar hud
	SetupInCarHUD();

	bool bHMDActive = false;
#if HMD_MODULE_INCLUDED
	if ((GEngine->XRSystem.IsValid() == true) && ((GEngine->XRSystem->IsHeadTrackingAllowed() == true) || (GEngine->IsStereoscopic3D() == true)))
	{
		bHMDActive = true;
	}
#endif // HMD_MODULE_INCLUDED
	if (bHMDActive == false)
	{
		if ( (InputComponent) && (bInCarCameraActive == true ))
		{
			FRotator HeadRotation = InternalCamera->RelativeRotation;
			HeadRotation.Pitch += InputComponent->GetAxisValue(LookUpBinding);
			HeadRotation.Yaw += InputComponent->GetAxisValue(LookRightBinding);
			InternalCamera->RelativeRotation = HeadRotation;
		}
	}

#pragma region AddedCode
	const auto RPM = GetVehicleMovement()->GetEngineRotationSpeed() / GetVehicleMovement()->GetEngineMaxRotationSpeed();
#pragma endregion
}

void AUE4VehiclePawn::BeginPlay()
{
	Super::BeginPlay();

	bool bEnableInCar = false;
#if HMD_MODULE_INCLUDED
	bEnableInCar = UHeadMountedDisplayFunctionLibrary::IsHeadMountedDisplayEnabled();
#endif // HMD_MODULE_INCLUDED
	EnableIncarView(bEnableInCar,true);
}

void AUE4VehiclePawn::OnResetVR()
{
#if HMD_MODULE_INCLUDED
	if (GEngine->XRSystem.IsValid())
	{
		GEngine->XRSystem->ResetOrientationAndPosition();
		InternalCamera->SetRelativeLocation(InternalCameraOrigin);
		GetController()->SetControlRotation(FRotator());
	}
#endif // HMD_MODULE_INCLUDED
}

#pragma region AddedCode
void AUE4VehiclePawn::OnGearUp()
{
#if 1
	GetVehicleMovement()->SetTargetGear(FMath::Min(GetVehicleMovement()->GetCurrentGear() + 1, 7), true);
#else
	GetVehicleMovement()->SetGearUp(true); //!< これだと上手くいかない…
#endif
}
void AUE4VehiclePawn::OnGearDown()
{
#if 1
	GetVehicleMovement()->SetTargetGear(FMath::Max(GetVehicleMovement()->GetCurrentGear() - 1, 0), true);
#else
	GetVehicleMovement()->SetGearDown(true); //!< これだと上手くいかない…
#endif
}
void AUE4VehiclePawn::OnTargetGear(int32 GearNum)
{
	GetVehicleMovement()->SetTargetGear(GearNum, true);
}
#pragma endregion

void AUE4VehiclePawn::UpdateHUDStrings()
{
	float KPH = FMath::Abs(GetVehicleMovement()->GetForwardSpeed()) * 0.036f;
	int32 KPH_int = FMath::FloorToInt(KPH);

	// Using FText because this is display text that should be localizable
	SpeedDisplayString = FText::Format(LOCTEXT("SpeedFormat", "{0} km/h"), FText::AsNumber(KPH_int));
	
	if (bInReverseGear == true)
	{
		GearDisplayString = FText(LOCTEXT("ReverseGear", "R"));
	}
	else
	{
		int32 Gear = GetVehicleMovement()->GetCurrentGear();
		GearDisplayString = (Gear == 0) ? LOCTEXT("N", "N") : FText::AsNumber(Gear);
	}	
}

void AUE4VehiclePawn::SetupInCarHUD()
{
	APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if ((PlayerController != nullptr) && (InCarSpeed != nullptr) && (InCarGear != nullptr) )
	{
		// Setup the text render component strings
		InCarSpeed->SetText(SpeedDisplayString);
		InCarGear->SetText(GearDisplayString);
		
		if (bInReverseGear == false)
		{
			InCarGear->SetTextRenderColor(GearDisplayColor);
		}
		else
		{
			InCarGear->SetTextRenderColor(GearDisplayReverseColor);
		}
	}
}

#undef LOCTEXT_NAMESPACE
