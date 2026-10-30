// Copyright Meowoods 2026. All Rights Reserved.

#include "DataTableRevisionHistory.h"
#include "ISourceControlModule.h"
#include "SourceControlOperations.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	class FNumberedTableRevision : public ISourceControlRevision
	{
	public:
		FNumberedTableRevision(const FString& InFilename, int32 InNumber)
			: Filename(InFilename), Number(InNumber), Revision(LexToString(InNumber)) {}
		virtual bool Get(FString& OutFilename, EConcurrency::Type Concurrency) const override
		{
			ISourceControlProvider& Provider = ISourceControlModule::Get().GetProvider();
			if (Provider.GetName() != TEXT("Perforce")) { return false; }
			const FString Spec = Filename + TEXT("#") + Revision;
			auto Operation = ISourceControlOperation::Create<FDownloadFile>();
			if (Provider.Execute(Operation, TArray<FString>{Spec}, EConcurrency::Synchronous) != ECommandResult::Succeeded) { return false; }
			const FSharedBuffer Data = Operation->GetFileData(Spec);
			if (!Data || Data.GetSize() > MAX_int32) { return false; }
			const FString Directory = FPaths::ProjectSavedDir() / TEXT("Temp/DataTableDiffTool");
			IFileManager::Get().MakeDirectory(*Directory, true);
			OutFilename = FPaths::CreateTempFilename(*Directory, TEXT("Revision"), TEXT(".uasset"));
			return FFileHelper::SaveArrayToFile(MakeArrayView(static_cast<const uint8*>(Data.GetData()), static_cast<int32>(Data.GetSize())), *OutFilename);
		}
		virtual bool GetAnnotated(TArray<FAnnotationLine>& Lines) const override { return false; }
		virtual bool GetAnnotated(FString& OutFilename) const override { return false; }
		virtual const FString& GetFilename() const override { return Filename; }
		virtual int32 GetRevisionNumber() const override { return Number; }
		virtual const FString& GetRevision() const override { return Revision; }
		virtual const FString& GetDescription() const override { return Description; }
		virtual const FString& GetUserName() const override { return Empty; }
		virtual const FString& GetClientSpec() const override { return Empty; }
		virtual const FString& GetAction() const override { return Empty; }
		virtual TSharedPtr<ISourceControlRevision, ESPMode::ThreadSafe> GetBranchSource() const override { return nullptr; }
		virtual const FDateTime& GetDate() const override { return Date; }
		virtual int32 GetCheckInIdentifier() const override { return -1; }
		virtual int32 GetFileSize() const override { return 0; }
	private:
		FString Filename;
		int32 Number;
		FString Revision;
		FString Empty;
		FString Description = TEXT("Older revision outside the source-control metadata cache. Contents are fetched when selected.");
		FDateTime Date;
	};
}

TSharedPtr<ISourceControlRevision, ESPMode::ThreadSafe> MakeNumberedTableRevision(const FString& Filename, int32 Number)
{
	return MakeShared<FNumberedTableRevision, ESPMode::ThreadSafe>(Filename, Number);
}

void CompletePerforceTableHistory(TArray<TSharedPtr<ISourceControlRevision, ESPMode::ThreadSafe>>& Revisions, const FString& Filename)
{
	if (Revisions.IsEmpty()) { return; }
	const int32 Head = Revisions[0]->GetRevisionNumber();
	const FString DepotFilename = Revisions[0]->GetFilename();
	TMap<int32, TSharedPtr<ISourceControlRevision, ESPMode::ThreadSafe>> Cached;
	for (const auto& Revision : Revisions)
	{
		if (Revision->GetFilename() == DepotFilename) { Cached.Add(Revision->GetRevisionNumber(), Revision); }
	}
	Revisions.Reset();
	for (int32 Number = Head; Number > 0; --Number)
	{
		auto Revision = Cached.FindRef(Number);
		Revisions.Add(Revision ? Revision : MakeNumberedTableRevision(Filename, Number));
	}
}
