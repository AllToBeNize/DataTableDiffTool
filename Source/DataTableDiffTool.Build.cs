// Copyright Meowoods 2026. All Rights Reserved.

using UnrealBuildTool;

public class DataTableDiffTool : ModuleRules
{
	public DataTableDiffTool(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PrivateDependencyModuleNames.AddRange(new[] {
			"Core", "CoreUObject", "Engine", "Slate", "SlateCore", "ApplicationCore", "InputCore",
			"UnrealEd", "AssetDefinition", "EngineAssetDefinitions", "AssetTools",
			"AssetRegistry", "ToolMenus", "ContentBrowser", "SourceControl", "DataTableEditor", "DeveloperSettings", "PropertyEditor"
		});
	}
}
