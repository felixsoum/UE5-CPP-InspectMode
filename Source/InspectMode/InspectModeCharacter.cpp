// Copyright Epic Games, Inc. All Rights Reserved.

#include "InspectModeCharacter.h"
#include "InspectModeProjectile.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "MovieSceneTracksComponentTypes.h"
#include "PlayerWidget.h"
#include "Blueprint/UserWidget.h"
#include "Engine/LocalPlayer.h"
#include "Runtime/Media/Public/IMediaControls.h"

DEFINE_LOG_CATEGORY(LogTemplateCharacter);

//////////////////////////////////////////////////////////////////////////
// AInspectModeCharacter

AInspectModeCharacter::AInspectModeCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(55.f, 96.0f);

	// Create a CameraComponent	
	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCameraComponent->SetupAttachment(GetCapsuleComponent());
	FirstPersonCameraComponent->SetRelativeLocation(FVector(-10.f, 0.f, 60.f)); // Position the camera
	FirstPersonCameraComponent->bUsePawnControlRotation = true;

	InspectOrigin = CreateDefaultSubobject<USceneComponent>(TEXT("InspectOrigin"));
	InspectOrigin->SetupAttachment(FirstPersonCameraComponent);

	// Create a mesh component that will be used when being viewed from a '1st person' view (when controlling this pawn)
	Mesh1P = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CharacterMesh1P"));
	Mesh1P->SetOnlyOwnerSee(true);
	Mesh1P->SetupAttachment(FirstPersonCameraComponent);
	Mesh1P->bCastDynamicShadow = false;
	Mesh1P->CastShadow = false;
	//Mesh1P->SetRelativeRotation(FRotator(0.9f, -19.19f, 5.2f));
	Mesh1P->SetRelativeLocation(FVector(-30.f, 0.f, -150.f));
}

void AInspectModeCharacter::BeginPlay()
{
	// Call the base class  
	Super::BeginPlay();

	auto UserWidget = CreateWidget<UUserWidget>(GetWorld(), PlayerWidgetClass);
	PlayerWidget = Cast<UPlayerWidget>(UserWidget);
	PlayerWidget->AddToViewport();
}

void AInspectModeCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!IsInspecting)
	{
		// https://dev.epicgames.com/community/snippets/2rR/simple-c-line-trace-collision-query
		FHitResult Hit;

		FVector Start = FirstPersonCameraComponent->GetComponentLocation();
		FVector End = Start + FirstPersonCameraComponent->GetForwardVector() * 5000.f;

		FCollisionObjectQueryParams ObjectQueryParams;
		ObjectQueryParams.AddObjectTypesToQuery(ECollisionChannel::ECC_WorldDynamic);

		FCollisionQueryParams QueryParams;
		QueryParams.AddIgnoredActor(this);

		bool IsHit = GetWorld()->LineTraceSingleByObjectType(Hit, Start, End, ObjectQueryParams, QueryParams);

		if (IsHit && IsValid(Hit.GetActor()))
		{
			CurrentInspectActor = Hit.GetActor();
			PlayerWidget->SetPromptF(true);
		}
		else
		{
			CurrentInspectActor = nullptr;
			PlayerWidget->SetPromptF(false);
		}
	}
}

//////////////////////////////////////////////////////////////////////////// Input

void AInspectModeCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AInspectModeCharacter::Move);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AInspectModeCharacter::Look);

		EnhancedInputComponent->BindAction(EnterInspectAction, ETriggerEvent::Triggered, this,
		                                   &AInspectModeCharacter::EnterInspect);
		EnhancedInputComponent->BindAction(ExitInspectAction, ETriggerEvent::Triggered, this,
		                                   &AInspectModeCharacter::ExitInspect);

		EnhancedInputComponent->BindAction(RotateInspectAction, ETriggerEvent::Triggered, this,
		                                   &AInspectModeCharacter::RotateInspect);
	}
	else
	{
		UE_LOG(LogTemplateCharacter, Error,
		       TEXT(
			       "'%s' Failed to find an Enhanced Input Component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."
		       ), *GetNameSafe(this));
	}
}


void AInspectModeCharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		PlayerWidget->SetPromptF(true);
		// add movement 
		AddMovementInput(GetActorForwardVector(), MovementVector.Y);
		AddMovementInput(GetActorRightVector(), MovementVector.X);
	}
}

void AInspectModeCharacter::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		PlayerWidget->SetPromptF(false);
		// add yaw and pitch input to controller
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y);
	}
}

void AInspectModeCharacter::EnterInspect()
{
	if (!IsInspecting && IsValid(CurrentInspectActor))
	{
		IsInspecting = true;
		PlayerWidget->SetPromptF(false);
		InspectOrigin->SetRelativeRotation(FRotator::ZeroRotator);
		InitialInspectTransform = CurrentInspectActor->GetActorTransform();
		CurrentInspectActor->AttachToComponent(InspectOrigin, FAttachmentTransformRules::SnapToTargetNotIncludingScale);

		// https://forums.unrealengine.com/t/get-enhanced-input-local-player-subsystem-in-c/1732524/2
		auto PlayerController = Cast<APlayerController>(GetController());
		auto InputSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(
			PlayerController->GetLocalPlayer());
		InputSubsystem->RemoveMappingContext(DefaultMappingContext);
		InputSubsystem->AddMappingContext(InspectMappingContext, 0);
	}
}

void AInspectModeCharacter::ExitInspect()
{
	if (IsInspecting)
	{
		IsInspecting = false;
		CurrentInspectActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		CurrentInspectActor->SetActorTransform(InitialInspectTransform);
		CurrentInspectActor = nullptr;
		
		auto PlayerController = Cast<APlayerController>(GetController());
		auto InputSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(
			PlayerController->GetLocalPlayer());
		InputSubsystem->RemoveMappingContext(InspectMappingContext);
		InputSubsystem->AddMappingContext(DefaultMappingContext, 0);
	}
}

void AInspectModeCharacter::RotateInspect(const FInputActionValue& Value)
{
	GEngine->AddOnScreenDebugMessage(-1, 1, FColor::Red, TEXT("Rotating"));
}
