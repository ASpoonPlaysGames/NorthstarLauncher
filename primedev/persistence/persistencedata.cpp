#include "persistencedata.h"

namespace ModdedPersistence
{








	bool PersistentVariable::FromStream(std::istream& stream, PersistenceDataInstance& parent, PersistentVariable& out)
	{
		out = PersistentVariable();
		out.m_parent = parent;
		auto startPos = stream.tellg();

		stream.read(&out.m_identifierIndex, sizeof(out.m_identifierIndex));
		stream.read(&out.m_type, sizeof(out.m_type));

		unsigned int possibilityCount;
		stream.read(&possibilityCount, sizeof(possibilityCount));

		for (unsigned int i = 0; i < possibilityCount; ++i)
		{
			PersistentVariablePossibility possibility;
			if (!PersistentVariablePossibility::FromStream(stream, possibility))
				return false;
			AddPossibility(possibility);
		}

		return true;
	}

	bool PersistentVariable::ToStream(std::ostream& stream)
	{
		stream.write(&m_identifierIndex, sizeof(m_identifierIndex));
		stream.write(&m_type, sizeof(m_type));

		unsigned int possibilityCount = static_cast<unsigned int>(m_possibilities.size());
		stream.write(&possibilityCount, sizeof(possibilityCount));

		for (auto& possibility : m_possibilities)
			if (!possibility.ToStream(stream))
				return false;

		return true;
	}






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

	bool PersistenceDataInstance::FromStream(std::istream& stream, PersistenceDataInstance& out)
	{
		out = PersistenceDataInstance();
		auto startPos = stream.tellg();

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
		// todo: one line? cpp doesnt like me doing maths with streampos though
		stream.seekg(startPos, std::ios_base::beg);
		stream.seekg(header.dependenciesOffset, std::ios_base::cur);
		out.m_dependencies.reserve(header.dependencyCount);
		for (int i = 0; i < header.dependencyCount; ++i)
			std::getline(stream, out.m_dependencies[i], '\0');

		// read identifiers
		stream.seekg(startPos, std::ios_base::beg);
		stream.seekg(header.identifiersOffset, std::ios_base::cur);
		out.m_identifiers.reserve(header.identifiersCount);
		for (int i = 0; i < header.identifiersCount; ++i)
			std::getline(stream, out.m_identifiers[i], '\0');

		// read variables
		stream.seekg(startPos, std::ios_base::beg);
		stream.seekg(header.variablesOffset, std::ios_base::cur);
		out.m_variables.reserve(header.variableCount);
		for (int i = 0; i < header.variableCount; ++i)
			if (!PersistentVariable::FromStream(stream, out.m_variables[i]))
				return false;

		// read groups
		stream.seekg(startPos, std::ios_base::beg);
		stream.seekg(header.groupsOffset, std::ios_base::cur);
		out.m_groups.reserve(header.groupCount);
		for (int i = 0; i < header.groupCount; ++i)
			if (!PersistentGroup::FromStream(stream, out.m_groups[i]))
				return false;

		return true;
	}

	bool PersistenceDataInstance::ToStream(std::ostream& stream)
	{
		// sort out any changes that we made to the data before writing it
		CommitChanges();

		auto startPos = stream.tellp();
		static const char* padStr = std::string(8, '\0').c_str();

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

		auto dependenciesEnd = stream.tellp();
		stream.write(padStr, 8 - dependenciesEnd % 8);

		// write variables
		header.variableCount = m_variables.size();
		header.variablesOffset = stream.tellp() - startPos;
		for (auto& variable : m_variables)
			if (!variable.ToStream(stream))
				return false;

		auto variablesEnd = stream.tellp();
		stream.write(padStr, 8 - variablesEnd % 8);

		// write groups
		header.groupCount = m_groups.size();
		header.groupsOffset = stream.tellp() - startPos;
		for (auto& group : m_groups)
			if (!group.ToStream(stream))
				return false;

		auto groupsEnd = stream.tellp();
		stream.write(padStr, 8 - groupsEnd % 8);

		// write identifiers
		header.identifiersCount = m_identifiers.size();
		header.identifiersOffset = stream.tellp() - startPos;
		for (auto& identifier : m_identifiers)
			stream.write(identifier.c_str(), identifier.size());

		auto identifiersEnd = stream.tellp();
		stream.write(padStr, 8 - identifiersEnd % 8);

		// go back to the start and write the populated header back
		auto curPos = stream.tellp();
		stream.seekp(startPos, std::ios_base::beg);
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
