#pragma once

#include "CoreMinimal.h"

class UDataTable;
class ISourceControlRevision;
struct FAssetDiffArgs;

void OpenDataTableDiffTool(UDataTable* Table, TSharedPtr<ISourceControlRevision, ESPMode::ThreadSafe> Revision = nullptr);
void OpenDataTableDiffTool(const FAssetDiffArgs& Args);
void RegisterDataTableDiffToolbar();
