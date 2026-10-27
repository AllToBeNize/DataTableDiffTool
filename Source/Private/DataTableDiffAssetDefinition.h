#pragma once

#include "Table/AssetDefinition_DataTable.h"
#include "DataTableDiffAssetDefinition.generated.h"

UCLASS()
class UDataTableDiffAssetDefinition : public UAssetDefinition_DataTable
{
	GENERATED_BODY()
public:
	virtual bool CanRegisterStatically() const override { return false; }
	virtual EAssetCommandResult PerformAssetDiff(const FAssetDiffArgs& Args) const override;
	static bool UsesVisualDiff();
	static bool UsesVisualDiff(const FAssetDiffArgs& Args);
	static bool IsTablePair(const FAssetDiffArgs& Args);
};

void InstallDataTableDiffRouter();
void RemoveDataTableDiffRouter();
