// Copyright Meowoods 2026. All Rights Reserved.

#include "SDataTableHistoryDiff.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Engine/DataTable.h"
#include "Modules/ModuleManager.h"
#include "ContentBrowserMenuContexts.h"
#include "ToolMenus.h"
#include "DataTableDiffToolbar.h"
#include "DataTableDiffAssetDefinition.h"

class FDataTableDiffToolModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		InstallDataTableDiffRouter();
		if (!IsRunningCommandlet())
		{
			UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FDataTableDiffToolModule::RegisterMenus));
		}
	}
	virtual void ShutdownModule() override
	{
		UnregisterDataTableDiffTab();
		RemoveDataTableDiffRouter();
		if (UToolMenus::IsToolMenuUIEnabled())
		{
			UToolMenus::UnRegisterStartupCallback(this);
			UToolMenus::UnregisterOwner(this);
			UToolMenus::Get()->UnregisterOwnerByName(TEXT("DataTableDiffToolToolbar"));
		}
	}
	virtual bool SupportsDynamicReloading() override { return false; }
private:
	void RegisterMenus()
	{
		FToolMenuOwnerScoped Owner(this);
		UToolMenu* Menu = UE::ContentBrowser::ExtendToolMenu_AssetContextMenu(UDataTable::StaticClass());
		Menu->FindOrAddSection(TEXT("GetAssetActions")).AddDynamicEntry(TEXT("VisualDiffTables"),
			FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& Section)
			{
				const UContentBrowserAssetContextMenuContext* Context = Section.FindContext<UContentBrowserAssetContextMenuContext>();
				if (!Context || Context->SelectedAssets.Num() != 1) { return; }
				for (const FAssetData& Asset : Context->SelectedAssets)
				{
					if (!Asset.IsInstanceOf(UDataTable::StaticClass())) { return; }
				}
				FToolUIAction Action;
				Action.ExecuteAction = FToolMenuExecuteAction::CreateLambda([](const FToolMenuContext& MenuContext)
				{
					const UContentBrowserAssetContextMenuContext* Selection = MenuContext.FindContext<UContentBrowserAssetContextMenuContext>();
					if (!Selection || Selection->SelectedAssets.Num() != 1) { return; }
					TArray<UDataTable*> Tables = Selection->LoadSelectedObjects<UDataTable>();
					if (Tables.Num() == 1) { OpenDataTableDiffTool(Tables[0]); }
				});
				Section.AddMenuEntry(TEXT("VisualDiffDataTable"),
					NSLOCTEXT("DataTableDiffTool", "VisualDiff", "DataTable Diff Tool"),
					NSLOCTEXT("DataTableDiffTool", "VisualDiffTip", "Compare the latest depot revision against the workspace, then select another revision in the diff window."), FSlateIcon(), Action);
			}));
	}
};

IMPLEMENT_MODULE(FDataTableDiffToolModule, DataTableDiffTool)
