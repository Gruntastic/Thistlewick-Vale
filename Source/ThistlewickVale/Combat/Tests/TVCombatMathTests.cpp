// Formula checks against Docs/BALANCE.md section 1. Run: Automation RunTests ThistlewickVale.Combat
#include "Misc/AutomationTest.h"
#include "Combat/TVCombatLibrary.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTVCombatMathTest, "ThistlewickVale.Combat.Formulas",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTVCombatMathTest::RunTest(const FString& Parameters)
{
	// GCD 1.5 s, scaled by haste (Haste/3 %), floor 1.0 s.
	TestEqual(TEXT("GCD without haste"), UTVCombatLibrary::ComputeGCD(0.f), 1.5f);
	TestEqual(TEXT("GCD at 30 haste (10 %)"), UTVCombatLibrary::ComputeGCD(30.f), 1.5f / 1.1f, 0.001f);
	TestEqual(TEXT("GCD floor"), UTVCombatLibrary::ComputeGCD(1000.f), 1.0f);

	// Armor at level 10: A / (A + 350); 105 armor ~ 23 %.
	TestEqual(TEXT("Armor 105 vs L10"), UTVCombatLibrary::ComputeArmorMitigation(105.f, 10), 105.f / 455.f, 0.0001f);
	TestEqual(TEXT("No armor"), UTVCombatLibrary::ComputeArmorMitigation(0.f, 10), 0.f);
	TestTrue(TEXT("Mitigation cap"), UTVCombatLibrary::ComputeArmorMitigation(100000.f, 1) <= TVCombatTuning::MaxMitigation + KINDA_SMALL_NUMBER);

	// Crit 5 % + Crit/3 %.
	TestEqual(TEXT("Base crit"), UTVCombatLibrary::ComputeCritChance(0.f, 0.f), 0.05f, 0.0001f);
	TestEqual(TEXT("Crit 30 rating"), UTVCombatLibrary::ComputeCritChance(30.f, 0.f), 0.15f, 0.0001f);

	// Cast time scales with haste like the GCD (no floor).
	TestEqual(TEXT("Cast 1.8 s at 30 haste"), UTVCombatLibrary::ComputeCastTime(1.8f, 30.f), 1.8f / 1.1f, 0.001f);
	return true;
}

#endif
