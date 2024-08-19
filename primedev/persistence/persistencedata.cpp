#include "persistencedata.h"

namespace ModdedPersistence
{
	PersistenceDataInstance::PersistenceDataInstance() {}

	bool PersistenceDataInstance::ParseFile(std::istream& stream)
	{
		using namespace ParseDefinitions;

		auto startPos = stream.tellg();

		DataHeader header {};
		stream.read(reinterpret_cast<char*>(&header), sizeof(header));

		if (header.version != 1)
		{
			spdlog::error("Invalid nspdata version {} (expected 1)", header.version);
			return false;
		}

		if (strncmp(header.magic, NSPDATA_MAGIC, 4))
		{
			spdlog::error("Invalid nspdata file magic?");
			return false;
		}

		// read all strings
		stream.seekg(startPos, std::ios_base::beg);
		stream.seekg(header.groupsOffset, std::ios_base::cur);
		m_strings.resize(header.stringCount);
		for (unsigned int i = 0; i < header.stringCount; ++i)
			std::getline(stream, m_strings[i], '\00');

		// read variables
		stream.seekg(startPos, std::ios_base::beg);
		stream.seekg(header.variablesOffset, std::ios_base::cur);
		m_variables.resize(header.variableCount);
		for (unsigned int variableIndex = 0; variableIndex < header.variableCount; ++variableIndex)
		{
			DataVariable& variable = m_variables[variableIndex];

			DataVariableHeader variableHeader;
			stream.read(reinterpret_cast<char*>(&variableHeader), sizeof(variableHeader));

			variable.name = variableHeader.name;
			variable.type = variableHeader.type;

			// read possibilities
			variable.possibilities.resize(variableHeader.possibilityCount);
			for (unsigned int possibilityIndex = 0; possibilityIndex < variableHeader.possibilityCount; ++possibilityIndex)
			{
				DataVariablePossibility& variablePossibility = variable.possibilities[possibilityIndex];

				stream.read(reinterpret_cast<char*>(&variablePossibility.dependency), sizeof(variablePossibility.dependency));
				// is this UB? could be ig...
				stream.read(reinterpret_cast<char*>(&variablePossibility.value), sizeof(variablePossibility.value));
			}
		}

		// read groups
		stream.seekg(startPos, std::ios_base::beg);
		stream.seekg(header.groupsOffset, std::ios_base::cur);
		m_groups.resize(header.groupCount);
		for (unsigned int groupIndex = 0; groupIndex < header.groupCount; ++groupIndex)
		{
			DataGroup& group = m_groups[groupIndex];

			DataGroupHeader groupHeader;
			stream.read(reinterpret_cast<char*>(&groupHeader), sizeof(groupHeader));

			// read group possibilities
			group.possibilities.resize(groupHeader.possibilityCount);
			for (unsigned int possibilityIndex = 0; possibilityIndex < groupHeader.possibilityCount; ++possibilityIndex)
			{
				DataGroupPossibility& groupPossibility = group.possibilities[possibilityIndex];

				DataGroupPossibilityHeader groupPossibilityHeader;
				stream.read(reinterpret_cast<char*>(&groupPossibilityHeader), sizeof(groupPossibilityHeader));

				// read dependencies
				groupPossibility.dependencies.resize(groupPossibilityHeader.dependencyCount);
				for (unsigned int dependencyIndex = 0; dependencyIndex < groupPossibilityHeader.dependencyCount; ++dependencyIndex)
				{
					StrIdx& stringsIndex = groupPossibility.dependencies[dependencyIndex];
					stream.read(reinterpret_cast<char*>(&stringsIndex), sizeof(stringsIndex));
				}

				// read members
				groupPossibility.members.resize(groupPossibilityHeader.memberCount);
				for (unsigned int memberIndex = 0; memberIndex < groupPossibilityHeader.memberCount; ++memberIndex)
				{
					DataGroupMember& member = groupPossibility.members[memberIndex];
					stream.read(reinterpret_cast<char*>(&member), sizeof(member));
				}
			}
		}

		return true;
	}

	bool PersistenceDataInstance::ToStream(std::ostream& stream)
	{
		// sort out any changes that we made to the data before writing it
		CommitChanges();

		auto startPos = stream.tellp();
		static const std::string padStr = std::string(8, '\0');

		ParseDefinitions::DataHeader header {};
		header.version = 1;
		memcpy(&header.magic, NSPDATA_MAGIC, 4);
		// write dummy header to the stream
		stream.write(reinterpret_cast<char*>(&header), sizeof(header));

		// todo: implement

		// go back to the start and write the populated header back
		auto curPos = stream.tellp();
		stream.seekp(startPos, std::ios_base::beg);
		stream.write(reinterpret_cast<char*>(&header), sizeof(header));
		stream.seekp(curPos);

		return true;
	}

	void PersistenceDataInstance::Finalise(std::vector<Mod>& loadedMods)
	{
		// todo: implement

		// iterate over pdef variables
		// get best possibility for each variable
	}

	void PersistenceDataInstance::CommitChanges()
	{
		// todo: implement
	}

	// PersistentVar::PersistentVar(PersistentVarDefinition* def, PersistentVarTypeVariant val)
	//{
	//	m_definition = def;
	//	m_value = val;
	// }

	// int PersistentVar::GetAsInteger()
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

	// PersistentVar& PersistentVarData::GetVar(void* player, const char* name)
	//{
	//	const size_t nameHash = STR_HASH(name);

	//	auto playerVars = m_persistentVars.find(player);
	//	assert_msg(playerVars != m_persistentVars.end(), "No persistent data for player?");

	//	auto var = playerVars->second.find(nameHash);
	//	assert_msg(var != playerVars->second.end(), "Cannot find persistent var for player " << name << " but it exists in pdef?");

	//	return var->second;
	//}

} // namespace ModdedPersistence

void ConCommand_persistence_dump(const CCommand& args)
{
	// commit changes

	// write to file stream
}

void ConCommand_persistence_load(const CCommand& args)
{
	// clear data

	// load from file stream
}

ON_DLL_LOAD("engine.dll", PersistenceData, (CModule module))
{
	RegisterConCommand("persistence_dump", ConCommand_persistence_dump, "Dumps persistence data to a file", FCVAR_GAMEDLL);
	RegisterConCommand("persistence_load", ConCommand_persistence_load, "Loads persistence data from a file", FCVAR_GAMEDLL);
}
