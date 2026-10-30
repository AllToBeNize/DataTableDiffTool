// Copyright Meowoods 2026. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"
#include "UObject/StrongObjectPtr.h"
#include "ISourceControlProvider.h"
#include "EditorUndoClient.h"
#include "AssetDefinition.h"

class UDataTable;
class SBox;
class ISourceControlRevision;
class SDataTableDiffView;
struct FTableRestoreTarget;

class SDataTableHistoryDiff : public SCompoundWidget, public FEditorUndoClient
{
public:
	using FRevisionPtr = TSharedPtr<ISourceControlRevision, ESPMode::ThreadSafe>;
	SLATE_BEGIN_ARGS(SDataTableHistoryDiff) : _UseInitialComparison(false) {} SLATE_ARGUMENT(UDataTable*, Table)
		SLATE_ARGUMENT(FRevisionPtr, InitialRevision)
		SLATE_ARGUMENT(bool, UseInitialComparison)
		SLATE_ARGUMENT(FAssetDiffArgs, InitialComparison) SLATE_END_ARGS()
	void Construct(const FArguments& Args);
	virtual ~SDataTableHistoryDiff() override;
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override { PostUndo(bSuccess); }
private:
	void RefreshHistory();
	void HistoryReady(const FSourceControlOperationRef& Operation, ECommandResult::Type Result);
	void CompareRevision(FRevisionPtr Revision, bool bRight = false);
	void LoadRevision(FRevisionPtr Revision, bool bRight, const FString& DownloadedFilename);
	void RevisionDownloaded(const FSourceControlOperationRef& Operation, ECommandResult::Type Result, FRevisionPtr Revision, bool bRight);
	TSharedRef<SWidget> RevisionMenu(bool bRight);
	TSharedRef<SWidget> MakeRevisionControls(bool bRight);
	void SelectLocal();
	void SetRevisionTable(const UDataTable* Table, FRevisionPtr Revision, bool bRight);
	void UpdateMetadata(FRevisionPtr Revision, bool bRight);
	void RefreshComparison();
	void CacheRevisionHistory();
	void SetHistorical(const UDataTable* Table);
	void InitializeComparison(const FAssetDiffArgs& Args);
	bool CanRevert(FName RowName, const FTableRestoreTarget& Restore) const;
	void Revert(FName RowName, const FTableRestoreTarget& Restore);
	TStrongObjectPtr<UDataTable> Workspace;
	TStrongObjectPtr<UDataTable> Historical;
	TStrongObjectPtr<UDataTable> RightHistorical;
	FDelegateHandle TableChanged;
	TArray<TSharedPtr<ISourceControlRevision, ESPMode::ThreadSafe>> Revisions;
	TSharedPtr<ISourceControlRevision, ESPMode::ThreadSafe> SelectedRevision;
	FRevisionPtr RightRevision;
	TSharedPtr<SDataTableDiffView> Comparison;
	FString Filename;
	FString ComparedRevision;
	FString RightRevisionNumber;
	FString ComparedLabel;
	FString WorkspaceLabel;
	FText Status;
	FText CommitText;
	FText LeftMetadata;
	FText RightMetadata;
	FText RightCommitText;
	bool bFetching = false;
	bool bLoading = false;
	bool bHasComparison = false;
	bool bRightMissing = false;
	bool bRightLocal = true;
	bool bWorkspaceEditable = true;
};
