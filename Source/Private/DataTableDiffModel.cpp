#include "DataTableDiffModel.h"
#include "Engine/DataTable.h"
#include "UObject/TextProperty.h"
#include "UObject/UnrealType.h"

namespace
{
	struct FTablePropertyPair
	{
		FName Name;
		const FProperty* Old;
		const FProperty* New;
	};

	TArray<FTablePropertyPair> PairProperties(const UStruct* OldStruct, const UStruct* NewStruct)
	{
		TMap<FName, const FProperty*> OldProperties, NewProperties;
		TArray<FName> Names;
		auto Gather = [&Names](const UStruct* Struct, TMap<FName, const FProperty*>& Properties)
		{
			if (!Struct) { return; }
			for (TFieldIterator<FProperty> It(Struct); It; ++It)
			{
				Properties.Add(It->GetFName(), *It); Names.AddUnique(It->GetFName());
			}
		};
		Gather(NewStruct, NewProperties); Gather(OldStruct, OldProperties);
		TArray<FTablePropertyPair> Pairs;
		Pairs.Reserve(Names.Num());
		for (FName Name : Names) { Pairs.Add({Name, OldProperties.FindRef(Name), NewProperties.FindRef(Name)}); }
		return Pairs;
	}

	TSharedPtr<FTableFieldDiff> PreviewProperty(const FProperty* OldProperty, const void* OldValue,
		const FProperty* NewProperty, const void* NewValue, const FString& Path, const FString& Label,
		const FString& OldPath, const FString& NewPath);

	void PreviewStruct(const UStruct* OldStruct, const void* OldData, const UStruct* NewStruct,
		const void* NewData, const FString& Prefix, TArray<TSharedPtr<FTableFieldDiff>>& Children,
		const FString& OldPrefix, const FString& NewPrefix)
	{
		for (const auto& Pair : PairProperties(OldStruct, NewStruct))
		{
			const FName Name = Pair.Name;
			const FProperty* Old = Pair.Old;
			const FProperty* New = Pair.New;
			const int32 Count = FMath::Max(Old ? Old->ArrayDim : 0, New ? New->ArrayDim : 0);
			for (int32 Index = 0; Index < Count; ++Index)
			{
				const FString Suffix = Count > 1 ? FString::Printf(TEXT("[%d]"), Index) : FString();
				const void* OldValue = Old && OldData && Index < Old->ArrayDim ? Old->ContainerPtrToValuePtr<void>(OldData, Index) : nullptr;
				const void* NewValue = New && NewData && Index < New->ArrayDim ? New->ContainerPtrToValuePtr<void>(NewData, Index) : nullptr;
				if (OldValue || NewValue)
				{
					Children.Add(PreviewProperty(Old, OldValue, New, NewValue, Prefix + Name.ToString() + Suffix,
						(New ? New : Old)->GetDisplayNameText().ToString() + Suffix,
						OldPrefix + Name.ToString() + Suffix, NewPrefix + Name.ToString() + Suffix));
				}
			}
		}
	}

	FString DisplayValue(const FProperty* Property, const void* Value)
	{
		if (!Property || !Value) { return FString(); }
		if (const FTextProperty* Text = CastField<FTextProperty>(Property)) { return Text->GetPropertyValue(Value).ToString(); }
		FString Result;
		Property->ExportTextItem_Direct(Result, Value, nullptr, nullptr, PPF_None);
		return Result;
	}

	FString PreviewValue(const FProperty* Property, const void* Value)
	{
		if (!Property || !Value) { return FString(); }
		if (const FObjectPropertyBase* Object = CastField<FObjectPropertyBase>(Property))
		{
			return GetNameSafe(Object->GetObjectPropertyValue(Value));
		}
		if (const FStrProperty* String = CastField<FStrProperty>(Property)) { return String->GetPropertyValue(Value); }
		return DisplayValue(Property, Value);
	}

	TSharedPtr<FTableFieldDiff> PreviewProperty(const FProperty* OldProperty, const void* OldValue,
		const FProperty* NewProperty, const void* NewValue, const FString& Path, const FString& Label,
		const FString& OldPath, const FString& NewPath)
	{
		auto Field = MakeShared<FTableFieldDiff>();
		Field->Path = Path; Field->Label = Label;
		Field->RestoreTarget.OldPath = OldValue ? OldPath : FString();
		Field->RestoreTarget.NewPath = NewValue ? NewPath : FString();
		Field->OldType = OldProperty ? OldProperty->GetCPPType() : FString();
		Field->NewType = NewProperty ? NewProperty->GetCPPType() : FString();
		if (!OldValue) { Field->Kind = ETableDiffKind::Added; }
		else if (!NewValue) { Field->Kind = ETableDiffKind::Removed; }
		else if (!OldProperty->SameType(NewProperty) || !OldProperty->Identical(OldValue, NewValue, PPF_None)) { Field->Kind = ETableDiffKind::Modified; }
		const bool bCompatible = !OldProperty || !NewProperty || OldProperty->SameType(NewProperty);
		const FProperty* Property = NewProperty ? NewProperty : OldProperty;
		if (bCompatible && CastField<FStructProperty>(Property))
		{
			const auto* Old = CastField<FStructProperty>(OldProperty);
			const auto* New = CastField<FStructProperty>(NewProperty);
			PreviewStruct(Old ? Old->Struct : nullptr, OldValue, New ? New->Struct : nullptr, NewValue, Path + TEXT("."), Field->Children,
				OldPath + TEXT("."), NewPath + TEXT("."));
		}
		else if (bCompatible && CastField<FArrayProperty>(Property))
		{
			const auto* Old = CastField<FArrayProperty>(OldProperty);
			const auto* New = CastField<FArrayProperty>(NewProperty);
			const int32 OldCount = OldValue ? FScriptArrayHelper(Old, OldValue).Num() : 0;
			const int32 NewCount = NewValue ? FScriptArrayHelper(New, NewValue).Num() : 0;
			if (OldValue) { Field->OldValue = FString::Printf(TEXT("%d Array elements"), OldCount); }
			if (NewValue) { Field->NewValue = FString::Printf(TEXT("%d Array elements"), NewCount); }
			// Like native AsyncTreeDifferences, anchor equal values with an LCS and pair replacements in the gaps.
			TArray<TArray<int32>> LCS;
			LCS.SetNum(OldCount + 1);
			for (auto& Line : LCS) { Line.Init(0, NewCount + 1); }
			for (int32 A = OldCount - 1; A >= 0; --A)
			{
				for (int32 B = NewCount - 1; B >= 0; --B)
				{
					const bool bEqual = Old->Inner->Identical(FScriptArrayHelper(Old, OldValue).GetRawPtr(A), FScriptArrayHelper(New, NewValue).GetRawPtr(B), PPF_None);
					LCS[A][B] = bEqual ? 1 + LCS[A + 1][B + 1] : FMath::Max(LCS[A + 1][B], LCS[A][B + 1]);
				}
			}
			TArray<FIntPoint> Anchors;
			int32 A = 0, B = 0;
			while (A < OldCount && B < NewCount)
			{
				if (Old->Inner->Identical(FScriptArrayHelper(Old, OldValue).GetRawPtr(A), FScriptArrayHelper(New, NewValue).GetRawPtr(B), PPF_None))
				{
					Anchors.Add(FIntPoint(A++, B++));
				}
				else if (LCS[A + 1][B] > LCS[A][B + 1]) { ++A; }
				else { ++B; }
			}
			Anchors.Add(FIntPoint(OldCount, NewCount));
			auto AddElement = [&](int32 OldIndex, int32 NewIndex, int32 InsertIndex)
			{
				const FString Suffix = FString::Printf(TEXT("[%d]"), Field->Children.Num());
				const FString OldElement = OldPath + FString::Printf(TEXT("[%d]"), OldIndex);
				const FString NewElement = NewPath + FString::Printf(TEXT("[%d]"), NewIndex);
				const FString ElementLabel = OldIndex != INDEX_NONE && NewIndex != INDEX_NONE && OldIndex != NewIndex
					? FString::Printf(TEXT("[%d] -> [%d]"), OldIndex, NewIndex)
					: FString::Printf(TEXT("[%d]"), NewIndex != INDEX_NONE ? NewIndex : OldIndex);
				auto Child = PreviewProperty(Old ? Old->Inner : nullptr, OldIndex != INDEX_NONE ? FScriptArrayHelper(Old, OldValue).GetRawPtr(OldIndex) : nullptr,
					New ? New->Inner : nullptr, NewIndex != INDEX_NONE ? FScriptArrayHelper(New, NewValue).GetRawPtr(NewIndex) : nullptr,
					Path + Suffix, ElementLabel, OldElement, NewElement);
				if (OldIndex == INDEX_NONE || NewIndex == INDEX_NONE)
				{
					Child->RestoreTarget.ContainerPath = NewPath;
					Child->RestoreTarget.Index = InsertIndex;
					Child->RestoreTarget.Operation = OldIndex == INDEX_NONE ? ETableRestoreOperation::DeleteElement : ETableRestoreOperation::InsertElement;
				}
				Field->Children.Add(Child);
			};
			A = 0; B = 0;
			for (const FIntPoint& Anchor : Anchors)
			{
				while (A < Anchor.X && B < Anchor.Y) { AddElement(A, B, B); ++A; ++B; }
				while (A < Anchor.X) { AddElement(A++, INDEX_NONE, B); }
				while (B < Anchor.Y) { AddElement(INDEX_NONE, B, B); ++B; }
				if (Anchor.X < OldCount) { AddElement(A, B, B); ++A; ++B; }
			}
		}
		else
		{
			Field->OldValue = PreviewValue(OldProperty, OldValue); Field->NewValue = PreviewValue(NewProperty, NewValue);
		}
		return Field;
	}

	void CompareStruct(const UStruct* OldStruct, const void* OldData, const UStruct* NewStruct,
		const void* NewData, const FString& Prefix, const FString& DisplayPrefix, TArray<TSharedPtr<FTableFieldDiff>>& Fields)
	{
		for (const auto& Pair : PairProperties(OldStruct, NewStruct))
		{
			const FName Name = Pair.Name;
			const FProperty* OldProperty = Pair.Old;
			const FProperty* NewProperty = Pair.New;
			const int32 Count = FMath::Max(OldProperty ? OldProperty->ArrayDim : 0, NewProperty ? NewProperty->ArrayDim : 0);
			for (int32 Index = 0; Index < Count; ++Index)
			{
				const void* OldValue = OldProperty && OldData && Index < OldProperty->ArrayDim
					? OldProperty->ContainerPtrToValuePtr<void>(OldData, Index) : nullptr;
				const void* NewValue = NewProperty && NewData && Index < NewProperty->ArrayDim
					? NewProperty->ContainerPtrToValuePtr<void>(NewData, Index) : nullptr;
				if (!OldValue && !NewValue) { continue; }
				FString Path = Prefix + Name.ToString();
				FString DisplayPath = DisplayPrefix + (NewProperty ? NewProperty : OldProperty)->GetDisplayNameText().ToString();
				if (Count > 1)
				{
					const FString Suffix = FString::Printf(TEXT("[%d]"), Index);
					Path += Suffix; DisplayPath += Suffix;
				}
				const FStructProperty* OldNested = CastField<FStructProperty>(OldProperty);
				const FStructProperty* NewNested = CastField<FStructProperty>(NewProperty);
				// Only expand matching struct types; keep type replacements visible as one field.
				if (OldNested && NewNested && OldNested->SameType(NewNested))
				{
					CompareStruct(OldNested->Struct, OldValue, NewNested->Struct, NewValue, Path + TEXT("."), DisplayPath + TEXT("."), Fields);
					continue;
				}
				TSharedPtr<FTableFieldDiff> Field = MakeShared<FTableFieldDiff>();
				Field->Path = Path;
				Field->DisplayPath = DisplayPath;
				Field->OldType = OldProperty ? OldProperty->GetCPPType() : FString();
				Field->NewType = NewProperty ? NewProperty->GetCPPType() : FString();
				Field->OldValue = DisplayValue(OldProperty, OldValue);
				Field->NewValue = DisplayValue(NewProperty, NewValue);
				if (!OldValue) { Field->Kind = ETableDiffKind::Added; }
				else if (!NewValue) { Field->Kind = ETableDiffKind::Removed; }
				else if (!OldProperty->SameType(NewProperty) || !OldProperty->Identical(OldValue, NewValue, PPF_None))
				{
					Field->Kind = ETableDiffKind::Modified;
				}
				Fields.Add(Field);
			}
		}
	}
}

FTableDiffModel FTableDiffModel::Compare(const UDataTable* OldTable, const UDataTable* NewTable)
{
	FTableDiffModel Model;
	const UScriptStruct* OldStruct = OldTable ? OldTable->GetRowStruct() : nullptr;
	const UScriptStruct* NewStruct = NewTable ? NewTable->GetRowStruct() : nullptr;
	Model.OldSchema = GetPathNameSafe(OldStruct);
	Model.NewSchema = GetPathNameSafe(NewStruct);
	TArray<FName> Names;
	if (NewTable) { for (const auto& Pair : NewTable->GetRowMap()) { Names.Add(Pair.Key); } }
	if (OldTable) { for (const auto& Pair : OldTable->GetRowMap()) { Names.AddUnique(Pair.Key); } }
	for (FName Name : Names)
	{
		const uint8* OldRow = OldTable ? OldTable->GetRowMap().FindRef(Name) : nullptr;
		const uint8* NewRow = NewTable ? NewTable->GetRowMap().FindRef(Name) : nullptr;
		TSharedPtr<FTableRowDiff> Row = MakeShared<FTableRowDiff>();
		Row->Name = Name;
		CompareStruct(OldStruct, OldRow, NewStruct, NewRow, FString(), FString(), Row->Fields);
		PreviewStruct(OldStruct, OldRow, NewStruct, NewRow, FString(), Row->Details, FString(), FString());
		if (!OldRow) { Row->Kind = ETableDiffKind::Added; ++Model.Added; }
		else if (!NewRow) { Row->Kind = ETableDiffKind::Removed; ++Model.Removed; }
		else if (Model.OldSchema != Model.NewSchema || Row->Fields.ContainsByPredicate(
			[](const TSharedPtr<FTableFieldDiff>& Field) { return Field->Kind != ETableDiffKind::Unchanged; }))
		{
			Row->Kind = ETableDiffKind::Modified;
			++Model.Modified;
		}
		Model.Rows.Add(Row);
	}
	return Model;
}
