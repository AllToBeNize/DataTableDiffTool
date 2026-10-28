#include "SDataTableDiffView.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Input/SButton.h"
#include "Styling/StyleColors.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SScrollBar.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/SExpanderArrow.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "DataTableDiffTool"

namespace
{
	constexpr float NameWidth = 160;
	constexpr float FieldWidth = 180;
	const FLinearColor FocusedBackground(0.2f, 0.4f, 0.75f, 0.45f);
	const FString RowWidthKey = TEXT("@RowName");
	FSlateColor DiffColor(ETableDiffKind Kind)
	{
		switch (Kind)
		{
		case ETableDiffKind::Added: return FLinearColor(0.25f, 0.85f, 0.4f);
		case ETableDiffKind::Removed: return FLinearColor(1.0f, 0.35f, 0.35f);
		case ETableDiffKind::Modified: return FLinearColor(1.0f, 0.75f, 0.25f);
		default: return FStyleColors::Foreground;
		}
	}
	bool RowAbsent(const FTableRowDiff& Row, bool bLocal)
	{
		return bLocal ? Row.Kind == ETableDiffKind::Removed : Row.Kind == ETableDiffKind::Added;
	}
	FString FieldValue(const FTableFieldDiff& Field, bool bLocal, bool bAbsentRow = false)
	{
		if (bAbsentRow) { return TEXT("\u2014"); }
		const bool bMissing = bLocal ? Field.Kind == ETableDiffKind::Removed : Field.Kind == ETableDiffKind::Added;
		return bMissing ? LOCTEXT("AbsentField", "Field not in this version").ToString() : bLocal ? Field.NewValue : Field.OldValue;
	}
	class STableDiffJumpRow : public STableRow<TSharedPtr<FTableDiffJump>>
	{
	public:
		SLATE_BEGIN_ARGS(STableDiffJumpRow) {} SLATE_ARGUMENT(ETableDiffKind, Kind) SLATE_END_ARGS()
		void Construct(const FArguments& Args, const TSharedRef<STableViewBase>& Owner)
		{
			Kind = Args._Kind;
			STableRow::Construct(STableRow::FArguments().Padding(5), Owner);
		}
		virtual void ConstructChildren(ETableViewMode::Type Mode, const TAttribute<FMargin>& Padding, const TSharedRef<SWidget>& RowContent) override
		{
			STableRow::ConstructChildren(Mode, Padding, RowContent);
			const TSharedRef<SWidget> TreeContent = ChildSlot.GetWidget();
			const FText Marker = FText::FromString(Kind == ETableDiffKind::Added ? TEXT("+") : Kind == ETableDiffKind::Removed ? TEXT("-") : TEXT("~"));
			ChildSlot.Padding(0)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[SNew(SBox).WidthOverride(18).HAlign(HAlign_Center)
					[SNew(STextBlock).Text(Marker).ColorAndOpacity(DiffColor(Kind))]]
				+ SHorizontalBox::Slot().FillWidth(1)[TreeContent]
			];
		}
	private:
		ETableDiffKind Kind = ETableDiffKind::Unchanged;
	};
	class STableDiffGridRow : public SMultiColumnTableRow<TSharedPtr<FTableRowDiff>>
	{
	public:
		SLATE_BEGIN_ARGS(STableDiffGridRow) {}
			SLATE_ARGUMENT(TSharedPtr<FTableRowDiff>, Row)
			SLATE_ARGUMENT(const TArray<FString>*, Columns)
			SLATE_ARGUMENT(bool, Local)
			SLATE_ARGUMENT(TFunction<bool(const FString&)>, IsFocused)
		SLATE_END_ARGS()
		void Construct(const FArguments& Args, const TSharedRef<STableViewBase>& Owner)
		{
			Row = Args._Row; Columns = Args._Columns; bLocal = Args._Local; IsFocused = Args._IsFocused;
			SMultiColumnTableRow::Construct(FSuperRowType::FArguments().Padding(2), Owner);
		}
		virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& Column) override
		{
			FString Path;
			const bool bAbsentRow = RowAbsent(*Row, bLocal);
			FString Value = bAbsentRow ? LOCTEXT("AbsentRow", "Row not in this revision").ToString() : Row->Name.ToString();
			ETableDiffKind Kind = Row->Kind;
			if (Column != TEXT("RowName"))
			{
				const int32 Index = FCString::Atoi(*Column.ToString().RightChop(1));
				if (!Columns->IsValidIndex(Index)) { return SNullWidget::NullWidget; }
				Path = (*Columns)[Index];
				const auto* Found = Row->Fields.FindByPredicate([&Path](const auto& Field) { return Field->Path == Path; });
				Value = Found ? FieldValue(**Found, bLocal, bAbsentRow) : TEXT("\u2014");
				Kind = Found ? (*Found)->Kind : ETableDiffKind::Unchanged;
			}
			return SNew(SBox).HeightOverride(24)
			[SNew(SBorder).Padding(3, 1).BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
				.BorderBackgroundColor_Lambda([this, Path]()
				{
					return IsFocused(Path) ? FocusedBackground : FLinearColor::Transparent;
				})
				[SNew(STextBlock).Text(FText::FromString(Value)).ToolTipText(FText::FromString(Value))
					.ColorAndOpacity(DiffColor(Kind)).OverflowPolicy(ETextOverflowPolicy::Ellipsis)]];
		}
	private:
		TSharedPtr<FTableRowDiff> Row;
		const TArray<FString>* Columns = nullptr;
		bool bLocal = false;
		TFunction<bool(const FString&)> IsFocused;
	};
	class SPropertyDiffTextBox : public SMultiLineEditableTextBox
	{
	public:
		SLATE_BEGIN_ARGS(SPropertyDiffTextBox) {} SLATE_ARGUMENT(FText, Value) SLATE_ARGUMENT(FSlateColor, Color) SLATE_END_ARGS()
		void Construct(const FArguments& Args)
		{
			SMultiLineEditableTextBox::Construct(SMultiLineEditableTextBox::FArguments()
				.Style(&FAppStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>(TEXT("NormalEditableTextBox")))
				.Text(Args._Value).ForegroundColor(Args._Color).ReadOnlyForegroundColor(Args._Color).FocusedForegroundColor(Args._Color)
				.IsReadOnly(true).AutoWrapText(true).WrappingPolicy(ETextWrappingPolicy::AllowPerCharacterWrapping).Margin(FMargin(4, 2)));
		}
		virtual FReply OnMouseWheel(const FGeometry& Geometry, const FPointerEvent& Event) override
		{
			return FReply::Unhandled();
		}
	};
	class STableDiffFocusedDetailRow : public SMultiColumnTableRow<TSharedPtr<FTableFieldDiff>>
	{
	public:
		SLATE_BEGIN_ARGS(STableDiffFocusedDetailRow) {}
			SLATE_ARGUMENT(TSharedPtr<FTableFieldDiff>, Field)
			SLATE_ARGUMENT(bool, Local)
			SLATE_ARGUMENT(bool, AbsentRow)
			SLATE_ARGUMENT(TFunction<float(float)>, SharedHeight)
			SLATE_ATTRIBUTE(bool, Focused)
		SLATE_END_ARGS()
		void Construct(const FArguments& Args, const TSharedRef<STableViewBase>& Owner)
		{
			Field = Args._Field; bLocal = Args._Local; bAbsentRow = Args._AbsentRow; SharedHeight = Args._SharedHeight;
			Focused = Args._Focused;
			SMultiColumnTableRow::Construct(FSuperRowType::FArguments().Padding(FMargin(0, 1))
				.Style(&FAppStyle::Get().GetWidgetStyle<FTableRowStyle>(TEXT("DetailsView.TreeView.TableRow"))), Owner);
		}
		virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& Column) override
		{
			if (Column == TEXT("Property"))
			{
				return SNew(SBorder).Padding(FMargin(6, 3)).BorderImage(FAppStyle::GetBrush(TEXT("DetailsView.CategoryMiddle")))
					.BorderBackgroundColor_Lambda([this]() { return DetailBackground(true); })
					[SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SExpanderArrow, SharedThis(this))]
					+ SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
					[SNew(STextBlock).Text(FText::FromString(!Field->Label.IsEmpty() ? Field->Label : Field->DisplayPath.IsEmpty() ? Field->Path : Field->DisplayPath))
						.ColorAndOpacity(DiffColor(Field->Kind)).OverflowPolicy(ETextOverflowPolicy::Ellipsis)
						.ToolTipText(FText::FromString(bLocal ? Field->NewType : Field->OldType))]];
			}
			if (Field->OldValue.IsEmpty() && Field->NewValue.IsEmpty() && !Field->Children.IsEmpty())
			{
				return SNew(SBorder).Padding(0).BorderImage(FAppStyle::GetBrush(TEXT("DetailsView.CategoryMiddle")))
					.BorderBackgroundColor_Lambda([this]() { return DetailBackground(); });
			}
			const FString Value = FieldValue(*Field, bLocal, bAbsentRow);
			TSharedRef<SWidget> ValueWidget = SNew(SPropertyDiffTextBox).Value(FText::FromString(Value)).Color(DiffColor(Field->Kind));
			if ((bLocal ? Field->NewType : Field->OldType) == TEXT("bool") && (Value == TEXT("True") || Value == TEXT("False")))
			{
				ValueWidget = SNew(SBox).HAlign(HAlign_Left).VAlign(VAlign_Center)
					[SNew(SCheckBox).IsChecked(Value == TEXT("True") ? ECheckBoxState::Checked : ECheckBoxState::Unchecked).IsEnabled(false)
						.ToolTipText(FText::FromString(Value))];
			}
			return SNew(SBorder).Padding(FMargin(6, 3))
				.BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
				.BorderBackgroundColor_Lambda([this]() { return DetailBackground(); })
				[ValueWidget];
		}
	private:
		FLinearColor DetailBackground(bool bAllowHover = false) const
		{
			if (Focused.Get()) { return FocusedBackground; }
			if (bAllowHover && IsHovered()) { return FStyleColors::Hover.GetSpecifiedColor(); }
			return Field->Children.IsEmpty() ? FStyleColors::Panel.GetSpecifiedColor() : FStyleColors::Header.GetSpecifiedColor();
		}
		virtual ESelectionMode::Type GetSelectionMode() const override { return ESelectionMode::None; }
		virtual FVector2D ComputeDesiredSize(float Scale) const override
		{
			FVector2D Size = SMultiColumnTableRow<TSharedPtr<FTableFieldDiff>>::ComputeDesiredSize(Scale);
			Size.Y = SharedHeight(Size.Y);
			return Size;
		}
		TSharedPtr<FTableFieldDiff> Field;
		bool bLocal = false;
		bool bAbsentRow = false;
		TFunction<float(float)> SharedHeight;
		TAttribute<bool> Focused;
	};
}

void SDataTableDiffView::Construct(const FArguments& Args)
{
	CanRevertChange = Args._OnCanRevertChange; RevertChange = Args._OnRevertChange;
	LeftControls = Args._LeftControls; RightControls = Args._RightControls;
	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(8)
		[SNew(STextBlock).Text_Lambda([this]()
		{
			return FText::Format(LOCTEXT("Summary", "{0} rows | +{1} added | -{2} removed | {3} modified"),
				FText::AsNumber(Model.Rows.Num()), FText::AsNumber(Model.Added), FText::AsNumber(Model.Removed), FText::AsNumber(Model.Modified));
		})]
		+ SVerticalBox::Slot().FillHeight(1).Padding(8)
		[
			SNew(SSplitter)
			+ SSplitter::Slot().Value(0.2f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 4, 8)
				[SNew(SSearchBox).HintText(LOCTEXT("SearchChanges", "Search changes"))
					.OnTextChanged_Lambda([this](const FText& Text) { Search = Text.ToString(); RefreshJumps(); })]
				+ SVerticalBox::Slot().AutoHeight()
				[SNew(STextBlock).Text_Lambda([this]() { return AllJumps.IsEmpty() ? LOCTEXT("NoChanges", "No differences detected") : LOCTEXT("Changes", "Changes - click to navigate"); })]
				+ SVerticalBox::Slot().FillHeight(1).Padding(0, 8, 0, 0)
				[SAssignNew(JumpList, STreeView<TSharedPtr<FTableDiffJump>>).TreeItemsSource(&VisibleJumps)
					.OnGetChildren_Lambda([](TSharedPtr<FTableDiffJump> Jump, TArray<TSharedPtr<FTableDiffJump>>& Children) { Children = Jump->FilteredChildren; })
					.OnGenerateRow(this, &SDataTableDiffView::GenerateJump).OnSelectionChanged(this, &SDataTableDiffView::JumpTo)
					.SelectionMode(ESelectionMode::Single)]
			]
			+ SSplitter::Slot().Value(0.4f)[MakePane(false)]
			+ SSplitter::Slot().Value(0.4f)[MakePane(true)]
		]
	];
	SetComparison(Args._Model, Args._OldLabel, Args._NewLabel);
}

bool SDataTableDiffView::CanRevertJump(TSharedPtr<FTableDiffJump> Jump) const
{
	return Jump && CanRevertChange.IsBound() && CanRevertChange.Execute(Jump->Row->Name, Jump->RestoreTarget);
}

FReply SDataTableDiffView::RevertJump(TSharedPtr<FTableDiffJump> Jump)
{
	if (CanRevertJump(Jump))
	{
		FocusRow(Jump->Row, Jump->FieldPath);
		RevertChange.ExecuteIfBound(Jump->Row->Name, Jump->RestoreTarget);
	}
	return FReply::Handled();
}

TSharedRef<SWidget> SDataTableDiffView::MakePane(bool bLocal)
{
	auto& Table = bLocal ? LocalTable : DepotTable;
	auto& Header = bLocal ? LocalHeader : DepotHeader;
	auto& Horizontal = bLocal ? LocalHorizontal : DepotHorizontal;
	auto& Details = bLocal ? LocalDetails : DepotDetails;
	auto& Splitter = bLocal ? LocalSplitter : DepotSplitter;
	TSharedRef<SScrollBar> TableScrollBar = SNew(SScrollBar).AlwaysShowScrollbar(true);
	TSharedRef<SScrollBar> DetailScrollBar = SNew(SScrollBar).AlwaysShowScrollbar(true);
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[(bLocal ? RightControls : LeftControls).IsValid() ? (bLocal ? RightControls : LeftControls).ToSharedRef() : SNullWidget::NullWidget]
		+ SVerticalBox::Slot().AutoHeight().Padding(6)
		[SNew(STextBlock).Text_Lambda([this, bLocal]() { return FText::FromString(bLocal ? LocalLabel : DepotLabel); })]
		+ SVerticalBox::Slot().FillHeight(1)
		[
			SAssignNew(Splitter, SSplitter).Orientation(Orient_Vertical)
			+ SSplitter::Slot().Value_Lambda([this]() { return TableFraction; })
				.OnSlotResized(this, &SDataTableDiffView::ResizePane)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1)
				[
				SAssignNew(Horizontal, SScrollBox).Orientation(Orient_Horizontal)
					.ConsumeMouseWheel(EConsumeMouseWheel::Never)
					.OnUserScrolled_Lambda([this, bLocal](float Offset)
					{
						if (bSyncing) { return; }
						TGuardValue<bool> Guard(bSyncing, true);
						(bLocal ? DepotHorizontal : LocalHorizontal)->SetScrollOffset(Offset);
					})
				+ SScrollBox::Slot()
				[
					SNew(SBox).WidthOverride_Lambda([this]() { return FOptionalSize(TableWidth()); })
					[
						SAssignNew(Table, SListView<TSharedPtr<FTableRowDiff>>).ListItemsSource(&Model.Rows)
							.ExternalScrollbar(TableScrollBar).ConsumeMouseWheel(EConsumeMouseWheel::Always)
							.OnGenerateRow(this, &SDataTableDiffView::GenerateTableRow, bLocal)
							.OnSelectionChanged(this, &SDataTableDiffView::SelectTableRow)
							.OnMouseButtonClick_Lambda([this](TSharedPtr<FTableRowDiff> Row)
							{
								if (SelectedRow == Row && !SelectedField.IsEmpty()) { SelectTableRow(Row, ESelectInfo::OnMouseClick); }
							})
							.OnListViewScrolled(this, &SDataTableDiffView::SyncScroll, bLocal)
							.SelectionMode(ESelectionMode::Single).HeaderRow(SAssignNew(Header, SHeaderRow))
					]
				]
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(2, 0)[TableScrollBar]
			]
			+ SSplitter::Slot().Value_Lambda([this]() { return 1.0f - TableFraction; })
				.OnSlotResized_Lambda([this](float Fraction) { ResizePane(1.0f - Fraction); })
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(6)
				[SNew(STextBlock).Text_Lambda([this, bLocal]()
				{
					if (!SelectedRow) { return LOCTEXT("SelectRow", "Select a row to inspect all properties"); }
					return RowAbsent(*SelectedRow, bLocal)
						? FText::Format(LOCTEXT("AbsentRowHeading", "{0} - Row not in this revision"), FText::FromName(SelectedRow->Name))
						: FText::FromName(SelectedRow->Name);
				})]
				+ SVerticalBox::Slot().FillHeight(1)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1)
					[
					SAssignNew(Details, STreeView<TSharedPtr<FTableFieldDiff>>).TreeItemsSource(&SelectedFields)
						.OnGetChildren_Lambda([](TSharedPtr<FTableFieldDiff> Field, TArray<TSharedPtr<FTableFieldDiff>>& Children) { Children = Field->Children; })
						.OnExpansionChanged(this, &SDataTableDiffView::SyncExpansion, bLocal)
						.ExternalScrollbar(DetailScrollBar).ConsumeMouseWheel(EConsumeMouseWheel::Always)
						.OnGenerateRow(this, &SDataTableDiffView::GenerateDetailRow, bLocal).SelectionMode(ESelectionMode::None)
						.OnTreeViewScrolled(this, &SDataTableDiffView::SyncDetailScroll, bLocal)
						.HeaderRow(SNew(SHeaderRow)
							+ SHeaderRow::Column(TEXT("Property")).DefaultLabel(LOCTEXT("Property", "Property")).FillWidth(0.4f)
							+ SHeaderRow::Column(TEXT("Value")).DefaultLabel(LOCTEXT("Value", "Value")).FillWidth(0.6f))
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(2, 0)[DetailScrollBar]
				]
			]
		];
}

void SDataTableDiffView::SetComparison(FTableDiffModel InModel, const FString& OldLabel, const FString& NewLabel)
{
	const FName PreviousName = SelectedRow ? SelectedRow->Name : NAME_None;
	const FString PreviousField = SelectedField;
	Model = MoveTemp(InModel); LocalLabel = NewLabel; DepotLabel = OldLabel;
	Columns.Reset(); AllJumps.Reset();
	TMap<FString, FString> Labels;
	for (const auto& Row : Model.Rows)
	{
		for (const auto& Field : Row->Fields)
		{
			Columns.AddUnique(Field->Path);
			Labels.Add(Field->Path, Field->DisplayPath.IsEmpty() ? Field->Path : Field->DisplayPath);
		}
		if (Row->Kind == ETableDiffKind::Unchanged) { continue; }
		auto Root = MakeShared<FTableDiffJump>(); Root->Row = Row; Root->Kind = Row->Kind; Root->Label = Row->Name.ToString(); AllJumps.Add(Root);
		TFunction<void(const TArray<TSharedPtr<FTableFieldDiff>>&, TArray<TSharedPtr<FTableDiffJump>>&)> AddChanges;
		AddChanges = [&](const auto& Fields, auto& Children)
		{
			for (const auto& Field : Fields)
			{
				if (Field->Kind == ETableDiffKind::Unchanged) { continue; }
				auto Jump = MakeShared<FTableDiffJump>(); Jump->Row = Row; Jump->FieldPath = Field->Path;
				Jump->Label = Field->Label; Jump->RestoreTarget = Field->RestoreTarget; Jump->Kind = Field->Kind;
				Children.Add(Jump); AllJumps.Add(Jump);
				// An added/removed element is one atomic change; its preview still exposes every field.
				if (Field->Kind == ETableDiffKind::Modified) { AddChanges(Field->Children, Jump->Children); }
			}
		};
		if (Row->Kind == ETableDiffKind::Modified) { AddChanges(Row->Details, Root->Children); }
	}
	for (const auto& Header : {LocalHeader, DepotHeader})
	{
		Header->ClearColumns();
		Header->AddColumn(SHeaderRow::Column(TEXT("RowName")).DefaultLabel(LOCTEXT("RowName", "Row Name"))
			.ManualWidth_Lambda([this]() { return ColumnWidth(RowWidthKey); })
			.OnWidthChanged(this, &SDataTableDiffView::ResizeColumn, RowWidthKey));
		for (int32 Index = 0; Index < Columns.Num(); ++Index)
		{
			const FString Path = Columns[Index];
			Header->AddColumn(SHeaderRow::Column(FName(*FString::Printf(TEXT("F%d"), Index))).DefaultLabel(FText::FromString(Labels[Path]))
				.ManualWidth_Lambda([this, Path]() { return ColumnWidth(Path); })
					.OnWidthChanged(this, &SDataTableDiffView::ResizeColumn, Path));
		}
	}
	LocalTable->RebuildList(); DepotTable->RebuildList();
	RefreshJumps();
	const auto* PreviousRow = Model.Rows.FindByPredicate([PreviousName](const auto& Row) { return Row->Name == PreviousName; });
	if (PreviousRow) { FocusRow(*PreviousRow, PreviousField); }
	else if (!AllJumps.IsEmpty()) { JumpTo(AllJumps[0], ESelectInfo::Direct); }
	else { FocusRow(Model.Rows.IsEmpty() ? nullptr : Model.Rows[0], FString()); }
}

void SDataTableDiffView::RefreshJumps()
{
	VisibleJumps.Reset();
	TFunction<bool(const TSharedPtr<FTableDiffJump>&, bool)> Filter;
	Filter = [&](const auto& Jump, bool bParentMatch)
	{
		const bool bMatch = bParentMatch || Search.IsEmpty() || Jump->Row->Name.ToString().Contains(Search)
			|| Jump->FieldPath.Contains(Search) || Jump->Label.Contains(Search);
		Jump->FilteredChildren.Reset();
		for (const auto& Child : Jump->Children) { if (Filter(Child, bMatch)) { Jump->FilteredChildren.Add(Child); } }
		return bMatch || !Jump->FilteredChildren.IsEmpty();
	};
	for (const auto& Jump : AllJumps)
	{
		if (Jump->FieldPath.IsEmpty() && Filter(Jump, false)) { VisibleJumps.Add(Jump); }
	}
	JumpList->RequestListRefresh();
	for (const auto& Jump : AllJumps) { if (!Jump->FilteredChildren.IsEmpty()) { JumpList->SetItemExpansion(Jump, true); } }
}

void SDataTableDiffView::JumpTo(TSharedPtr<FTableDiffJump> Jump, ESelectInfo::Type SelectInfo)
{
	if (!bSyncing && Jump) { FocusRow(Jump->Row, Jump->FieldPath); }
}

void SDataTableDiffView::SelectTableRow(TSharedPtr<FTableRowDiff> Row, ESelectInfo::Type SelectInfo)
{
	if (bSyncing || !Row) { return; }
	FocusRow(Row, FString());
	TGuardValue<bool> Guard(bSyncing, true);
	const auto* Jump = VisibleJumps.FindByPredicate([&Row](const auto& Item) { return Item->Row == Row; });
	JumpList->ClearSelection();
	if (Jump)
	{
		JumpList->SetSelection(*Jump, ESelectInfo::Direct);
		JumpList->RequestScrollIntoView(*Jump);
	}
}

void SDataTableDiffView::FocusRow(TSharedPtr<FTableRowDiff> Row, const FString& FieldPath)
{
	TGuardValue<bool> Guard(bSyncing, true);
	const bool bRowChanged = SelectedRow != Row;
	SelectedRow = Row; SelectedField = FieldPath;
	if (bRowChanged)
	{
		DetailHeights.Reset();
		SelectedFields = Row ? (Row->Details.IsEmpty() ? Row->Fields : Row->Details) : TArray<TSharedPtr<FTableFieldDiff>>();
		LocalDetails->RequestListRefresh(); DepotDetails->RequestListRefresh();
	}
	TSharedPtr<FTableFieldDiff> Target;
	TFunction<void(const TSharedPtr<FTableFieldDiff>&)> ExpandChanged;
	ExpandChanged = [&](const TSharedPtr<FTableFieldDiff>& Field)
	{
		if (Field->Children.IsEmpty()) { return; }
		ExpandedPaths.Add(Field->Path);
		for (const auto& Child : Field->Children)
		{
			if (Child->Kind != ETableDiffKind::Unchanged) { ExpandChanged(Child); }
		}
	};
	TFunction<bool(const TArray<TSharedPtr<FTableFieldDiff>>&)> RestoreExpansion;
	RestoreExpansion = [&](const TArray<TSharedPtr<FTableFieldDiff>>& Fields)
	{
		bool bContainsTarget = false;
		for (const auto& Field : Fields)
		{
			if (!FieldPath.IsEmpty() && Field->Path == FieldPath) { ExpandChanged(Field); }
			const bool bChildTarget = RestoreExpansion(Field->Children);
			if (bChildTarget) { ExpandedPaths.Add(Field->Path); }
			for (const auto& Details : {LocalDetails, DepotDetails}) { Details->SetItemExpansion(Field, ExpandedPaths.Contains(Field->Path)); }
			if (Field->Path == FieldPath) { Target = Field; bContainsTarget = true; }
			bContainsTarget |= bChildTarget;
		}
		return bContainsTarget;
	};
	RestoreExpansion(SelectedFields);
	for (const auto& Table : {LocalTable, DepotTable})
	{
		if (bRowChanged) { Table->ClearSelection(); }
		if (Row) { Table->SetSelection(Row); Table->RequestScrollIntoView(Row); }
	}
	if (!FieldPath.IsEmpty())
	{
		const int32 Column = Columns.IndexOfByPredicate([&](const FString& Path)
		{
			return Path == FieldPath || FieldPath.StartsWith(Path + TEXT("[")) || FieldPath.StartsWith(Path + TEXT("."))
				|| Path.StartsWith(FieldPath + TEXT("."));
		});
		if (Column != INDEX_NONE)
		{
			float Offset = ColumnWidth(RowWidthKey);
			for (int32 Index = 0; Index < Column; ++Index) { Offset += ColumnWidth(Columns[Index]); }
			LocalHorizontal->SetScrollOffset(Offset); DepotHorizontal->SetScrollOffset(Offset);
		}
		if (Target)
		{
			LocalDetails->RequestScrollIntoView(Target); DepotDetails->RequestScrollIntoView(Target);
		}
	}
}

void SDataTableDiffView::SyncScroll(double Offset, bool bLocal)
{
	if (bSyncing) { return; }
	TGuardValue<bool> Guard(bSyncing, true);
	(bLocal ? DepotTable : LocalTable)->SetScrollOffset(static_cast<float>(Offset));
}

void SDataTableDiffView::SyncDetailScroll(double Offset, bool bLocal)
{
	if (bSyncing) { return; }
	TGuardValue<bool> Guard(bSyncing, true);
	(bLocal ? DepotDetails : LocalDetails)->SetScrollOffset(static_cast<float>(Offset));
}

void SDataTableDiffView::SyncExpansion(TSharedPtr<FTableFieldDiff> Field, bool bExpanded, bool bLocal)
{
	if (bSyncing || !Field) { return; }
	TGuardValue<bool> Guard(bSyncing, true);
	if (bExpanded) { ExpandedPaths.Add(Field->Path); } else { ExpandedPaths.Remove(Field->Path); }
	(bLocal ? DepotDetails : LocalDetails)->SetItemExpansion(Field, bExpanded);
}

TSharedRef<ITableRow> SDataTableDiffView::GenerateTableRow(TSharedPtr<FTableRowDiff> Row, const TSharedRef<STableViewBase>& Owner, bool bLocal)
{
	return SNew(STableDiffGridRow, Owner).Row(Row).Columns(&Columns).Local(bLocal)
		.IsFocused([this, Row](const FString& Path)
		{
			return SelectedRow == Row && !SelectedField.IsEmpty() && !Path.IsEmpty()
				&& (SelectedField == Path || SelectedField.StartsWith(Path + TEXT("[")) || SelectedField.StartsWith(Path + TEXT("."))
					|| Path.StartsWith(SelectedField + TEXT(".")));
		});
}

TSharedRef<ITableRow> SDataTableDiffView::GenerateDetailRow(TSharedPtr<FTableFieldDiff> Field, const TSharedRef<STableViewBase>& Owner, bool bLocal)
{
	return SNew(STableDiffFocusedDetailRow, Owner).Field(Field).Local(bLocal).AbsentRow(SelectedRow && RowAbsent(*SelectedRow, bLocal))
		.Focused_Lambda([this, Field]() { return !SelectedField.IsEmpty() && SelectedField == Field->Path; })
		.SharedHeight([this, Field, bLocal](float Height) { return ShareDetailHeight(Field->Path, bLocal, Height); });
}

TSharedRef<ITableRow> SDataTableDiffView::GenerateJump(TSharedPtr<FTableDiffJump> Jump, const TSharedRef<STableViewBase>& Owner)
{
	const FText Label = FText::FromString(Jump->Label);
	auto TreeRow = SNew(STableDiffJumpRow, Owner).Kind(Jump->Kind);
	TreeRow->SetContent(
		SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
			[SNew(STextBlock).ColorAndOpacity(DiffColor(Jump->Kind)).Text(Label).ToolTipText(Label).OverflowPolicy(ETextOverflowPolicy::Ellipsis)]
			+ SHorizontalBox::Slot().AutoWidth().Padding(6, 0, 0, 0)
			[SNew(SButton).Text(LOCTEXT("RevertInline", "Revert")).ContentPadding(FMargin(6, 2))
				.Visibility_Lambda([this]() { return RevertChange.IsBound() ? EVisibility::Visible : EVisibility::Collapsed; })
				.ToolTipText_Lambda([this, Jump]()
				{
					return CanRevertJump(Jump)
						? LOCTEXT("RevertInlineTip", "Restore this change from the compared revision. Supports Undo; save the DataTable to persist the edit.")
						: LOCTEXT("RevertLocalOnlyTip", "Revert is only supported when the right version is Local.");
				})
				.IsEnabled_Lambda([this, Jump]() { return CanRevertJump(Jump); })
				.OnClicked_Lambda([this, Jump]() { return RevertJump(Jump); })]);
	return TreeRow;
}

float SDataTableDiffView::ColumnWidth(const FString& Path) const
{
	const float* Width = ColumnWidths.Find(Path);
	return Width ? *Width : Path == RowWidthKey ? NameWidth : FieldWidth;
}

float SDataTableDiffView::TableWidth() const
{
	float Width = ColumnWidth(RowWidthKey);
	for (const FString& Path : Columns) { Width += ColumnWidth(Path); }
	return Width;
}

void SDataTableDiffView::ResizeColumn(float Width, FString Path)
{
	ColumnWidths.Add(Path, FMath::Max(60.0f, Width));
	LocalHeader->Invalidate(EInvalidateWidgetReason::Layout); DepotHeader->Invalidate(EInvalidateWidgetReason::Layout);
	Invalidate(EInvalidateWidgetReason::Layout);
}

float SDataTableDiffView::ShareDetailHeight(const FString& Path, bool bLocal, float Height)
{
	FVector2D& Heights = DetailHeights.FindOrAdd(Path, FVector2D::ZeroVector);
	double& Side = bLocal ? Heights.X : Heights.Y;
	if (!FMath::IsNearlyEqual(Side, static_cast<double>(Height)))
	{
		Side = Height;
		(bLocal ? DepotDetails : LocalDetails)->Invalidate(EInvalidateWidgetReason::Layout);
	}
	return static_cast<float>(FMath::Max(Heights.X, Heights.Y));
}

void SDataTableDiffView::ResizePane(float Fraction)
{
	TableFraction = FMath::Clamp(Fraction, 0.1f, 0.9f);
	LocalSplitter->Invalidate(EInvalidateWidgetReason::Layout); DepotSplitter->Invalidate(EInvalidateWidgetReason::Layout);
}

#undef LOCTEXT_NAMESPACE
