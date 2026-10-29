# Fab listing preparation

| Field | Value |
| --- | --- |
| Product name | DataTable Diff Tool |
| Publisher | Meowoods |
| Support email | 2184196946@qq.com |
| Tagline | Compare. Review. Restore. |
| Product type | Unreal Engine Code Plugin / Tools & Plugins |
| Version | 1.0.0 |
| Module | DataTableDiffTool (Editor) |
| Suggested tags | DataTable, Diff, Editor Tools, Source Control, Perforce |
| Verified environment | Windows, custom UE 5.8.1; Perforce |
| Dependencies | EngineAssetDefinitions and built-in Unreal editor modules |
| Content | Source and documentation; no runtime module or bundled assets |

## English description

Compare DataTable revisions directly in Unreal Editor. Review row and property changes side by side, then restore only the changes you need to your local table.

- Compare nested structs, arrays, Maps and Sets.
- Search changes and navigate synchronized tables and property views.
- Restore individual changes with Undo / Redo support.
- Switch DataTables in a dockable tab with saved panel proportions and column widths.
- Open from the DataTable editor or Content Browser, and switch back to the native external diff workflow in Editor Preferences.

Source control with revision history is required. Perforce has been tested. Restoring is available only when the right version is Local; save the asset after restoring.

## 中文描述

在 Unreal 编辑器中直接比较 DataTable 历史版本，并排查看行与属性的变化，按需恢复到本地表格。

- 支持嵌套结构体、数组、Map 和 Set。
- 搜索并定位变更，同步浏览两侧表格和属性。
- 按项恢复数据，支持撤销与重做。
- 可停靠页签内切换 DataTable，自动保存面板比例和列宽。
- 支持 DataTable 编辑器和内容浏览器入口，可在偏好设置中切回原生外部比较工具。

需要连接支持历史版本的源代码管理服务，目前已实测 Perforce。仅右侧为 Local 时支持恢复，恢复后需要手动保存资产。

## FAQ

**Where can I open the tool? / 在哪里打开？**

Right-click a DataTable in the Content Browser and select **DataTable Diff Tool**, or choose a revision from the DataTable editor's **Diff** toolbar menu. By default, **Revision Control > Diff Against Depot** also opens this tool.

在内容浏览器右键 DataTable，选择 **DataTable Diff Tool**；或打开 DataTable，通过工具栏 **Diff** 菜单选择历史版本。默认情况下，**Revision Control > Diff Against Depot** 也会使用本工具。

**How do I switch back to the native diff tool? / 如何切回原生比较工具？**

In **Editor Preferences > Plugins > DataTable Diff Tool**, set **Revision Comparison Tool** to **Native External Tool (e.g. P4Merge)**. Set the executable under **Loading & Saving > Text Diff Tool Path**. The change takes effect immediately for native revision-comparison entry points.

在 **Editor Preferences > Plugins > DataTable Diff Tool** 中，将 **Revision Comparison Tool** 设为 **Native External Tool (e.g. P4Merge)**。外部程序路径在 **Loading & Saving > Text Diff Tool Path** 中设置，原生版本比较入口立即采用新设置。

## Before publishing

- Publisher: Meowoods. Support: 2184196946@qq.com. Match these values in the seller account; confirm the documentation URL, price and license selections before publishing.
- Build and test against every stock Unreal Engine version and platform selected on the listing. A custom UE build does not establish stock-version support.
- Run a clean plugin packaging build for each supported engine; verify history, restore, Undo / Redo and layout persistence after restarting that engine.
- Select a plugin-browser icon if desired; none is bundled in this source snapshot.
- Project-specific demonstration DataTables and their C++ row types are not distributed with the plugin. Screenshots show illustrative project data.
- Mask private source-control names and metadata before publishing screenshots. Format-converted screenshots retain their original visible content.
- Source ZIP snapshots omit Binaries, Intermediate, local user configuration and project assets; they are preparation snapshots rather than engine-certified packages.

## Official references

- [Fab file format and structure requirements](https://dev.epicgames.com/documentation/fab/asset-file-format-and-structure-requirements-in-fab)
- [Publishing assets on Fab](https://dev.epicgames.com/documentation/fab/publishing-assets-for-sale-or-free-download-in-fab)

Media gallery images: JPEG or PNG, at least 1920 × 1080, less than 3 MB each, less than 25 MB combined. Recheck the linked requirements at submission time.
