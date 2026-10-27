#include "SDataTableRevisionMenu.h"
#include "DataTableRevisionHistory.h"
#include "Engine/DataTable.h"
#include "ISourceControlModule.h"
#include "ISourceControlRevision.h"
#include "ISourceControlState.h"
#include "SourceControlHelpers.h"
#include "SourceControlOperations.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/Paths.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/Text/STextBlock.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "DataTableDiffTool"

void SDataTableRevisionMenu::Construct(const FArguments& Args)
{
	OnRevisionSelected = Args._OnRevisionSelected;
	ChildSlot[SAssignNew(MenuBox, SVerticalBox)];
	ISourceControlModule& Module = ISourceControlModule::Get();
	if (!Args._Table || !Module.IsEnabled() || !Module.GetProvider().IsAvailable())
	{
		ShowMessage(LOCTEXT("SourceControlDisabled", "Revision control is disabled"));
		return;
	}
	Filename = FPlatformFileManager::Get().GetPlatformFile().GetFilenameOnDisk(
		*FPaths::ConvertRelativePathToFull(SourceControlHelpers::PackageFilename(Args._Table->GetOutermost())));
	MenuBox->AddSlot().AutoHeight().Padding(8)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SThrobber)]
		+ SHorizontalBox::Slot().FillWidth(1).Padding(8, 0).VAlign(VAlign_Center)[SNew(STextBlock).Text(LOCTEXT("UpdatingHistory", "Updating history..."))]
		+ SHorizontalBox::Slot().AutoWidth()
		[SNew(SButton).Text(LOCTEXT("CancelHistory", "Cancel")).OnClicked(this, &SDataTableRevisionMenu::CancelQuery)
			.Visibility_Lambda([this]()
			{
				return Query && ISourceControlModule::Get().GetProvider().CanCancelOperation(Query.ToSharedRef()) ? EVisibility::Visible : EVisibility::Collapsed;
			})]
	];
	Query = ISourceControlOperation::Create<FUpdateStatus>();
	Query->SetUpdateHistory(true);
	Module.GetProvider().Execute(Query.ToSharedRef(), TArray<FString>{Filename}, EConcurrency::Asynchronous,
		FSourceControlOperationComplete::CreateSP(this, &SDataTableRevisionMenu::HistoryReady));
}

SDataTableRevisionMenu::~SDataTableRevisionMenu()
{
	if (Query)
	{
		if (auto* Module = FModuleManager::GetModulePtr<ISourceControlModule>(TEXT("SourceControl")))
		{
			ISourceControlProvider& Provider = Module->GetProvider();
			if (Provider.CanCancelOperation(Query.ToSharedRef())) { Provider.CancelOperation(Query.ToSharedRef()); }
		}
	}
}

FReply SDataTableRevisionMenu::CancelQuery()
{
	if (Query) { ISourceControlModule::Get().GetProvider().CancelOperation(Query.ToSharedRef()); }
	return FReply::Handled();
}

void SDataTableRevisionMenu::ShowMessage(const FText& Text)
{
	MenuBox->ClearChildren();
	MenuBox->AddSlot().AutoHeight().Padding(8)[SNew(STextBlock).Text(Text)];
}

void SDataTableRevisionMenu::HistoryReady(const FSourceControlOperationRef& Operation, ECommandResult::Type Result)
{
	Query.Reset();
	if (Result != ECommandResult::Succeeded)
	{
		ShowMessage(LOCTEXT("HistoryUnavailable", "Revision history could not be retrieved. Reopen Diff to retry."));
		return;
	}
	ISourceControlProvider& Provider = ISourceControlModule::Get().GetProvider();
	FSourceControlStatePtr State = Provider.GetState(Filename, EStateCacheUsage::Use);
	TArray<TSharedPtr<ISourceControlRevision, ESPMode::ThreadSafe>> Revisions;
	if (State)
	{
		for (int32 Index = 0; Index < State->GetHistorySize(); ++Index)
		{
			if (auto Revision = State->GetHistoryItem(Index)) { Revisions.Add(Revision); }
		}
	}
	if (Revisions.IsEmpty()) { ShowMessage(LOCTEXT("NoRevisions", "No revisions found")); return; }
	if (Provider.GetName() == TEXT("Perforce")) { CompletePerforceTableHistory(Revisions, Filename); }
	FMenuBuilder Menu(true, nullptr);
	for (int32 Index = 0; Index < Revisions.Num(); ++Index)
	{
		const auto Revision = Revisions[Index];
		const FText Label = Index == 0 ? LOCTEXT("Depot", "Depot")
			: FText::Format(LOCTEXT("RevisionNumber", "Revision {0}"), FText::FromString(Revision->GetRevision()));
		const FText Tooltip = Revision->GetDate().GetTicks()
			? FText::FromString(FString::Printf(TEXT("%s | CL #%d | %s\n%s\n%s"), *Revision->GetRevision(),
				Revision->GetCheckInIdentifier(), *Revision->GetUserName(), *Revision->GetDate().ToString(), *Revision->GetDescription()))
			: FText::FromString(Revision->GetDescription());
		const FOnRevisionSelected Callback = OnRevisionSelected;
		Menu.AddMenuEntry(Label, Tooltip, FSlateIcon(), FUIAction(FExecuteAction::CreateLambda([Callback, Revision]()
		{
			FSlateApplication::Get().DismissAllMenus();
			Callback.ExecuteIfBound(Revision);
		})));
	}
	MenuBox->ClearChildren();
	MenuBox->AddSlot().AutoHeight()[Menu.MakeWidget(nullptr, 500)];
}

#undef LOCTEXT_NAMESPACE
