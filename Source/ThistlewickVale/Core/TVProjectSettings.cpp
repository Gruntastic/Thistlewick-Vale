#include "Core/TVProjectSettings.h"
#include "Engine/DataTable.h"

UDataTable* UTVProjectSettings::LoadTable(FName Key) const
{
	if (const TSoftObjectPtr<UDataTable>* Found = Tables.Find(Key))
	{
		return Found->LoadSynchronous();
	}
	return nullptr;
}
