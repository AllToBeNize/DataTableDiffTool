# DataTable Diff Tool

独立的 Unreal Engine 编辑器代码插件，不依赖项目模块、脚本插件或项目资源。当前版本在 Windows 的定制 UE 5.8.1 下编译验证；原版引擎版本及其他平台尚未验证。

## 功能

- 内容浏览器右键单张 DataTable → DataTable Diff Tool；默认比较 Depot 与 Local。
- DataTable 编辑器顶部 Diff 菜单异步查询历史，选择任意版本与当前数据比较。
- 接管原生 Diff Against Depot 和历史版本比较；偏好设置中可切换为 UE 原生外部工具。
- 左右分别选择历史版本，右侧支持 Local；Local 包含尚未保存的内存修改。
- 显示版本号、提交人、时间和提交说明。
- 按行名和反射属性比较，显示新增、删除、修改及结构差异；保留原有行序和字段定义顺序。
- 嵌套结构体递归比较；数组通过最长公共子序列对齐，中间插入或删除不会导致后续元素全部误报。
- Map 按键对齐，支持逐键增删、值变化及值内嵌套属性的比较与恢复；键本身的变化显示为删除和新增。
- Set 按元素内容匹配，忽略容器内部顺序，支持单个元素的恢复；结构体元素可以展开查看，元素内容变化显示为删除和新增，不直接修改参与哈希的字段。
- 变更树搜索、字段跳转、属性展开、滚动和列宽同步；下方属性行仅跟随左侧变更项高亮，不接受手动行选择；属性文本只读，支持选中文字复制和自动换行。
- 点击任一侧上方表格行时，左侧变更树同步选中并定位对应行；没有变更或被搜索过滤的行会清除变更树旧选中。
- 可停靠页签内搜索和切换 DataTable；保存面板分割比例、表格与属性列宽到本机用户配置。
- 选中的变更树行和有差异的属性详情行显示 UE 原生恢复箭头；两侧详情按钮均从左侧版本恢复到 Local。
- 属性区复用 UE 原生 Details 的分组与行配色、标准只读文本框和布尔复选框；保留差异文字颜色及左右定位高亮。
- 右侧为 Local 时，支持恢复整行、结构体、数组、数组元素或字段；支持 Undo / Redo。
- 恢复仅写回内存并标记待保存，不自动保存资产或执行版本库 revert。
- Perforce 历史超过 UE 缓存范围时补齐版本号，较旧内容按需下载。
- 历史文件异步下载，下载完成后在主线程加载资产；没有版本列表时，重新打开版本菜单会自动重试历史查询。

## 暂不包含

- 两张不同 DataTable 的比较窗口、双选比较菜单、两表工具设置；原生 Diff Against 的两表行为交回引擎。
- 自动合并、行重命名识别。
- 自动化测试、测试类型、测试友元声明和测试运行入口。
- 运行时模块及项目专属资源。

## 安装与使用

将整个 DataTableDiffTool 目录放入工程 Plugins，编译并重启编辑器。插件默认启用。

编辑器偏好设置 → 插件 → DataTable Diff Tool → Revision Comparison Tool，可选择本插件或 Native External Tool。外部工具沿用 UE 的 Text Diff Tool Path。

历史由当前 UE 源代码管理提供者提供，需先连接源代码管理。两个历史版本互比只读；右侧为 Local 时才允许恢复。结构或类型不兼容时禁用相应恢复。

## Installation and usage

Copy `DataTableDiffTool` into your project's `Plugins` directory. Build the project's Editor target with a compatible Unreal C++ toolchain, then restart the editor. The plugin enables Unreal's built-in EngineAssetDefinitions dependency; no project-specific assets or external libraries are required.

Connect source control and select a DataTable with revision history. Right-click it in the Content Browser and choose **DataTable Diff Tool**, or choose a revision from the DataTable editor's **Diff** toolbar menu. By default, native **Revision Control > Diff Against Depot** and revision-history comparisons also use the plugin.

Select a DataTable in the dockable tab and choose revisions independently on both sides. The right-side **Local** option includes unsaved in-memory changes. Search and navigate changes; both sides synchronize scrolling, expansion and column widths. Panel proportions and column widths persist in local editor user settings. Text is read-only and supports selection and copying.

Use the reset arrow beside the selected change or a changed Details row to restore data from the left revision into Local. Both Details panes perform the same operation. Undo / Redo is supported. Save the asset to persist the edit; restoring does not revert or submit files in source control. Historical-to-historical comparisons are read-only. Comparing two different DataTables and automatic merging are not provided.

To use the native external tool, open **Editor Preferences > Plugins > DataTable Diff Tool** and set **Revision Comparison Tool** to **Native External Tool (e.g. P4Merge)**. Configure the executable under **Loading & Saving > Text Diff Tool Path**. This immediately affects native revision-comparison entry points. The explicit **DataTable Diff Tool** command still opens this plugin.

History and revision downloads use Unreal's source-control provider interfaces. Perforce has been tested; other providers depend on their history and download implementations and have not been validated. This version has been built and used on Windows with a custom UE 5.8.1 engine. Stock launcher engine versions and other platforms have not been validated.
