// Copyright Meowoods 2026. All Rights Reserved.

#pragma once

#include "Engine/DeveloperSettings.h"
#include "DataTableDiffSettings.generated.h"

UENUM()
enum class EDataTableDiffTool : uint8
{
	ExternalTool UMETA(DisplayName="Native External Tool (e.g. P4Merge)"),
	VisualDiff UMETA(DisplayName="DataTable Diff Tool")
};

UCLASS(config=EditorPerProjectUserSettings, meta=(DisplayName="DataTable Diff Tool"))
class UDataTableDiffSettings : public UDeveloperSettings
{
	GENERATED_BODY()
public:
	virtual FName GetContainerName() const override { return TEXT("Editor"); }
	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }
	virtual FName GetSectionName() const override { return TEXT("DataTableDiffTool"); }

	/** Tool used by Diff Against Depot and revision history. */
	UPROPERTY(config, EditAnywhere, Category="DataTable Diff", meta=(DisplayName="Revision Comparison Tool",
		ToolTip="Native External Tool uses Editor Preferences > Loading & Saving > Text Diff Tool Path. DataTable Diff Tool opens the selected revisions inside UE. Applies immediately to DataTables only."))
	EDataTableDiffTool ComparisonTool = EDataTableDiffTool::VisualDiff;
};
