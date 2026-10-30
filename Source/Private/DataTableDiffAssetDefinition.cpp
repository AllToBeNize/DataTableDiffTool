// Copyright Meowoods 2026. All Rights Reserved.

#include "DataTableDiffAssetDefinition.h"
#include "DataTableDiffSettings.h"
#include "DataTableDiffToolbar.h"
#include "AssetDefinitionRegistry.h"
#include "UObject/StrongObjectPtr.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "IAssetTypeActions.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"

namespace
{
	TStrongObjectPtr<UAssetDefinition> PreviousTableDefinition;
	TSharedPtr<IAssetTypeActions> PreviousTableActions;
}

bool UDataTableDiffAssetDefinition::UsesVisualDiff()
{
	return GetDefault<UDataTableDiffSettings>()->ComparisonTool == EDataTableDiffTool::VisualDiff;
}

bool UDataTableDiffAssetDefinition::IsTablePair(const FAssetDiffArgs& Args)
{
	const auto* Old = Cast<UDataTable>(Args.OldAsset);
	const auto* New = Cast<UDataTable>(Args.NewAsset);
	return Old && New && Args.OldRevision.Revision.IsEmpty() && Args.NewRevision.Revision.IsEmpty()
		&& !FPackageName::IsTempPackage(Old->GetOutermost()->GetName()) && !FPackageName::IsTempPackage(New->GetOutermost()->GetName());
}

bool UDataTableDiffAssetDefinition::UsesVisualDiff(const FAssetDiffArgs& Args)
{
	const auto* Settings = GetDefault<UDataTableDiffSettings>();
	return !IsTablePair(Args) && Settings->ComparisonTool == EDataTableDiffTool::VisualDiff;
}

EAssetCommandResult UDataTableDiffAssetDefinition::PerformAssetDiff(const FAssetDiffArgs& Args) const
{
	if (!UsesVisualDiff(Args)) { return Super::PerformAssetDiff(Args); }
	if (!Cast<UDataTable>(Args.OldAsset) && !Cast<UDataTable>(Args.NewAsset)) { return EAssetCommandResult::Unhandled; }
	OpenDataTableDiffTool(Args);
	return EAssetCommandResult::Handled;
}

void InstallDataTableDiffRouter()
{
	UAssetDefinitionRegistry* Registry = UAssetDefinitionRegistry::Get();
	if (!Registry) { return; }
	auto* Router = GetMutableDefault<UDataTableDiffAssetDefinition>();
	const auto* Current = Registry->GetAssetDefinitionForClass(UDataTable::StaticClass());
	if (Current == Router) { return; }
	PreviousTableDefinition.Reset(const_cast<UAssetDefinition*>(Current));
	auto& Tools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
	PreviousTableActions = Tools.GetAssetTypeActionsForClass(UDataTable::StaticClass()).Pin();
	// UE caches the definition proxy by asset class, so replacing only the registry leaves an old dispatcher alive.
	if (PreviousTableActions) { Tools.UnregisterAssetTypeActions(PreviousTableActions.ToSharedRef()); }
	if (Current) { Registry->UnregisterAssetDefinition(const_cast<UAssetDefinition*>(Current)); }
	Registry->RegisterAssetDefinition(Router);
	Tools.GetAssetTypeActionsForClass(UDataTable::StaticClass());
}

void RemoveDataTableDiffRouter()
{
	if (auto* Registry = UAssetDefinitionRegistry::Get())
	{
		if (Registry->GetAssetDefinitionForClass(UDataTable::StaticClass()) == GetDefault<UDataTableDiffAssetDefinition>())
		{
			auto* ToolsModule = FModuleManager::GetModulePtr<FAssetToolsModule>(TEXT("AssetTools"));
			if (ToolsModule)
			{
				if (auto Actions = ToolsModule->Get().GetAssetTypeActionsForClass(UDataTable::StaticClass()).Pin())
				{
					ToolsModule->Get().UnregisterAssetTypeActions(Actions.ToSharedRef());
				}
			}
			Registry->UnregisterAssetDefinition(GetMutableDefault<UDataTableDiffAssetDefinition>());
			if (PreviousTableDefinition.IsValid()) { Registry->RegisterAssetDefinition(PreviousTableDefinition.Get()); }
			if (ToolsModule && PreviousTableActions) { ToolsModule->Get().RegisterAssetTypeActions(PreviousTableActions.ToSharedRef()); }
		}
	}
	PreviousTableDefinition.Reset();
	PreviousTableActions.Reset();
}
