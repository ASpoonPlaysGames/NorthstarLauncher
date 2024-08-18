#include "persistencedata.h"

namespace ModdedPersistence
{

	PersistentVariablePossibility::PersistentVariablePossibility(PersistentVariable& parent) : m_parent(parent) {}

	bool PersistentVariablePossibility::FromStream(std::istream& stream, PersistentVariablePossibility& out)
	{
		stream.read(reinterpret_cast<char*>(&out.m_dependencyIndex), sizeof(out.m_dependencyIndex));

		switch (out.m_parent.m_type)
		{
		case (VarType::BOOL):
		{
			bool value = false;
			stream.read(reinterpret_cast<char*>(&value), sizeof(value));
			out.m_value = value;
			break;
		}
		case (VarType::INT):
		{
			int value = 0;
			stream.read(reinterpret_cast<char*>(&value), sizeof(value));
			out.m_value = value;
			break;
		}
		case (VarType::FLOAT):
		{
			float value = false;
			stream.read(reinterpret_cast<char*>(&value), sizeof(value));
			out.m_value = value;
			break;
		}
		case (VarType::STRING):
		case (VarType::ENUM):
		{
			std::string value;
			std::getline(stream, value, '\00');
			out.m_value = value;
			break;
		}
		default:
			spdlog::error("Invalid type {}", out.m_parent.m_type);
			return false;
		}

		return true;
	}

	bool PersistentVariablePossibility::ToStream(std::ostream& stream)
	{
		stream.write(reinterpret_cast<char*>(&m_dependencyIndex), sizeof(m_dependencyIndex));

		switch (m_parent.m_type)
		{
		case (VarType::BOOL):
		{
			bool value = std::get<bool>(m_value);
			stream.write(reinterpret_cast<char*>(&value), sizeof(value));
			break;
		}
		case (VarType::INT):
		{
			int value = std::get<int>(m_value);
			stream.write(reinterpret_cast<char*>(&value), sizeof(value));
			break;
		}
		case (VarType::FLOAT):
		{
			float value = std::get<float>(m_value);
			stream.write(reinterpret_cast<char*>(&value), sizeof(value));
			break;
		}
		case (VarType::STRING):
		case (VarType::ENUM):
		{
			std::string value = std::get<std::string>(m_value);
			stream.write(value.c_str(), value.length());
			break;
		}
		default:
			spdlog::error("Invalid type {}", m_parent.m_type);
			return false;
		}

		return true;
	}

	std::vector<bool> ModdedPersistence::PersistentVariablePossibility::GetDependencies()
	{
		size_t dependencySize = m_parent.m_parent.m_dependencies.size();
		std::vector<bool> ret(dependencySize);

		assert(m_dependencyIndex < dependencySize);
		ret[m_dependencyIndex] = true;

		return ret;
	}

	PersistentVariable::PersistentVariable(PersistenceDataInstance& parent) : m_parent(parent) {}

	bool PersistentVariable::FromStream(std::istream& stream, PersistentVariable& out)
	{
		stream.read(reinterpret_cast<char*>(&out.m_identifierIndex), sizeof(out.m_identifierIndex));
		stream.read(reinterpret_cast<char*>(&out.m_type), sizeof(out.m_type));

		unsigned int possibilityCount = 0;
		stream.read(reinterpret_cast<char*>(&possibilityCount), sizeof(possibilityCount));

		out.m_possibilities.resize(possibilityCount, PersistentVariablePossibility(out));
		for (unsigned int i = 0; i < possibilityCount; ++i)
			if (!PersistentVariablePossibility::FromStream(stream, out.m_possibilities[i]))
				return false;

		return true;
	}

	bool PersistentVariable::ToStream(std::ostream& stream)
	{
		stream.write(reinterpret_cast<char*>(&m_identifierIndex), sizeof(m_identifierIndex));
		stream.write(reinterpret_cast<char*>(&m_type), sizeof(m_type));

		unsigned int possibilityCount = static_cast<unsigned int>(m_possibilities.size());
		stream.write(reinterpret_cast<char*>(&possibilityCount), sizeof(possibilityCount));

		for (auto& possibility : m_possibilities)
			if (!possibility.ToStream(stream))
				return false;

		return true;
	}

	PersistentVariablePossibility& PersistentVariable::GetBestPossibility()
	{
		// TODO: insert return statement here
		int highestScore = -1;
		PersistentVariablePossibility* ret = nullptr;

		for (auto& possibility : m_possibilities)
		{
			int score = 0;
			bool valid = true;
			auto dependencies = possibility.GetDependencies();
			assert(dependencies.size() == m_parent.m_enabledDependencies.size());

			for (int i = 0; i < dependencies.size(); ++i)
			{
				if (dependencies[i] != m_parent.m_enabledDependencies[i])
				{
					valid = false;
					return;
				}

				if (dependencies[i])
					++score;
			}

			if (valid && score > highestScore)
			{
				highestScore = score;
				ret = &possibility;
			}
		}

		if (ret)
			return *ret;
		else
			return m_possibilities.emplace_back(this); // make a new one if we dont have any valid ones
	}

	PersistentGroupPossibility::PersistentGroupPossibility(PersistentGroup& parent) : m_parent(parent) {}

	bool PersistentGroupPossibility::FromStream(std::istream& stream, PersistentGroupPossibility& out)
	{
		// todo: implement
		return false;
	}

	bool PersistentGroupPossibility::ToStream(std::ostream& stream)
	{
		// todo: implement
		return false;
	}

	std::vector<bool> PersistentGroupPossibility::GetDependencies()
	{
		// todo: this is kinda shit 
		std::vector<bool> ret(m_parent.m_parent.m_identifiers.size());

		for (auto& possibility : m_members)
		{
			auto dep = possibility.GetDependencies();
			std::transform(ret.begin(), ret.end(), dep.begin(), ret.begin(), [](bool first, bool second) { return first || second; });
		}

		return ret;
	}

	PersistentGroup::PersistentGroup(PersistenceDataInstance& parent) : m_parent(parent) {}

	bool PersistentGroup::FromStream(std::istream& stream, PersistentGroup& out)
	{
		// todo: implement
		return false;
	}

	bool PersistentGroup::ToStream(std::ostream& stream)
	{
		// todo: implement
		return false;
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
		auto startPos = stream.tellg();

		PersistenceDataHeader header {};
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

		// read dependencies
		// todo: one line? cpp doesnt like me doing maths with streampos though
		stream.seekg(startPos, std::ios_base::beg);
		stream.seekg(header.dependenciesOffset, std::ios_base::cur);
		out.m_dependencies.resize(header.dependencyCount);
		for (unsigned int i = 0; i < header.dependencyCount; ++i)
			std::getline(stream, out.m_dependencies[i], '\0');

		// read identifiers
		stream.seekg(startPos, std::ios_base::beg);
		stream.seekg(header.identifiersOffset, std::ios_base::cur);
		out.m_identifiers.resize(header.identifiersCount);
		for (unsigned int i = 0; i < header.identifiersCount; ++i)
			std::getline(stream, out.m_identifiers[i], '\0');

		// read variables
		stream.seekg(startPos, std::ios_base::beg);
		stream.seekg(header.variablesOffset, std::ios_base::cur);
		out.m_variables.resize(header.variableCount, PersistentVariable(out));
		for (unsigned int i = 0; i < header.variableCount; ++i)
			if (!PersistentVariable::FromStream(stream, out.m_variables[i]))
				return false;

		// read groups
		stream.seekg(startPos, std::ios_base::beg);
		stream.seekg(header.groupsOffset, std::ios_base::cur);
		out.m_groups.resize(header.groupCount, PersistentGroup(out));
		for (unsigned int i = 0; i < header.groupCount; ++i)
			if (!PersistentGroup::FromStream(stream, out.m_groups[i]))
				return false;

		return true;
	}

	bool PersistenceDataInstance::ToStream(std::ostream& stream)
	{
		// sort out any changes that we made to the data before writing it
		CommitChanges();

		auto startPos = stream.tellp();
		static const std::string padStr = std::string(8, '\0');

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
		stream.write(padStr.c_str(), 8 - dependenciesEnd % 8);

		// write variables
		header.variableCount = m_variables.size();
		header.variablesOffset = stream.tellp() - startPos;
		for (auto& variable : m_variables)
			if (!variable.ToStream(stream))
				return false;

		auto variablesEnd = stream.tellp();
		stream.write(padStr.c_str(), 8 - variablesEnd % 8);

		// write groups
		header.groupCount = m_groups.size();
		header.groupsOffset = stream.tellp() - startPos;
		for (auto& group : m_groups)
			if (!group.ToStream(stream))
				return false;

		auto groupsEnd = stream.tellp();
		stream.write(padStr.c_str(), 8 - groupsEnd % 8);

		// write identifiers
		header.identifiersCount = m_identifiers.size();
		header.identifiersOffset = stream.tellp() - startPos;
		for (auto& identifier : m_identifiers)
			stream.write(identifier.c_str(), identifier.size());

		auto identifiersEnd = stream.tellp();
		stream.write(padStr.c_str(), 8 - identifiersEnd % 8);

		// go back to the start and write the populated header back
		auto curPos = stream.tellp();
		stream.seekp(startPos, std::ios_base::beg);
		stream.write(reinterpret_cast<char*>(&header), sizeof(header));
		stream.seekp(curPos);

		return true;
	}

	void PersistenceDataInstance::Finalise(std::vector<std::string> loadedModNames)
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
