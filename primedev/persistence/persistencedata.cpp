#include "persistencedata.h"

namespace ModdedPersistence
{















	int PersistenceDataInstance::GetDependencyIndex(const char* dependency)
	{
		const int originalSize = static_cast<int>(m_dependencies.size());

		for (int i = 0; i < originalSize; ++i)
		{
			if (m_dependencies[i] == dependency)
				return i;
		}

		m_dependencies.push_back(dependency);
		return originalSize;
	}

	bool PersistenceDataInstance::FromStream(std::istream stream)
	{
		PersistenceDataHeader header {};
		stream.read(reinterpret_cast<char*>(&header), sizeof(header));

		if (header.version != 1)
		{
			spdlog::error("Invalid nspdata version {} (expected 1)", header.version);
			return false;
		}

		if (reinterpret_cast<int>(header.magic) != reinterpret_cast<int>(NSPDATA_MAGIC))
		{
			spdlog::error("Invalid nspdata file magic?");
			return false;
		}

		// read dependencies

		// read variables

		// read groups

		// read identifiers

		return true;
	}

	bool PersistenceDataInstance::ToStream(std::ostream& stream)
	{
		// sort out any changes that we made to the data before writing it
		CommitChanges();

		auto startPos = stream.tellp();

		PersistenceDataHeader header {};
		header.version = 1;
		memcpy(&header.magic, NSPDATA_MAGIC, 4);
		// write dummy header to the stream
		stream.write(reinterpret_cast<char*>(&header), sizeof(header));

		// write dependencies
		header.dependencyCount = m_dependencies.size();
		header.dependenciesOffset = stream.tellp() - startPos;
		for (auto& dependency : m_dependencies)
			stream.write(dependency.c_str(), dependency.size());

		// write variables
		header.variableCount = m_variables.size();
		header.variablesOffset = stream.tellp() - startPos;
		for (auto& variable : m_variables)
			if (!variable.ToStream(stream))
				return false;

		// write groups
		header.groupCount = m_groups.size();
		header.groupsOffset = stream.tellp() - startPos;
		for (auto& group : m_groups)
			if (!group.ToStream(stream))
				return false;

		// write identifiers
		header.identifiersCount = m_identifiers.size();
		header.identifiersOffset = stream.tellp() - startPos;
		for (auto& identifier : m_identifiers)
			stream.write(identifier.c_str(), identifier.size());

		// go back to the start and write the populated header back
		auto curPos = stream.tellp();
		stream.seekp(startPos);
		stream.write(reinterpret_cast<char*>(&header), sizeof(header));
		stream.seekp(curPos);

		return true;
	}


	//PersistentVar::PersistentVar(PersistentVarDefinition* def, PersistentVarTypeVariant val)
	//{
	//	m_definition = def;
	//	m_value = val;
	//}

	//int PersistentVar::GetAsInteger()
	//{
	//	switch (m_definition->m_type)
	//	{
	//	case VarType::INT:
	//	case VarType::ENUM:
	//		return std::get<int>(m_value);
	//	case VarType::BOOL:
	//		return static_cast<int>(std::get<bool>(m_value));
	//	case VarType::FLOAT:
	//	case VarType::STRING:
	//	case VarType::INVALID:
	//		assert_msg(false, "Trying to get invalid value as an integer, check IsValidAsInteger first");
	//		return -1;
	//	}

	//	assert_msg(false, "PersistentVar::GetAsInteger - Unreachable VarType found?");
	//	return false;
	//}

	PersistentVarData* PersistentVarData::GetInstance()
	{
		static PersistentVarData* instance = nullptr;
		if (instance == nullptr)
		{
			instance = new PersistentVarData();

			// todo: remove test data
			/*auto varDefs = PersistentVarDefinitionData::GetInstance();
			PersistentVarTypeVariant variant;
			variant = 69;
			auto newVar = PersistentVar(varDefs->FindVarDefinition("test_enum_array[test_enum_def_2]"), variant);
			std::map<size_t, PersistentVar> persistentVars;
			persistentVars.emplace(STR_HASH("test_enum_array[test_enum_def_2]"), newVar);
			instance->m_persistentVars.emplace(nullptr, persistentVars);*/
		}

		return instance;
	}

	//PersistentVar& PersistentVarData::GetVar(void* player, const char* name)
	//{
	//	const size_t nameHash = STR_HASH(name);

	//	auto playerVars = m_persistentVars.find(player);
	//	assert_msg(playerVars != m_persistentVars.end(), "No persistent data for player?");

	//	auto var = playerVars->second.find(nameHash);
	//	assert_msg(var != playerVars->second.end(), "Cannot find persistent var for player " << name << " but it exists in pdef?");

	//	return var->second;
	//}
}
