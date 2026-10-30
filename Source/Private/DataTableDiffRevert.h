// Copyright Meowoods 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UDataTable;
struct FTableRestoreTarget;

// Restores one change from the selected comparison revision into the editable table.
class FDataTableDiffRevert
{
public:
	static bool CanApply(const UDataTable* Historical, const UDataTable* Workspace, FName RowName, const FTableRestoreTarget& Restore, FText& Error);
	static bool Apply(const UDataTable* Historical, UDataTable* Workspace, FName RowName, const FTableRestoreTarget& Restore, FText& Error);
};
