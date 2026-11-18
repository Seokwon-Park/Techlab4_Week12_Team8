#include "EnginePCH.h"
#include "Particles/ParticleSystemFile.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <limits>

#include "Particles/ParticleSystem.h"
#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModuleRequired.h"
#include "Asset/AssetManager.h"
#include "Asset/AssetUtils.h"
#include "UObject/UObjectHash.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/StructuredArchive.h"
#include "Serialization/StructuredArchiveSlots.h"
#include "Serialization/Formatter/JsonArchiveInputFormatter.h"
#include "Serialization/Formatter/JsonArchiveOutputFormatter.h"

bool ParticleSystemFile::Read(const std::filesystem::path& Path, UParticleSystem& System, FString& OutError)
{
	OutError.clear();
	std::ifstream File(Path, std::ios::binary | std::ios::ate);
	const auto Size = static_cast<std::streamoff>(File.tellg());
	if (!File || Size <= 0 || Size > (std::numeric_limits<int32>::max)())
	{
		OutError = "ParticleSystem: cannot open file or invalid size";
		return false;
	}

	TArray<uint8> Bytes;
	Bytes.SetNum(static_cast<int32>(Size));
	File.seekg(0);
	if (!File.read(reinterpret_cast<char*>(Bytes.GetData()), Size))
	{
		OutError = "ParticleSystem: cannot read file";
		return false;
	}

	FMemoryReader Reader(Bytes, true);
	FJsonArchiveInputFormatter Formatter(Reader);
	if (Reader.IsError())
	{
		OutError = "ParticleSystem: invalid JSON";
		return false;
	}
	FStructuredArchive Archive(Formatter);
	System.Serialize(Archive.Open().EnterRecord());
	if (Reader.IsError())
	{
		OutError = "ParticleSystem: unsupported version or broken data";
		return false;
	}

	System.SetAssetPath(FAssetUtils::GetAssetKey(Path.generic_string()));
	return true;
}

bool ParticleSystemFile::Write(const std::filesystem::path& Path, UParticleSystem& System, FString& OutError)
{
	OutError.clear();
	TArray<uint8> Bytes;
	FMemoryWriter Writer(Bytes);
	{
		FJsonArchiveOutputFormatter Formatter(Writer);
		FStructuredArchive Archive(Formatter);
		System.Serialize(Archive.Open().EnterRecord());
	}
	if (Writer.IsError())
	{
		OutError = "ParticleSystem: cannot serialize data";
		return false;
	}

	std::ofstream File(Path, std::ios::binary | std::ios::trunc);
	if (!File)
	{
		OutError = "ParticleSystem: cannot open output file";
		return false;
	}
	File.write(reinterpret_cast<const char*>(Bytes.GetData()), Bytes.Num());
	File.close();
	if (!File)
	{
		OutError = "ParticleSystem: cannot write file";
		return false;
	}

	System.SetAssetPath(FAssetUtils::GetAssetKey(Path.generic_string()));
	return true;
}

bool ParticleSystemFile::IsParticleSystemPath(const FString& Path)
{
	FString Extension = std::filesystem::path(Path).extension().string();
	std::transform(Extension.begin(), Extension.end(), Extension.begin(),
		[](unsigned char C) { return static_cast<char>(std::tolower(C)); });
	return Extension == ".particle";
}

UParticleEmitter* ParticleSystemFile::CreateDefaultEmitter()
{
	UParticleEmitter* Emitter = NewObject<UParticleEmitter>();
	UParticleLODLevel* LOD = NewObject<UParticleLODLevel>();
	Emitter->LODLevels.Add(LOD);

	UParticleModuleRequired* Required = NewObject<UParticleModuleRequired>();
	Required->Texture = UAssetManager::GetAssetByPath<UTexture2D>("Engine/SubUV/StarParticle.png");
	LOD->RequiredModule = Required;

	// 2번 모듈이 있으면 UE 기본 이미터처럼 붙인다. 이름은 2번과 맞춘다. 없는 클래스는 건너뛴다
	const char* DefaultModules[] = {
		"UParticleModuleSpawn", "UParticleModuleLifetime", "UParticleModuleSize",
		"UParticleModuleVelocity", "UParticleModuleColor",
	};
	for (const char* Name : DefaultModules)
	{
		if (UClass* Class = FindClass(Name))
		{
			LOD->Modules.Add(NewObject<UParticleModule>(nullptr, Class));
		}
	}
	return Emitter;
}

bool ParticleSystemFile::CreateDefault(const std::filesystem::path& Path, FString& OutError)
{
	UParticleSystem* System = NewObject<UParticleSystem>();
	System->Emitters.Add(CreateDefaultEmitter());
	const bool bOk = Write(Path, *System, OutError);
	System->DeleteEmitters();   // 소멸자는 자식을 지우지 않는다
	delete System;   // 파일만 만든다. 여는 쪽이 LoadParticleSystem으로 다시 읽는다
	return bOk;
}