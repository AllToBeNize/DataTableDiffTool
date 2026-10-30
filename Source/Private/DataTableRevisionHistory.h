// Copyright Meowoods 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ISourceControlRevision.h"

// Perforce file revisions are contiguous. Fill entries beyond the provider's 100-entry metadata cache.
TSharedPtr<ISourceControlRevision, ESPMode::ThreadSafe> MakeNumberedTableRevision(const FString& Filename, int32 Number);
void CompletePerforceTableHistory(TArray<TSharedPtr<ISourceControlRevision, ESPMode::ThreadSafe>>& Revisions, const FString& Filename);
