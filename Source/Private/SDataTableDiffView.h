#pragma once

#include "DataTableDiffModel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STreeView.h"

class SHeaderRow;
class SScrollBox;
class SSplitter;

struct FTableDiffJump
{
	TSharedPtr<FTableRowDiff> Row;
	FString FieldPath;
	FString Label;
	FTableRestoreTarget RestoreTarget;
	TArray<TSharedPtr<FTableDiffJump>> Children;
	TArray<TSharedPtr<FTableDiffJump>> FilteredChildren;
	ETableDiffKind Kind = ETableDiffKind::Modified;
};

class SDataTableDiffView : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_RetVal_TwoParams(bool, FCanRevertChange, FName, const FTableRestoreTarget&);
	DECLARE_DELEGATE_TwoParams(FRevertChange, FName, const FTableRestoreTarget&);
	SLATE_BEGIN_ARGS(SDataTableDiffView) {}
		SLATE_ARGUMENT(FTableDiffModel, Model)
		SLATE_ARGUMENT(FString, OldLabel)
		SLATE_ARGUMENT(FString, NewLabel)
		SLATE_ARGUMENT(TSharedPtr<SWidget>, LeftControls)
		SLATE_ARGUMENT(TSharedPtr<SWidget>, RightControls)
		SLATE_EVENT(FCanRevertChange, OnCanRevertChange)
		SLATE_EVENT(FRevertChange, OnRevertChange)
	SLATE_END_ARGS()
	void Construct(const FArguments& Args);
	void SetComparison(FTableDiffModel InModel, const FString& OldLabel, const FString& NewLabel);
private:
	TSharedRef<SWidget> MakePane(bool bLocal);
	TSharedRef<ITableRow> GenerateTableRow(TSharedPtr<FTableRowDiff> Row, const TSharedRef<STableViewBase>& Owner, bool bLocal);
	TSharedRef<ITableRow> GenerateDetailRow(TSharedPtr<FTableFieldDiff> Field, const TSharedRef<STableViewBase>& Owner, bool bLocal);
	TSharedRef<ITableRow> GenerateJump(TSharedPtr<FTableDiffJump> Jump, const TSharedRef<STableViewBase>& Owner);
	void JumpTo(TSharedPtr<FTableDiffJump> Jump, ESelectInfo::Type SelectInfo);
	void SelectTableRow(TSharedPtr<FTableRowDiff> Row, ESelectInfo::Type SelectInfo);
	void FocusRow(TSharedPtr<FTableRowDiff> Row, const FString& FieldPath);
	void RefreshJumps();
	bool CanRevertJump(TSharedPtr<FTableDiffJump> Jump) const;
	FReply RevertJump(TSharedPtr<FTableDiffJump> Jump);
	void SyncScroll(double Offset, bool bLocal);
	void SyncDetailScroll(double Offset, bool bLocal);
	void SyncExpansion(TSharedPtr<FTableFieldDiff> Field, bool bExpanded, bool bLocal);
	float ColumnWidth(const FString& Path) const;
	float TableWidth() const;
	void ResizeColumn(float Width, FString Path);
	float ShareDetailHeight(const FString& Path, bool bLocal, float Height);
	void ResizePane(float Fraction);
	FTableDiffModel Model;
	TArray<FString> Columns;
	TMap<FString, float> ColumnWidths;
	TMap<FString, FVector2D> DetailHeights;
	TSet<FString> ExpandedPaths;
	float TableFraction = 0.65f;
	TArray<TSharedPtr<FTableDiffJump>> AllJumps;
	TArray<TSharedPtr<FTableDiffJump>> VisibleJumps;
	TArray<TSharedPtr<FTableFieldDiff>> SelectedFields;
	TSharedPtr<FTableRowDiff> SelectedRow;
	FString SelectedField;
	FString Search;
	FString LocalLabel;
	FString DepotLabel;
	bool bSyncing = false;
	FCanRevertChange CanRevertChange;
	FRevertChange RevertChange;
	TSharedPtr<SWidget> LeftControls;
	TSharedPtr<SWidget> RightControls;
	TSharedPtr<STreeView<TSharedPtr<FTableDiffJump>>> JumpList;
	TSharedPtr<SListView<TSharedPtr<FTableRowDiff>>> LocalTable;
	TSharedPtr<SListView<TSharedPtr<FTableRowDiff>>> DepotTable;
	TSharedPtr<STreeView<TSharedPtr<FTableFieldDiff>>> LocalDetails;
	TSharedPtr<STreeView<TSharedPtr<FTableFieldDiff>>> DepotDetails;
	TSharedPtr<SHeaderRow> LocalHeader;
	TSharedPtr<SHeaderRow> DepotHeader;
	TSharedPtr<SScrollBox> LocalHorizontal;
	TSharedPtr<SScrollBox> DepotHorizontal;
	TSharedPtr<SSplitter> LocalSplitter;
	TSharedPtr<SSplitter> DepotSplitter;
};
