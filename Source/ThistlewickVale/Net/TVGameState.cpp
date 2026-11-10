#include "Net/TVGameState.h"

#include "ThistlewickVale.h"
#include "Net/UnrealNetwork.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"

ATVGameState::ATVGameState()
{
}

void ATVGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATVGameState, WorldFlags);
}

void ATVGameState::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		LoadFlagsFromDisk();
	}
}

bool ATVGameState::HasWorldFlag(FName Flag) const
{
	return !Flag.IsNone() && WorldFlags.Contains(Flag);
}

void ATVGameState::SetWorldFlag(FName Flag)
{
	if (!HasAuthority() || Flag.IsNone() || WorldFlags.Contains(Flag))
	{
		return;
	}
	WorldFlags.Add(Flag);
	UE_LOG(LogTV, Log, TEXT("%s World flag set: %s"), TV_NET_LOG_PREFIX, *Flag.ToString());
	SaveFlagsToDisk();
	ForceNetUpdate();
	OnWorldFlagsChanged.Broadcast();
}

void ATVGameState::ClearWorldFlag(FName Flag)
{
	if (!HasAuthority() || WorldFlags.Remove(Flag) == 0)
	{
		return;
	}
	SaveFlagsToDisk();
	ForceNetUpdate();
	OnWorldFlagsChanged.Broadcast();
}

void ATVGameState::OnRep_WorldFlags()
{
	OnWorldFlagsChanged.Broadcast();
}

FString ATVGameState::GetFlagsFilePath()
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("World"), TEXT("WorldFlags.json"));
}

void ATVGameState::LoadFlagsFromDisk()
{
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *GetFlagsFilePath()))
	{
		return;
	}
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		UE_LOG(LogTV, Warning, TEXT("%s Could not parse %s"), TV_NET_LOG_PREFIX, *GetFlagsFilePath());
		return;
	}
	const TArray<TSharedPtr<FJsonValue>>* Flags = nullptr;
	if (Root->TryGetArrayField(TEXT("worldFlags"), Flags) && Flags)
	{
		for (const TSharedPtr<FJsonValue>& Value : *Flags)
		{
			FString FlagString;
			if (Value.IsValid() && Value->TryGetString(FlagString) && !FlagString.IsEmpty())
			{
				WorldFlags.AddUnique(FName(*FlagString));
			}
		}
	}
	UE_LOG(LogTV, Log, TEXT("%s Loaded %d world flags"), TV_NET_LOG_PREFIX, WorldFlags.Num());
}

void ATVGameState::SaveFlagsToDisk() const
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("schemaVersion"), 1);
	TArray<TSharedPtr<FJsonValue>> Flags;
	for (const FName& Flag : WorldFlags)
	{
		Flags.Add(MakeShared<FJsonValueString>(Flag.ToString()));
	}
	Root->SetArrayField(TEXT("worldFlags"), Flags);

	FString Text;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Text);
	if (FJsonSerializer::Serialize(Root, Writer))
	{
		FFileHelper::SaveStringToFile(Text, *GetFlagsFilePath());
	}
}
