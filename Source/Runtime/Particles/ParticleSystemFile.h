#pragma once

#include "Core/EngineString.h"
#include <filesystem>

class UParticleSystem;
class UParticleEmitter;

namespace ParticleSystemFile
{
	bool Read(const std::filesystem::path& Path, UParticleSystem& System, FString& OutError);
	bool Write(const std::filesystem::path& Path, UParticleSystem& System, FString& OutError);
	bool IsParticleSystemPath(const FString& Path);

	UParticleEmitter* CreateDefaultEmitter();

	bool CreateDefault(const std::filesystem::path& Path, FString& OutError);
}