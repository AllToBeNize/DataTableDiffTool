#include "DataTableDiffToolbar.h"
#include "SDataTableHistoryDiff.h"
#include "SDataTableRevisionMenu.h"
#include "Engine/DataTable.h"
#include "ToolMenus.h"
#include "Toolkits/AssetEditorToolkitMenuContext.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Styling/AppStyle.h"
#include "Misc/DelayedAutoRegister.h"
#include "Async/Async.h"
#include "AssetDefinition.h"

void OpenDataTableDiffTool(UDataTable* Table, TSharedPtr<ISourceControlRevision, ESPMode::ThreadSafe> Revision)
{
	if (!Table || !FSlateApplication::IsInitialized()) { return; }
	FSlateApplication::Get().AddWindow(SNew(SWindow)
		.Title(FText::FromString(TEXT("DataTable Diff Tool - ") + Table->GetName())).ClientSize(FVector2D(1250, 800))
		[SNew(SDataTableHistoryDiff).Table(Table).InitialRevision(Revision)]);
}

void OpenDataTableDiffTool(const FAssetDiffArgs& Args)
{
	UDataTable* Table = Cast<UDataTable>(Args.NewAsset ? Args.NewAsset : Args.OldAsset);
	if (!Table || !FSlateApplication::IsInitialized()) { return; }
	FSlateApplication::Get().AddWindow(SNew(SWindow)
		.Title(FText::FromString(TEXT("DataTable Diff Tool - ") + Table->GetName())).ClientSize(FVector2D(1250, 800))
		[SNew(SDataTableHistoryDiff).Table(Table).UseInitialComparison(true).InitialComparison(Args)]);
}

void RegisterDataTableDiffToolbar()
{
	if (!IsInGameThread())
	{
		AsyncTask(ENamedThreads::GameThread, []() { RegisterDataTableDiffToolbar(); });
		return;
	}
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
