// Copyright Epic Games, Inc. All Rights Reserved.

#include "InspectModeGameMode.h"
#include "InspectModeCharacter.h"
#include "UObject/ConstructorHelpers.h"

AInspectModeGameMode::AInspectModeGameMode()
	: Super()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnClassFinder(TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter"));
	DefaultPawnClass = PlayerPawnClassFinder.Class;

}
