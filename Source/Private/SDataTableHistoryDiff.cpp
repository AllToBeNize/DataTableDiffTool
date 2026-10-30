// Copyright Meowoods 2026. All Rights Reserved.

#include "SDataTableHistoryDiff.h"
#include "SDataTableDiffView.h"
#include "Engine/DataTable.h"
#include "ISourceControlModule.h"
#include "ISourceControlRevision.h"
#include "ISourceControlState.h"
#include "SourceControlHelpers.h"
#include "SourceControlOperations.h"
#include "DiffUtils.h"
#include "UObject/Package.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/Paths.h"
#include "DataTableRevisionHistory.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "DataTableDiffRevert.h"
#include "Editor.h"
#include "Serialization/ArchiveReplaceObjectRef.h"
#include "Misc/PackageName.h"
#include "Misc/PackagePath.h"
#include "Async/Async.h"

#define LOCTEXT_NAMESPACE "DataTableDiffTool"

namespace
{
	UDataTable* CreateHistoricalSnapshot(const UDataTable* Table, UDataTable* Workspace)
	{
		if (!Table) { return nullptr; }
		UDataTable* Snapshot = DuplicateObject<UDataTable>(Table, GetTransientPackage());
		TMap<UObject*, UObject*> Replacement;
		Replacement.Add(Snapshot, Workspace);
		FArchiveReplaceObjectRef<UObject> Replace(Snapshot, Replacement);
		return Snapshot;
	}
}

void SDataTableHistoryDiff::Construct(const FArguments& Args)
{
	Workspace.Reset(Args._Table);
	TableChanged = Workspace->OnDataTableChanged().AddSP(this, &SDataTableHistoryDiff::RefreshComparison);
	if (GEditor) { GEditor->RegisterForUndo(this); }
	SelectedRevision = Args._InitialRevision;
	const FString PackageFilename = FPaths::ConvertRelativePathToFull(SourceControlHelpers::PackageFilename(Workspace->GetOutermost()));
	Filename = FPlatformFileManager::Get().GetPlatformFile().GetFilenameOnDisk(*PackageFilename);
	SelectLocal();
	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(8, 4)
		[SNew(STextBlock).AutoWrapText(true).Text_Lambda([this]() { return Status; })
			.Visibility_Lambda([this]() { return Status.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible; })]
		+ SVerticalBox::Slot().FillHeight(1)
		[SAssignNew(Comparison, SDataTableDiffView)
			.LeftControls(MakeRevisionControls(false)).RightControls(MakeRevisionControls(true))
			.OnCanRevertChange(this, &SDataTableHistoryDiff::CanRevert).OnRevertChange(this, &SDataTableHistoryDiff::Revert)]
	];
	if (Args._UseInitialComparison) { InitializeComparison(Args._InitialComparison); }
	else { RefreshHistory(); }
}

TSharedRef<SWidget> SDataTableHistoryDiff::MakeRevisionControls(bool bRight)
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(6, 4)
		[SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()
			[SNew(SComboButton).OnGetMenuContent_Lambda([this, bRight]() { return RevisionMenu(bRight); })
				.IsEnabled_Lambda([this]() { return !bFetching && !bLoading; })
				.ButtonContent()[SNew(STextBlock).Text_Lambda([this, bRight]()
				{
					if (bRight && bRightLocal) { return LOCTEXT("LocalVersion", "Local"); }
					const FString Number = bRight ? RightRevisionNumber : !ComparedRevision.IsEmpty() ? ComparedRevision : SelectedRevision ? SelectedRevision->GetRevision() : FString();
					if (!bRight && !Revisions.IsEmpty() && Number == Revisions[0]->GetRevision()) { return LOCTEXT("DepotVersion", "Depot"); }
					return Number.IsEmpty() ? LOCTEXT("SelectVersion", "Select revision")
						: FText::Format(LOCTEXT("SelectedVersion", "Revision {0}"), FText::FromString(Number));
				})]]
			+ SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center).Padding(8, 0)
			[SNew(STextBlock).Text(bRight ? LOCTEXT("RightVersion", "Right version") : LOCTEXT("LeftCompared", "Compared version"))]]
		+ SVerticalBox::Slot().AutoHeight().Padding(6, 0)
		[SNew(SBox).HeightOverride(32)
			[SNew(STextBlock).AutoWrapText(true).Text_Lambda([this, bRight]() { return bRight ? RightMetadata : LeftMetadata; })]]
		+ SVerticalBox::Slot().AutoHeight().Padding(6, 0, 6, 4)
		[SNew(SBox).HeightOverride(62)
			[SNew(SMultiLineEditableTextBox).IsReadOnly(true).AutoWrapText(true)
				.Text_Lambda([this, bRight]() { return bRight ? RightCommitText : CommitText; })]];
}

void SDataTableHistoryDiff::SetHistorical(const UDataTable* Table)
{
	Historical.Reset(CreateHistoricalSnapshot(Table, Workspace.Get()));
}

void SDataTableHistoryDiff::InitializeComparison(const FAssetDiffArgs& Args)
{
	bHasComparison = true; bRightMissing = Args.NewAsset == nullptr;
	bWorkspaceEditable = !bRightMissing && Args.NewRevision.Revision.IsEmpty()
		&& !FPackageName::IsTempPackage(Workspace->GetOutermost()->GetName());
	bRightLocal = bWorkspaceEditable;
	SetHistorical(Cast<UDataTable>(Args.OldAsset));
	ComparedRevision = Args.OldRevision.Revision;
	ComparedLabel = FString::Printf(TEXT("Compared: %s (revision %s)"), *Workspace->GetName(), *ComparedRevision);
	LeftMetadata = FText::FromString(FString::Printf(TEXT("CL %d | %s"), Args.OldRevision.Changelist, *Args.OldRevision.Date.ToString()));
	if (!bRightLocal)
	{
		RightHistorical.Reset(CreateHistoricalSnapshot(Cast<UDataTable>(Args.NewAsset), Workspace.Get()));
		RightRevisionNumber = Args.NewRevision.Revision;
		WorkspaceLabel = FString::Printf(TEXT("Right: %s (revision %s)"), *Workspace->GetName(), *RightRevisionNumber);
		RightMetadata = FText::FromString(FString::Printf(TEXT("CL %d | %s"), Args.NewRevision.Changelist, *Args.NewRevision.Date.ToString()));
		RightCommitText = FText::GetEmpty();
	}
	CacheRevisionHistory();
	for (const auto& Revision : Revisions)
	{
		if (Revision->GetRevision() == ComparedRevision) { SelectedRevision = Revision; UpdateMetadata(Revision, false); }
		if (!bRightLocal && Revision->GetRevision() == RightRevisionNumber) { RightRevision = Revision; UpdateMetadata(Revision, true); }
	}
	Status = FText::GetEmpty();
	RefreshComparison();
}

SDataTableHistoryDiff::~SDataTableHistoryDiff()
{
	if (Workspace.IsValid()) { Workspace->OnDataTableChanged().Remove(TableChanged); }
	if (GEditor) { GEditor->UnregisterForUndo(this); }
}

void SDataTableHistoryDiff::PostUndo(bool bSuccess)
{
	if (bSuccess) { RefreshComparison(); }
}

void SDataTableHistoryDiff::RefreshComparison()
{
	if (!bHasComparison || !Comparison) { return; }
	const UDataTable* RightTable = bRightMissing ? nullptr : bRightLocal ? Workspace.Get() : RightHistorical.Get();
	Comparison->SetComparison(FTableDiffModel::Compare(Historical.Get(), RightTable), ComparedLabel, WorkspaceLabel);
}

bool SDataTableHistoryDiff::CanRevert(FName RowName, const FTableRestoreTarget& Restore) const
{
	FText Error;
	return !bLoading && bRightLocal && bWorkspaceEditable && FDataTableDiffRevert::CanApply(Historical.Get(), Workspace.Get(), RowName, Restore, Error);
}

void SDataTableHistoryDiff::Revert(FName RowName, const FTableRestoreTarget& Restore)
{
	if (!CanRevert(RowName, Restore)) { return; }
	FText Error;
	if (FDataTableDiffRevert::Apply(Historical.Get(), Workspace.Get(), RowName, Restore, Error))
	{
		Status = LOCTEXT("Reverted", "Local change restored from the left revision. Undo is available; save the DataTable to persist the edit.");
	}
	else { Status = Error; }
}

void SDataTableHistoryDiff::SelectLocal()
{
	if (!bWorkspaceEditable) { return; }
	bRightLocal = true; bRightMissing = false;
	RightHistorical.Reset(); RightRevision.Reset(); RightRevisionNumber.Reset();
	WorkspaceLabel = TEXT("Local workspace: ") + Workspace->GetName();
	RightMetadata = LOCTEXT("LocalMetadata", "Local workspace - current in-memory DataTable");
	RightCommitText = LOCTEXT("LocalDescription", "Includes unsaved edits. Revert restores individual local changes from the left revision.");
	RefreshComparison();
}

void SDataTableHistoryDiff::RefreshHistory()
{
	if (bFetching) { return; }
	ISourceControlModule& Module = ISourceControlModule::Get();
	if (!Module.IsEnabled() || !Module.GetProvider().IsAvailable())
	{
		Status = LOCTEXT("NoProvider", "Connect source control, then reopen either revision menu to retry."); return;
	}
	bFetching = true;
	Status = LOCTEXT("Fetching", "Fetching depot revision history...");
	TSharedRef<FUpdateStatus, ESPMode::ThreadSafe> Operation = ISourceControlOperation::Create<FUpdateStatus>();
	Operation->SetUpdateHistory(true);
	Module.GetProvider().Execute(Operation, TArray<FString>{Filename}, EConcurrency::Asynchronous,
		FSourceControlOperationComplete::CreateSP(this, &SDataTableHistoryDiff::HistoryReady));
}

void SDataTableHistoryDiff::HistoryReady(const FSourceControlOperationRef& Operation, ECommandResult::Type Result)
{
	bFetching = false;
	if (Result != ECommandResult::Succeeded) { Status = LOCTEXT("HistoryFailed", "Could not fetch history. Reopen either revision menu to retry."); return; }
	CacheRevisionHistory();
	if (Revisions.IsEmpty()) { Status = LOCTEXT("NoHistory", "No depot revisions are available for this table."); return; }
	if (bHasComparison)
	{
		for (const auto& Revision : Revisions)
		{
			if (Revision->GetRevision() == ComparedRevision) { SelectedRevision = Revision; UpdateMetadata(Revision, false); }
			if (!bRightLocal && Revision->GetRevision() == RightRevisionNumber) { RightRevision = Revision; UpdateMetadata(Revision, true); }
		}
		Status = LOCTEXT("HistoryRefreshed", "Revision history refreshed; both selected versions are unchanged."); return;
	}
	CompareRevision(SelectedRevision ? SelectedRevision : Revisions[0]);
}

void SDataTableHistoryDiff::CacheRevisionHistory()
{
	Revisions.Reset();
	if (!ISourceControlModule::Get().IsEnabled()) { return; }
	FSourceControlStatePtr State = ISourceControlModule::Get().GetProvider().GetState(Filename, EStateCacheUsage::Use);
	if (State)
	{
		for (int32 Index = 0; Index < State->GetHistorySize(); ++Index) { if (auto Revision = State->GetHistoryItem(Index)) { Revisions.Add(Revision); } }
	}
	if (!Revisions.IsEmpty() && ISourceControlModule::Get().GetProvider().GetName() == TEXT("Perforce")) { CompletePerforceTableHistory(Revisions, Filename); }
}

void SDataTableHistoryDiff::CompareRevision(FRevisionPtr Revision, bool bRight)
{
	if (bLoading || !Revision) { return; }
	bLoading = true; Status = LOCTEXT("LoadingRevision", "Loading DataTable revision...");
	ISourceControlProvider& Provider = ISourceControlModule::Get().GetProvider();
	const auto Operation = MakeShared<FGetFile, ESPMode::ThreadSafe>(LexToString(Revision->GetCheckInIdentifier()), Revision->GetRevision(), Revision->GetFilename());
	if (Provider.CanExecuteOperation(Operation))
	{
		if (Provider.Execute(Operation, TArray<FString>{}, EConcurrency::Asynchronous,
			FSourceControlOperationComplete::CreateSP(this, &SDataTableHistoryDiff::RevisionDownloaded, Revision, bRight)) != ECommandResult::Succeeded)
		{
			LoadRevision(Revision, bRight, FString());
		}
		return;
	}
	// Providers without GetFile still expose revision downloads; only asset loading stays on the game thread.
	const TWeakPtr<SDataTableHistoryDiff> WeakThis = SharedThis(this);
	Async(EAsyncExecution::ThreadPool, [WeakThis, Revision, bRight]()
	{
		FString DownloadedFilename;
		const bool bDownloaded = Revision->Get(DownloadedFilename, EConcurrency::Asynchronous);
		AsyncTask(ENamedThreads::GameThread, [WeakThis, Revision, bRight, DownloadedFilename, bDownloaded]()
		{
			if (const auto View = WeakThis.Pin()) { View->LoadRevision(Revision, bRight, bDownloaded ? DownloadedFilename : FString()); }
		});
	});
}

void SDataTableHistoryDiff::RevisionDownloaded(const FSourceControlOperationRef& Operation, ECommandResult::Type Result, FRevisionPtr Revision, bool bRight)
{
	const auto Download = StaticCastSharedRef<FGetFile>(Operation);
	LoadRevision(Revision, bRight, Result == ECommandResult::Succeeded ? Download->GetOutPackageFilename() : FString());
}

void SDataTableHistoryDiff::LoadRevision(FRevisionPtr Revision, bool bRight, const FString& DownloadedFilename)
{
	bLoading = false;
	UPackage* Package = DownloadedFilename.IsEmpty() ? nullptr : DiffUtils::LoadPackageForDiff(
		FPackagePath::FromLocalPath(DownloadedFilename), FPackagePath::FromLocalPath(Revision->GetFilename()));
	const UDataTable* Table = Package ? Cast<UDataTable>(Package->FindAssetInPackage()) : nullptr;
	if (!Table) { Status = LOCTEXT("LoadFailed", "Unable to load this revision as a DataTable. Select another revision to retry."); return; }
	SetRevisionTable(Table, Revision, bRight);
}

void SDataTableHistoryDiff::UpdateMetadata(FRevisionPtr Revision, bool bRight)
{
	FText& Metadata = bRight ? RightMetadata : LeftMetadata;
	FText& Description = bRight ? RightCommitText : CommitText;
	Metadata = Revision->GetDate().GetTicks() ? FText::FromString(FString::Printf(TEXT("CL %d | %s | %s"),
		Revision->GetCheckInIdentifier(), *Revision->GetUserName(), *Revision->GetDate().ToString()))
		: LOCTEXT("OlderMetadata", "Submission metadata is outside the provider cache.");
	Description = FText::FromString(Revision->GetDescription());
}

void SDataTableHistoryDiff::SetRevisionTable(const UDataTable* Table, FRevisionPtr Revision, bool bRight)
{
	if (bRight)
	{
		RightHistorical.Reset(CreateHistoricalSnapshot(Table, Workspace.Get()));
		RightRevision = Revision; RightRevisionNumber = Revision->GetRevision(); bRightLocal = false; bRightMissing = false;
		WorkspaceLabel = FString::Printf(TEXT("Right: %s (revision %s)"), *Workspace->GetName(), *RightRevisionNumber);
	}
	else
	{
		SetHistorical(Table); SelectedRevision = Revision; ComparedRevision = Revision->GetRevision();
		ComparedLabel = FString::Printf(TEXT("Compared: %s (revision %s)"), *Workspace->GetName(), *ComparedRevision);
	}
	bHasComparison = true;
	UpdateMetadata(Revision, bRight); RefreshComparison();
	Status = FText::GetEmpty();
}

TSharedRef<SWidget> SDataTableHistoryDiff::RevisionMenu(bool bRight)
{
	FMenuBuilder Menu(true, nullptr);
	if (Revisions.IsEmpty() && !bFetching && !bLoading)
	{
		RefreshHistory();
		Menu.AddMenuEntry(LOCTEXT("RetryHistory", "Retry history query"), LOCTEXT("RetryHistoryTip", "Retry the history query, then reopen this menu to see the results."), FSlateIcon(),
			FUIAction(FExecuteAction::CreateSP(this, &SDataTableHistoryDiff::RefreshHistory), FCanExecuteAction::CreateLambda([this]() { return !bFetching; })));
	}
	if (bRight)
	{
		Menu.AddMenuEntry(LOCTEXT("LocalVersion", "Local"), LOCTEXT("LocalVersionTip", "Use the current editable workspace table, including unsaved edits."), FSlateIcon(),
			FUIAction(FExecuteAction::CreateSP(this, &SDataTableHistoryDiff::SelectLocal), FCanExecuteAction::CreateLambda([this]() { return bWorkspaceEditable; })));
		Menu.AddMenuSeparator();
	}
	for (const auto& Revision : Revisions)
	{
		const FText Label = !bRight && Revision == Revisions[0] ? LOCTEXT("DepotVersion", "Depot")
			: FText::Format(LOCTEXT("VersionItem", "Revision {0}"), FText::FromString(Revision->GetRevision()));
		Menu.AddMenuEntry(Label, FText::FromString(Revision->GetDescription()), FSlateIcon(),
			FUIAction(FExecuteAction::CreateSP(this, &SDataTableHistoryDiff::CompareRevision, Revision, bRight)));
	}
	return Menu.MakeWidget();
}

#undef LOCTEXT_NAMESPACE
