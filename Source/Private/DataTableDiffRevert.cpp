#include "DataTableDiffRevert.h"
#include "DataTableDiffModel.h"
#include "Engine/DataTable.h"
#include "DataTableEditorUtils.h"
#include "ScopedTransaction.h"
#include "UObject/UnrealType.h"
#include "Serialization/ArchiveReplaceObjectRef.h"

#define LOCTEXT_NAMESPACE "DataTableDiffTool"

namespace
{
	struct FResolvedTableField
	{
		const FProperty* Property = nullptr;
		void* Value = nullptr;
	};

	FResolvedTableField ResolveField(const UStruct* Struct, void* Data, const FString& Path)
	{
		TArray<FString> Parts;
		Path.ParseIntoArray(Parts, TEXT("."));
		FResolvedTableField Result;
		for (int32 PartIndex = 0; PartIndex < Parts.Num(); ++PartIndex)
		{
			FString Name = Parts[PartIndex];
			int32 ArrayIndex = 0;
			int32 Bracket = INDEX_NONE;
			const bool bIndexed = Name.FindChar(TEXT('['), Bracket);
			if (bIndexed)
			{
				if (!Name.EndsWith(TEXT("]"))) { return {}; }
				const FString IndexText = Name.Mid(Bracket + 1, Name.Len() - Bracket - 2);
				if (!IndexText.IsNumeric()) { return {}; }
				ArrayIndex = FCString::Atoi(*IndexText); Name.LeftInline(Bracket);
			}
			const FProperty* Property = FindFProperty<FProperty>(Struct, *Name);
			if (!Property || !Data) { return {}; }
			if (const auto* Array = CastField<FArrayProperty>(Property); Array && bIndexed)
			{
				FScriptArrayHelper Helper(Array, Array->ContainerPtrToValuePtr<void>(Data));
				if (ArrayIndex < 0 || ArrayIndex >= Helper.Num()) { return {}; }
				Result = {Array->Inner, Helper.GetRawPtr(ArrayIndex)};
			}
			else
			{
				if (ArrayIndex < 0 || ArrayIndex >= Property->ArrayDim) { return {}; }
				Result = {Property, Property->ContainerPtrToValuePtr<void>(Data, ArrayIndex)};
			}
			if (PartIndex + 1 < Parts.Num())
			{
				const auto* Nested = CastField<FStructProperty>(Result.Property);
				if (!Nested) { return {}; }
				Struct = Nested->Struct; Data = Result.Value;
			}
		}
		return Result;
	}

	struct FRestoreTarget
	{
		FResolvedTableField Old, New, Container;
		int32 Index = INDEX_NONE;
		bool bInsert = false;
		bool bDelete = false;
	};

	FRestoreTarget ResolveTarget(const UDataTable* Historical, const UDataTable* Workspace, uint8* OldRow, uint8* NewRow, const FTableRestoreTarget& Restore)
	{
		FRestoreTarget Target;
		Target.Old = ResolveField(Historical->GetRowStruct(), OldRow, Restore.OldPath);
		Target.New = ResolveField(Workspace->GetRowStruct(), NewRow, Restore.NewPath);
		if (Restore.Operation != ETableRestoreOperation::Replace)
		{
			Target.Container = ResolveField(Workspace->GetRowStruct(), NewRow, Restore.ContainerPath);
			Target.Index = Restore.Index;
			Target.bInsert = Restore.Operation == ETableRestoreOperation::InsertElement && Target.Old.Property;
			Target.bDelete = Restore.Operation == ETableRestoreOperation::DeleteElement && Target.New.Property;
		}
		return Target;
	}
}

bool FDataTableDiffRevert::CanApply(const UDataTable* Historical, const UDataTable* Workspace,
	FName RowName, const FTableRestoreTarget& Restore, FText& Error)
{
	if (!Historical || !Workspace || RowName.IsNone() || Historical == Workspace)
	{
		Error = LOCTEXT("NoComparison", "Select a change after loading a comparison revision."); return false;
	}
	uint8* OldRow = Historical->GetRowMap().FindRef(RowName);
	uint8* NewRow = Workspace->GetRowMap().FindRef(RowName);
	if (Restore.IsRow() && !OldRow && NewRow) { return true; }
	if (Restore.IsRow() && OldRow && !NewRow && Historical->GetRowStruct() == Workspace->GetRowStruct()) { return true; }
	if (Restore.IsRow() && OldRow && NewRow && Historical->GetRowStruct() == Workspace->GetRowStruct())
	{
		return !Historical->GetRowStruct()->CompareScriptStruct(OldRow, NewRow, PPF_None);
	}
	if (OldRow && NewRow && !Restore.IsRow())
	{
		const auto Target = ResolveTarget(Historical, Workspace, OldRow, NewRow, Restore);
		const auto Old = Target.Old; const auto New = Target.New;
		if (const auto* Array = CastField<FArrayProperty>(Target.Container.Property))
		{
			const int32 Count = FScriptArrayHelper(Array, Target.Container.Value).Num();
			if (Target.Index >= 0 && ((Target.bInsert && Target.Index <= Count && Array->Inner->SameType(Old.Property))
				|| (Target.bDelete && Target.Index < Count && Array->Inner->SameType(New.Property)))) { return true; }
		}
		if (Old.Property && New.Property && Old.Property->SameType(New.Property))
		{
			if (!Old.Property->Identical(Old.Value, New.Value, PPF_None)) { return true; }
			Error = LOCTEXT("AlreadyRestored", "This property already matches the compared revision."); return false;
		}
	}
	Error = LOCTEXT("CannotRestoreSchema", "Select a changed property or an added/deleted row. Incompatible or missing fields cannot be restored.");
	return false;
}

bool FDataTableDiffRevert::Apply(const UDataTable* Historical, UDataTable* Workspace,
	FName RowName, const FTableRestoreTarget& Restore, FText& Error)
{
	if (!CanApply(Historical, Workspace, RowName, Restore, Error)) { return false; }
	const FScopedTransaction Transaction(LOCTEXT("RestoreTableChange", " Revert DataTable Change"));
	uint8* OldRow = Historical->GetRowMap().FindRef(RowName);
	if (Restore.IsRow() && !OldRow)
	{
		// Use the editor's deletion workflow; the runtime RemoveRow callback may look up a row that was just deleted.
		const bool bRemoved = FDataTableEditorUtils::RemoveRow(Workspace, RowName);
		if (bRemoved) { Workspace->MarkPackageDirty(); }
		return bRemoved;
	}
	const auto Info = Restore.IsRow() ? FDataTableEditorUtils::EDataTableChangeInfo::RowList : FDataTableEditorUtils::EDataTableChangeInfo::RowData;
	FDataTableEditorUtils::BroadcastPreChange(Workspace, Info);
	Workspace->Modify();
	if (Restore.IsRow())
	{
		Workspace->AddRow(RowName, OldRow, Historical->GetRowStruct());
	}
	else
	{
		const auto Target = ResolveTarget(Historical, Workspace, OldRow, Workspace->GetRowMap().FindRef(RowName), Restore);
		if (Target.bInsert || Target.bDelete)
		{
			const auto* Array = CastFieldChecked<FArrayProperty>(Target.Container.Property);
			FScriptArrayHelper Helper(Array, Target.Container.Value);
			if (Target.bDelete) { Helper.RemoveValues(Target.Index, 1); }
			else
			{
				Helper.InsertValues(Target.Index, 1);
				Array->Inner->CopySingleValue(Helper.GetRawPtr(Target.Index), Target.Old.Value);
			}
		}
		else { Target.New.Property->CopySingleValue(Target.New.Value, Target.Old.Value); }
	}
	// A historical self-reference must point back to the editable asset, not to the diff package.
	TMap<UObject*, UObject*> Replacement;
	Replacement.Add(const_cast<UDataTable*>(Historical), Workspace);
	FArchiveReplaceObjectRef<UObject> Replace(Workspace, Replacement);
	Workspace->MarkPackageDirty();
	FDataTableEditorUtils::BroadcastPostChange(Workspace, Info);
	return true;
}

#undef LOCTEXT_NAMESPACE
