#include "persistencedata.h"

namespace ModdedPersistence
{
	PersistentVar::PersistentVar(PersistentVarDefinition* def, PersistentVarTypeVariant val)
	{
		m_definition = def;
		m_value = val;
	}

	int PersistentVar::GetAsInteger()
	{
		switch (m_definition->m_type)
		{
		case VarType::INT:
			return std::get<int>(m_value);
		case VarType::BOOL:
			return static_cast<int>(std::get<bool>(m_value));
		case VarType::ENUM:
			// todo: returns index into enum
			return -1;
		case VarType::FLOAT:
		case VarType::STRING:
		case VarType::INVALID:
			assert_msg(false, "Trying to get invalid value as an integer, check IsValidAsInteger first");
			return -1;
		}

		assert_msg(false, "PersistentVar::GetAsInteger - Unreachable VarType found?");
		return false;
	}

	PersistentVarData* PersistentVarData::GetInstance()
	{
		static PersistentVarData* instance = nullptr;
		if (instance == nullptr)
		{
			instance = new PersistentVarData();

			
		}

		return instance;
	}

	PersistentVar& PersistentVarData::GetVar(void* player, const char* name)
	{
		const size_t nameHash = STR_HASH(name);

		auto playerVars = m_persistentVars.find(player);
		assert_msg(playerVars != m_persistentVars.end(), "No persistent data for player?");

		auto var = playerVars->second.find(nameHash);
		assert_msg(var != playerVars->second.end(), "Cannot find persistent var for player " << name << " but it exists in pdef?");

		return var->second;
	}
}
