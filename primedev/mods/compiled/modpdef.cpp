#include "mods/modmanager.h"
#include "core/filesystem/filesystem.h"
#include "persistence/persistencedef.h"

#include <map>
#include <sstream>
#include <fstream>

const char* VPK_PDEF_PATH = "cfg/server/persistent_player_data_version_231.pdef";

void ModManager::BuildPdef()
{
	// todo: move this to a different function/file since it's no longer a compiled mod file
	spdlog::info("Building persistence definition...");

	auto* pdef = ModdedPersistence::PersistentVarDefinitionData::GetInstance();
	pdef->Clear();

	std::string pdefStr = ReadVPKOriginalFile(VPK_PDEF_PATH);
	std::stringstream stream(pdefStr);
	pdef->LoadPersistenceBase(stream);

	for (Mod& mod : m_LoadedMods)
	{
		if (!mod.m_bEnabled || !mod.Pdiff.size())
			continue;

		std::stringstream modStream(mod.Pdiff);
		pdef->LoadPersistenceDiff(mod, modStream);
	}

	pdef->Finalise();
}
