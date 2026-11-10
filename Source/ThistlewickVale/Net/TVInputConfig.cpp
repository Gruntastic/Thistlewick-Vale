#include "Net/TVInputConfig.h"

#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "EnhancedActionKeyMapping.h"
#include "InputCoreTypes.h"

namespace
{
	void AddNegate(UInputMappingContext* Context, FEnhancedActionKeyMapping& Mapping)
	{
		Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(Context));
	}

	void AddSwizzleYXZ(UInputMappingContext* Context, FEnhancedActionKeyMapping& Mapping)
	{
		UInputModifierSwizzleAxis* Swizzle = NewObject<UInputModifierSwizzleAxis>(Context);
		Swizzle->Order = EInputAxisSwizzle::YXZ;
		Mapping.Modifiers.Add(Swizzle);
	}
}

UInputAction* UTVInputConfig::MakeAction(const TCHAR* Name, EInputActionValueType ValueType)
{
	UInputAction* Action = NewObject<UInputAction>(this, FName(Name));
	Action->ValueType = ValueType;
	return Action;
}

UTVInputConfig* UTVInputConfig::Create(UObject* Outer)
{
	UTVInputConfig* Config = NewObject<UTVInputConfig>(Outer, TEXT("TVInputConfig"));

	Config->Move = Config->MakeAction(TEXT("IA_TV_Move"), EInputActionValueType::Axis2D);
	Config->Look = Config->MakeAction(TEXT("IA_TV_Look"), EInputActionValueType::Axis2D);
	Config->OrbitHold = Config->MakeAction(TEXT("IA_TV_OrbitHold"), EInputActionValueType::Boolean);
	Config->Zoom = Config->MakeAction(TEXT("IA_TV_Zoom"), EInputActionValueType::Axis1D);
	Config->Jump = Config->MakeAction(TEXT("IA_TV_Jump"), EInputActionValueType::Boolean);
	Config->AutoRun = Config->MakeAction(TEXT("IA_TV_AutoRun"), EInputActionValueType::Boolean);
	Config->TabTarget = Config->MakeAction(TEXT("IA_TV_TabTarget"), EInputActionValueType::Boolean);
	Config->TargetSelf = Config->MakeAction(TEXT("IA_TV_TargetSelf"), EInputActionValueType::Boolean);
	for (int32 Index = 0; Index < 4; ++Index)
	{
		Config->TargetParty.Add(Config->MakeAction(*FString::Printf(TEXT("IA_TV_TargetParty%d"), Index + 1), EInputActionValueType::Boolean));
	}
	Config->ClickSelect = Config->MakeAction(TEXT("IA_TV_ClickSelect"), EInputActionValueType::Boolean);
	for (int32 Index = 0; Index < 10; ++Index)
	{
		Config->ActionSlots.Add(Config->MakeAction(*FString::Printf(TEXT("IA_TV_ActionSlot%d"), Index + 1), EInputActionValueType::Boolean));
	}
	Config->Interact = Config->MakeAction(TEXT("IA_TV_Interact"), EInputActionValueType::Boolean);
	Config->Mount = Config->MakeAction(TEXT("IA_TV_Mount"), EInputActionValueType::Boolean);
	Config->ToggleBags = Config->MakeAction(TEXT("IA_TV_ToggleBags"), EInputActionValueType::Boolean);
	Config->ToggleCharacter = Config->MakeAction(TEXT("IA_TV_ToggleCharacter"), EInputActionValueType::Boolean);
	Config->ToggleQuestLog = Config->MakeAction(TEXT("IA_TV_ToggleQuestLog"), EInputActionValueType::Boolean);
	Config->ToggleAbilityBook = Config->MakeAction(TEXT("IA_TV_ToggleAbilityBook"), EInputActionValueType::Boolean);
	Config->ToggleMenu = Config->MakeAction(TEXT("IA_TV_ToggleMenu"), EInputActionValueType::Boolean);
	Config->OpenChat = Config->MakeAction(TEXT("IA_TV_OpenChat"), EInputActionValueType::Boolean);

	// IMC_TV_Default
	UInputMappingContext* Default = NewObject<UInputMappingContext>(Config, TEXT("IMC_TV_Default"));
	Config->DefaultContext = Default;
	Config->MapMovementAndCamera(Default);
	Config->MapUiKeys(Default);
	Default->MapKey(Config->TabTarget, EKeys::Tab);
	Default->MapKey(Config->TargetSelf, EKeys::F1);
	const FKey PartyKeys[4] = { EKeys::F2, EKeys::F3, EKeys::F4, EKeys::F5 };
	for (int32 Index = 0; Index < 4; ++Index)
	{
		Default->MapKey(Config->TargetParty[Index], PartyKeys[Index]);
	}
	Default->MapKey(Config->ClickSelect, EKeys::LeftMouseButton);
	const FKey SlotKeys[10] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five,
		EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine, EKeys::Zero };
	for (int32 Index = 0; Index < 10; ++Index)
	{
		Default->MapKey(Config->ActionSlots[Index], SlotKeys[Index]);
	}
	Default->MapKey(Config->Interact, EKeys::F);
	Default->MapKey(Config->Mount, EKeys::Y);

	// IMC_TV_Mounted: movement only + dismount (DATA_SCHEMAS 10); camera and UI keys stay usable.
	UInputMappingContext* Mounted = NewObject<UInputMappingContext>(Config, TEXT("IMC_TV_Mounted"));
	Config->MountedContext = Mounted;
	Config->MapMovementAndCamera(Mounted);
	Config->MapUiKeys(Mounted);
	Mounted->MapKey(Config->Mount, EKeys::Y);

	return Config;
}

void UTVInputConfig::MapMovementAndCamera(UInputMappingContext* Context)
{
	// Move: X = right, Y = forward (same modifier recipe as the 5.x third-person template).
	AddSwizzleYXZ(Context, Context->MapKey(Move, EKeys::W));
	{
		FEnhancedActionKeyMapping& S = Context->MapKey(Move, EKeys::S);
		AddSwizzleYXZ(Context, S);
		AddNegate(Context, S);
	}
	AddNegate(Context, Context->MapKey(Move, EKeys::A));
	Context->MapKey(Move, EKeys::D);
	AddSwizzleYXZ(Context, Context->MapKey(Move, EKeys::Up));
	{
		FEnhancedActionKeyMapping& Down = Context->MapKey(Move, EKeys::Down);
		AddSwizzleYXZ(Context, Down);
		AddNegate(Context, Down);
	}

	Context->MapKey(Look, EKeys::Mouse2D);
	Context->MapKey(OrbitHold, EKeys::RightMouseButton);
	Context->MapKey(Zoom, EKeys::MouseWheelAxis);
	Context->MapKey(Jump, EKeys::SpaceBar);
	Context->MapKey(AutoRun, EKeys::NumLock);
	Context->MapKey(AutoRun, EKeys::R);
}

void UTVInputConfig::MapUiKeys(UInputMappingContext* Context)
{
	Context->MapKey(ToggleBags, EKeys::B);
	Context->MapKey(ToggleCharacter, EKeys::C);
	Context->MapKey(ToggleQuestLog, EKeys::L);
	Context->MapKey(ToggleAbilityBook, EKeys::P);
	Context->MapKey(ToggleMenu, EKeys::Escape);
	Context->MapKey(OpenChat, EKeys::Enter);
}
