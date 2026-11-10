// Builds the input actions (IA_TV_*) and mapping contexts (IMC_TV_Default, IMC_TV_Mounted) at runtime with NewObject,
// so no editor-authored input assets are needed. DATA_SCHEMAS 10 names are kept as object names.
// API checked against 5.8.3 EnhancedInput: UInputMappingContext::MapKey, FEnhancedActionKeyMapping::Modifiers,
// UInputAction::ValueType (public), UInputModifierNegate, UInputModifierSwizzleAxis (Order = YXZ).
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "InputActionValue.h"
#include "TVInputConfig.generated.h"

class UInputAction;
class UInputMappingContext;

UCLASS(Transient)
class THISTLEWICKVALE_API UTVInputConfig : public UObject
{
	GENERATED_BODY()

public:
	/** Creates every action and both contexts with Outer as owner. */
	static UTVInputConfig* Create(UObject* Outer);

	UPROPERTY() TObjectPtr<UInputAction> Move;          // Axis2D: WASD (X = right, Y = forward)
	UPROPERTY() TObjectPtr<UInputAction> Look;          // Axis2D: Mouse XY, applied only while orbiting
	UPROPERTY() TObjectPtr<UInputAction> OrbitHold;     // Bool: right mouse button
	UPROPERTY() TObjectPtr<UInputAction> Zoom;          // Axis1D: mouse wheel
	UPROPERTY() TObjectPtr<UInputAction> Jump;          // Space
	UPROPERTY() TObjectPtr<UInputAction> AutoRun;       // Num Lock, R
	UPROPERTY() TObjectPtr<UInputAction> TabTarget;     // Tab
	UPROPERTY() TObjectPtr<UInputAction> TargetSelf;    // F1
	UPROPERTY() TArray<TObjectPtr<UInputAction>> TargetParty; // F2..F5
	UPROPERTY() TObjectPtr<UInputAction> ClickSelect;   // Left mouse button
	UPROPERTY() TArray<TObjectPtr<UInputAction>> ActionSlots; // 1..0
	UPROPERTY() TObjectPtr<UInputAction> Interact;      // F
	UPROPERTY() TObjectPtr<UInputAction> Mount;         // Y (summon or dismount)
	UPROPERTY() TObjectPtr<UInputAction> ToggleBags;    // B
	UPROPERTY() TObjectPtr<UInputAction> ToggleCharacter; // C
	UPROPERTY() TObjectPtr<UInputAction> ToggleQuestLog;  // L
	UPROPERTY() TObjectPtr<UInputAction> ToggleAbilityBook; // P
	UPROPERTY() TObjectPtr<UInputAction> ToggleMenu;    // Escape
	UPROPERTY() TObjectPtr<UInputAction> OpenChat;      // Enter

	UPROPERTY() TObjectPtr<UInputMappingContext> DefaultContext; // IMC_TV_Default
	UPROPERTY() TObjectPtr<UInputMappingContext> MountedContext; // IMC_TV_Mounted: movement, camera, dismount, UI keys

private:
	UInputAction* MakeAction(const TCHAR* Name, EInputActionValueType ValueType);
	void MapMovementAndCamera(UInputMappingContext* Context);
	void MapUiKeys(UInputMappingContext* Context);
};
