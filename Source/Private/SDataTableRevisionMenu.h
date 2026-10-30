// Copyright Meowoods 2026. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"
#include "ISourceControlProvider.h"
#include "UObject/WeakObjectPtr.h"

class UDataTable;
class SVerticalBox;
class FUpdateStatus;
class ISourceControlRevision;

class SDataTableRevisionMenu : public SCompoundWidget
{
public:
	using FRevisionPtr = TSharedPtr<ISourceControlRevision, ESPMode::ThreadSafe>;
	DECLARE_DELEGATE_OneParam(FOnRevisionSelected, FRevisionPtr);
	SLATE_BEGIN_ARGS(SDataTableRevisionMenu) {} SLATE_ARGUMENT(UDataTable*, Table) SLATE_EVENT(FOnRevisionSelected, OnRevisionSelected) SLATE_END_ARGS()
	void Construct(const FArguments& Args);
	virtual ~SDataTableRevisionMenu();
private:
	void HistoryReady(const FSourceControlOperationRef& Operation, ECommandResult::Type Result);
	FReply CancelQuery();
	void ShowMessage(const FText& Text);
	TSharedPtr<SVerticalBox> MenuBox;
	TSharedPtr<FUpdateStatus, ESPMode::ThreadSafe> Query;
	FOnRevisionSelected OnRevisionSelected;
	FString Filename;
};
