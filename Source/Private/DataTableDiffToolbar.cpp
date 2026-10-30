// Copyright Meowoods 2026. All Rights Reserved.

#include "DataTableDiffToolbar.h"
#include "SDataTableHistoryDiff.h"
#include "SDataTableRevisionMenu.h"
#include "Engine/DataTable.h"
#include "ToolMenus.h"
#include "Toolkits/AssetEditorToolkitMenuContext.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "PropertyCustomizationHelpers.h"
#include "Styling/AppStyle.h"
#include "Misc/DelayedAutoRegister.h"
#include "Async/Async.h"
#include "AssetDefinition.h"

namespace
{
	const FName DataTableDiffTabId(TEXT("DataTableDiffTool"));

	class SDataTableDiffTab : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SDataTableDiffTab) {} SLATE_END_ARGS()
		void Construct(const FArguments& Args)
		{
			ChildSlot
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(8)
				[SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
					[SNew(STextBlock).Text(NSLOCTEXT("DataTableDiffTool", "TablePickerLabel", "DataTable"))]
					+ SHorizontalBox::Slot().AutoWidth()
					[SNew(SBox).MinDesiredWidth(320).MaxDesiredWidth(600)
						[SNew(SObjectPropertyEntryBox).AllowedClass(UDataTable::StaticClass()).AllowClear(false)
							.ObjectPath_Lambda([this]() { return TablePath; })
							.OnObjectChanged_Lambda([this](const FAssetData& Asset)
							{
								if (Asset.GetObjectPathString() == TablePath) { return; }
								if (UDataTable* Table = Cast<UDataTable>(Asset.GetAsset())) { SetTable(Table); }
							})]]]
				+ SVerticalBox::Slot().FillHeight(1)
				[SAssignNew(Content, SBox)
					[SNew(STextBlock).Margin(8).Text(NSLOCTEXT("DataTableDiffTool", "ChooseTable", "Select a DataTable to compare its revision history."))]]
			];
		}

		void SetTable(UDataTable* Table, SDataTableHistoryDiff::FRevisionPtr Revision = nullptr, const FAssetDiffArgs* InitialComparison = nullptr)
		{
			TablePath = Table->GetPathName();
			Content->SetContent(SNew(SDataTableHistoryDiff).Table(Table).InitialRevision(Revision)
				.UseInitialComparison(InitialComparison != nullptr).InitialComparison(InitialComparison ? *InitialComparison : FAssetDiffArgs()));
		}

	private:
		FString TablePath;
		TSharedPtr<SBox> Content;
	};

	TSharedRef<SDockTab> SpawnDataTableDiffTab(const FSpawnTabArgs& Args)
	{
		return SNew(SDockTab).TabRole(ETabRole::NomadTab)[SNew(SDataTableDiffTab)];
	}

	void RegisterDiffTab()
	{
		if (!FGlobalTabmanager::Get()->HasTabSpawner(DataTableDiffTabId))
		{
			FGlobalTabmanager::Get()->RegisterNomadTabSpawner(DataTableDiffTabId, FOnSpawnTab::CreateStatic(&SpawnDataTableDiffTab))
				.SetDisplayName(NSLOCTEXT("DataTableDiffTool", "TabTitle", "DataTable Diff Tool"))
				.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("ClassIcon.DataTable")));
		}
	}

	TSharedPtr<SDataTableDiffTab> OpenDiffTab()
	{
		RegisterDiffTab();
		const TSharedPtr<SDockTab> Tab = FGlobalTabmanager::Get()->TryInvokeTab(DataTableDiffTabId);
		return Tab ? StaticCastSharedRef<SDataTableDiffTab>(Tab->GetContent()).ToSharedPtr() : nullptr;
	}
}

void UnregisterDataTableDiffTab()
{
	if (!FSlateApplication::IsInitialized()) { return; }
	if (const auto Tab = FGlobalTabmanager::Get()->FindExistingLiveTab(DataTableDiffTabId)) { Tab->RequestCloseTab(); }
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(DataTableDiffTabId);
}

void OpenDataTableDiffTool(UDataTable* Table, TSharedPtr<ISourceControlRevision, ESPMode::ThreadSafe> Revision)
{
	if (!Table || !FSlateApplication::IsInitialized()) { return; }
	if (const auto View = OpenDiffTab()) { View->SetTable(Table, Revision); }
}

void OpenDataTableDiffTool(const FAssetDiffArgs& Args)
{
	UDataTable* Table = Cast<UDataTable>(Args.NewAsset ? Args.NewAsset : Args.OldAsset);
	if (!Table || !FSlateApplication::IsInitialized()) { return; }
	if (const auto View = OpenDiffTab()) { View->SetTable(Table, nullptr, &Args); }
}

void RegisterDataTableDiffToolbar()
{
	if (!IsInGameThread())
	{
		AsyncTask(ENamedThreads::GameThread, []() { RegisterDataTableDiffToolbar(); });
		return;
	}
	RegisterDiffTab();
	UToolMenus::Get()->UnregisterOwnerByName(TEXT("DataTableDiffToolToolbar"));
	FToolMenuOwnerScoped Owner(FName(TEXT("DataTableDiffToolToolbar")));
	UToolMenu* Toolbar = UToolMenus::Get()->ExtendMenu(TEXT("AssetEditor.DataTableEditor.ToolBar"));
	FToolMenuSection& Section = Toolbar->FindOrAddSection(TEXT("DataTableDiffTool"));
	Section.InsertPosition = FToolMenuInsert(TEXT("Asset"), EToolMenuInsertType::After);
	Section.AddDynamicEntry(TEXT("Diff"), FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& Generated)
	{
		const UAssetEditorToolkitMenuContext* Context = Generated.FindContext<UAssetEditorToolkitMenuContext>();
		if (!Context) { return; }
		const TArray<UObject*> Objects = Context->GetEditingObjects();
		if (Objects.Num() != 1) { return; }
		UDataTable* Table = Cast<UDataTable>(Objects[0]);
		if (!Table) { return; }
		const TWeakObjectPtr<UDataTable> WeakTable(Table);
		FToolMenuEntry& Entry = Generated.AddEntry(FToolMenuEntry::InitComboButton(TEXT("Diff"), FUIAction(),
			FOnGetContent::CreateLambda([WeakTable]() -> TSharedRef<SWidget>
			{
				return SNew(SDataTableRevisionMenu).Table(WeakTable.Get())
					.OnRevisionSelected_Lambda([WeakTable](TSharedPtr<ISourceControlRevision, ESPMode::ThreadSafe> Revision)
					{
						OpenDataTableDiffTool(WeakTable.Get(), Revision);
					});
			}), NSLOCTEXT("DataTableDiffTool", "ToolbarDiff", "Diff"),
			NSLOCTEXT("DataTableDiffTool", "ToolbarDiffTip", "Diff against previous revisions"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("BlueprintDiff.ToolbarIcon"))));
		Entry.StyleNameOverride = TEXT("CalloutToolbar");
	}));
	UToolMenus::Get()->RefreshMenuWidget(TEXT("AssetEditor.DataTableEditor.ToolBar"));
}

// UE provides this hook specifically to refresh editor registrations after Live Coding.
static FDelayedAutoRegisterHelper RegisterToolbarAfterInit(EDelayedRegisterRunPhase::EndOfEngineInit, []()
{
	if (!IsRunningCommandlet())
	{
		// Patch DLL static initializers can run on the Live Coding worker thread.
		AsyncTask(ENamedThreads::GameThread, []()
		{
			UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateStatic(&RegisterDataTableDiffToolbar));
		});
	}
}, true);
