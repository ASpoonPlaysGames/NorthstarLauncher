#include "persistencedef.h"

namespace ModdedPersistence
{
	PersistentVarDefinition::PersistentVarDefinition(VarType type)
	{
		m_type = type;
	}

	PersistentVarDefinitionData* PersistentVarDefinitionData::GetInstance()
	{
		static PersistentVarDefinitionData* instance = nullptr;
		if (instance == nullptr)
		{
			instance = new PersistentVarDefinitionData();
			// todo: remove test data
			instance->m_persistentVarDefs.emplace(STR_HASH("test_thing"), PersistentVarDefinition(VarType::INT));
		}

		return instance;
	}

	PersistentVarDefinition* PersistentVarDefinitionData::FindVarDefinition(const char* name, bool& success)
	{
		const size_t nameHash = STR_HASH(name);

		auto var = m_persistentVarDefs.find(nameHash);
		success = var != m_persistentVarDefs.end();

		return success ? &var->second : nullptr;
	}

	bool PersistentVarDefinitionData::LoadPersistenceBase(std::stringstream stream)
	{

	}

	bool PersistentVarDefinitionData::LoadPersistenceDiff(Mod& mod, std::stringstream stream)
	{

	}

	void PersistentVarDefinitionData::Finalise()
	{
		// flatten variables and populate m_persistentVarDefs

		m_finalised = true;
	}

	void PersistentVarDefinitionData::Clear()
	{
		m_persistentVarDefs.clear();
		// todo: clear persistent data as well since that will point to garbage now
		m_finalised = false;
	}
}
