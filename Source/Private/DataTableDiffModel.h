#pragma once

#include "CoreMinimal.h"

class UDataTable;

enum class ETableDiffKind : uint8 { Unchanged, Added, Removed, Modified };

struct FTableFieldDiff
{
	FString Path;
	FString DisplayPath;
	FString OldValue;
	FString NewValue;
	FString OldType;
	FString NewType;
	FString Label;
	// Preview identity and the actual source/destination paths may differ after array alignment.
	FString RevertPath;
	TArray<TSharedPtr<FTableFieldDiff>> Children;
	ETableDiffKind Kind = ETableDiffKind::Unchanged;
};

struct FTableRowDiff
{
	FName Name;
	ETableDiffKind Kind = ETableDiffKind::Unchanged;
	TArray<TSharedPtr<FTableFieldDiff>> Fields;
	TArray<TSharedPtr<FTableFieldDiff>> Details;
};

// Owns only strings, so a diff window cannot retain source-control temporary assets.
struct FTableDiffModel
{
	FString OldSchema;
	FString NewSchema;
	TArray<TSharedPtr<FTableRowDiff>> Rows;
	int32 Added = 0;
	int32 Removed = 0;
	int32 Modified = 0;

	static FTableDiffModel Compare(const UDataTable* OldTable, const UDataTable* NewTable);
};
