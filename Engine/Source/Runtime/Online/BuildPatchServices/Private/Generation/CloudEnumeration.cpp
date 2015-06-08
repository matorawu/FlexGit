// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#if WITH_BUILDPATCHGENERATION

#include "BuildPatchServicesPrivatePCH.h"

#include "CloudEnumeration.h"

namespace FBuildPatchServices
{
	class FCloudEnumerationImpl
		: public FCloudEnumeration
	{
	public:
		FCloudEnumerationImpl(const FString& CloudDirectory);
		virtual ~FCloudEnumerationImpl();

		virtual TSet<FGuid> GetChunkSet(uint64 ChunkHash) const override;
		virtual TMap<uint64, TSet<FGuid>> GetChunkInventory() const override;
		virtual TMap<FGuid, int64> GetChunkFileSizes() const override;
	private:
		void EnumerateCloud();
		void EnumerateManifestData(const FBuildPatchAppManifestRef& Manifest);

	private:
		const FString CloudDirectory;
		mutable FCriticalSection InventoryCS;
		TMap<uint64, TSet<FGuid>> ChunkInventory;
		TMap<FGuid, int64> ChunkFileSizes;
		TMap<FSHAHash, TSet<FGuid>> FileInventory;
		uint64 NumChunksFound;
		uint64 NumFilesFound;
		TFuture<void> Future;
	};

	FCloudEnumerationImpl::FCloudEnumerationImpl(const FString& InCloudDirectory)
		: CloudDirectory(InCloudDirectory)
		, NumChunksFound(0)
		, NumFilesFound(0)
	{
		TFunction<void()> Task = [this]() { EnumerateCloud(); };
		Future = Async(EAsyncExecution::Thread, Task);
	}

	FCloudEnumerationImpl::~FCloudEnumerationImpl()
	{
	}

	TSet<FGuid> FCloudEnumerationImpl::GetChunkSet(uint64 ChunkHash) const
	{
		FScopeLock ScopeLock(&InventoryCS);
		Future.Wait();
		return ChunkInventory.FindRef(ChunkHash);
	}

	TMap<uint64, TSet<FGuid>> FCloudEnumerationImpl::GetChunkInventory() const
	{
		FScopeLock ScopeLock(&InventoryCS);
		Future.Wait();
		return ChunkInventory;
	}

	TMap<FGuid, int64> FCloudEnumerationImpl::GetChunkFileSizes() const
	{
		FScopeLock ScopeLock(&InventoryCS);
		Future.Wait();
		return ChunkFileSizes;
	}

	void FCloudEnumerationImpl::EnumerateCloud()
	{
		FScopeLock ScopeLock(&InventoryCS);

		IFileManager& FileManager = IFileManager::Get();
		FString JSONOutput;
		TSharedRef< TJsonWriter< TCHAR, TPrettyJsonPrintPolicy< TCHAR > > > DebugWriter = TJsonWriterFactory< TCHAR, TPrettyJsonPrintPolicy< TCHAR > >::Create(&JSONOutput);
		DebugWriter->WriteObjectStart();

		// Find all manifest files
		if (FileManager.DirectoryExists(*CloudDirectory))
		{
			const double StartEnumerate = FPlatformTime::Seconds();
			TArray<FString> AllManifests;
			GLog->Logf(TEXT("BuildDataGenerator: Enumerating Manifests from %s"), *CloudDirectory);
			FileManager.FindFiles(AllManifests, *(CloudDirectory / TEXT("*.manifest")), true, false);
			const double EnumerateTime = FPlatformTime::Seconds() - StartEnumerate;
			GLog->Logf(TEXT("BuildDataGenerator: Found %d manifests in %.1f seconds"), AllManifests.Num(), EnumerateTime);

			// Load all manifest files
			const double StartLoadAllManifest = FPlatformTime::Seconds();
			for (const auto& ManifestFile : AllManifests)
			{
				// Determine chunks from manifest file
				const FString ManifestFilename = CloudDirectory / ManifestFile;
				FBuildPatchAppManifestRef BuildManifest = MakeShareable(new FBuildPatchAppManifest());
				const double StartLoadManifest = FPlatformTime::Seconds();
				if (BuildManifest->LoadFromFile(ManifestFilename))
				{
					const double LoadManifestTime = FPlatformTime::Seconds() - StartLoadManifest;
					GLog->Logf(TEXT("BuildDataGenerator: Loaded %s in %.1f seconds"), *ManifestFile, LoadManifestTime);
					EnumerateManifestData(BuildManifest);
				}
				else
				{
					GLog->Logf(TEXT("BuildDataGenerator: WARNING: Could not read Manifest file. Data recognition will suffer (%s)"), *ManifestFilename);
				}
			}
			const double LoadAllManifestTime = FPlatformTime::Seconds() - StartLoadAllManifest;
			GLog->Logf(TEXT("BuildDataGenerator: Used %d manifests to enumerate %llu chunks in %.1f seconds"), AllManifests.Num(), NumChunksFound, LoadAllManifestTime);
		}
		else
		{
			GLog->Logf(TEXT("BuildDataGenerator: Cloud directory does not exist: %s"), *CloudDirectory);
		}
	}

	void FCloudEnumerationImpl::EnumerateManifestData(const FBuildPatchAppManifestRef& Manifest)
	{
		TArray<FGuid> DataList;
		Manifest->GetDataList(DataList);
		if (!Manifest->IsFileDataManifest())
		{
			for (const auto& DataGuid : DataList)
			{
				uint64 DataChunkHash;
				if (Manifest->GetChunkHash(DataGuid, DataChunkHash))
				{
					if (DataChunkHash != 0)
					{
						TSet<FGuid>& HashChunkList = ChunkInventory.FindOrAdd(DataChunkHash);
						if (!HashChunkList.Contains(DataGuid))
						{
							++NumChunksFound;
							HashChunkList.Add(DataGuid);
							ChunkFileSizes.Add(DataGuid, Manifest->GetDataSize(DataGuid));
						}
					}
					else
					{
						GLog->Logf(TEXT("BuildDataGenerator: INFO: Ignored an existing chunk %s with a failed hash value of zero to avoid performance problems while chunking"), *DataGuid.ToString());
					}
				}
				else
				{
					GLog->Logf(TEXT("BuildDataGenerator: WARNING: Missing chunk hash for %s in manifest %s %s"), *DataGuid.ToString(), *Manifest->GetAppName(), *Manifest->GetVersionString());
				}
			}
		}
		else
		{
			GLog->Logf(TEXT("BuildDataGenerator: INFO: Ignoring non-chunked manifest %s %s"), *Manifest->GetAppName(), *Manifest->GetVersionString());
		}
	}

	FCloudEnumerationRef FCloudEnumerationFactory::Create(const FString& CloudDirectory)
	{
		return MakeShareable(new FCloudEnumerationImpl(CloudDirectory));
	}
}

#endif