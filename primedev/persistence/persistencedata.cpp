#include "persistencedata.h"
#include "persistencedef.h"

namespace ModdedPersistence
{
	static void ResetToDefault(FlattenedDataVariable& variable)
	{
		switch (variable.m_type)
		{
		case VarType::INT:
			variable.m_value = 0;
			break;
		case VarType::FLOAT:
			variable.m_value = 0.0f;
			break;
		case VarType::BOOL:
			variable.m_value = false;
			break;
		case VarType::STRING:
		case VarType::ENUM:
			variable.m_value = "";
			break;
		default:
			spdlog::error("Invalid VarType {} for variable {}", variable.m_type, variable.m_name);
			return;
		}
	}

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
		stream.seekg(header.stringsOffset, std::ios_base::cur);
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
		using namespace ParseDefinitions;

		// sort out any changes that we made to the data before writing it
		CommitChanges();

		auto startPos = stream.tellp();
		static const std::string padStr = std::string(8, '\0');

		ParseDefinitions::DataHeader header {};
		header.version = 1;
		memcpy(&header.magic, NSPDATA_MAGIC, 4);
		// write dummy header to the stream
		stream.write(reinterpret_cast<char*>(&header), sizeof(header));

		// write all strings
		auto stringOffset = stream.tellp() - startPos;
		header.stringsOffset = stringOffset;
		header.stringCount = m_strings.size();
		for (auto& str : m_strings)
			stream.write(str.c_str(), str.length() + 1);


		// write variables
		auto variablesOffset = stream.tellp() - startPos;
		header.variablesOffset = variablesOffset;
		header.variableCount = m_variables.size();
		for (auto& variable : m_variables)
		{
			// write header
			DataVariableHeader variableHeader;
			variableHeader.name = variable.name;
			variableHeader.type = variable.type;
			variableHeader.possibilityCount = variable.possibilities.size();
			stream.write(reinterpret_cast<char*>(&variableHeader), sizeof(variableHeader));

			// write possibilities
			for (auto& possibility : variable.possibilities)
				stream.write(reinterpret_cast<char*>(&possibility), sizeof(possibility));
		}

		// write groups
		auto groupsOffset = stream.tellp() - startPos;
		header.groupsOffset = groupsOffset;
		header.groupCount = m_groups.size();
		for (auto& group : m_groups)
		{
			// write header
			DataGroupHeader groupHeader;
			groupHeader.possibilityCount = group.possibilities.size();
			stream.write(reinterpret_cast<char*>(&groupHeader), sizeof(groupHeader));

			// write group possibilities
			for (auto& groupPossibility : group.possibilities)
			{
				// write possibility header
				DataGroupPossibilityHeader groupPossibilityHeader;
				groupPossibilityHeader.dependencyCount = groupPossibility.dependencies.size();
				groupPossibilityHeader.memberCount = groupPossibility.members.size();
				stream.write(reinterpret_cast<char*>(&groupPossibilityHeader), sizeof(groupPossibilityHeader));

				// write dependencies
				for (StrIdx dependency : groupPossibility.dependencies)
					stream.write(reinterpret_cast<char*>(&dependency), sizeof(dependency));

				// write members
				for (auto& member : groupPossibility.members)
					stream.write(reinterpret_cast<char*>(&member), sizeof(member));
			}
		}

		// go back to the start and write the populated header back
		auto curPos = stream.tellp();
		stream.seekp(startPos, std::ios_base::beg);
		stream.write(reinterpret_cast<char*>(&header), sizeof(header));
		stream.seekp(curPos);

		return true;
	}

	void PersistenceDataInstance::ProcessData(std::vector<Mod>& loadedMods)
	{
		std::map<size_t, bool> enabledMods;
		for (auto& mod : loadedMods)
		{
			// find name in m_strings
			size_t hash = STR_HASH(mod.Name);
			enabledMods.emplace(hash, mod.m_bEnabled);
		}

		// iterate through all groups and determine the best possibility
		// flatten the groups into vars
		for (auto& group : m_groups)
		{
			// find all valid possibilities
			std::vector<std::pair<int, ParseDefinitions::DataGroupPossibility*>> validPossibilities;
			for (auto& possibility : group.possibilities)
			{
				bool isValid = true;
				int numMods = 0;
				for (StrIdx index : possibility.dependencies)
				{
					const size_t hash = STR_HASH(m_strings[index]);
					const auto it = enabledMods.find(hash);
					// not present in mods or is disabled
					if (it == enabledMods.end() || !it->second)
					{
						isValid = false;
						break;
					}
					++numMods;
				}

				if (!isValid)
					continue;

				validPossibilities.push_back(std::make_pair(numMods, &possibility));
			}

			// find the possibility with the highest number of mods (todo?: sorting out multiple possibilities with the same number of mods)
			int highestNumMods = -1;
			ParseDefinitions::DataGroupPossibility* bestPossibility = nullptr;
			for (auto& [numMods, possibility] : validPossibilities)
			{
				if (numMods > highestNumMods)
				{
					highestNumMods = numMods;
					bestPossibility = possibility;
				}
			}

			// no valid possibility for this group, skip loading it
			if (bestPossibility == nullptr)
				continue;

			// flatten members into usable variables for the best possibility
			for (auto& member : bestPossibility->members)
			{
				FlattenedDataVariable variable;

				variable.m_name = m_strings[member.name];
				variable.m_type = member.type;
				switch (member.type)
				{
				case VarType::INT:
					variable.m_value = member.value.asInt;
					break;
				case VarType::FLOAT:
					variable.m_value = member.value.asFloat;
					break;
				case VarType::BOOL:
					variable.m_value = member.value.asBool;
					break;
				case VarType::STRING:
				case VarType::ENUM:
					variable.m_value = m_strings[member.value.asInt];
					break;
				default:
					spdlog::error("Invalid VarType {} for variable {}", member.type, variable.m_name);
					return;
				}

				auto [foundVar, success] = m_flattened.emplace(STR_HASH(variable.m_name), variable);
				if (!success)
				{
					spdlog::error("Duplicate persistence var: {} is defined twice (instead of two possibilities)", foundVar->second.m_name);
					return;
				}
			}
		}

		// iterate through all vars and determine the best possibility
		for (auto& variable : m_variables)
		{
			// find all valid possibilities
			std::vector<std::pair<int, ParseDefinitions::DataVariablePossibility*>> validPossibilities;
			for (auto& possibility : variable.possibilities)
			{
				bool isValid = true;
				int numMods = 0;

				if (possibility.dependency != -1)
				{
					const size_t hash = STR_HASH(m_strings[possibility.dependency]);
					const auto it = enabledMods.find(hash);
					// not present in mods or is disabled
					if (it == enabledMods.end() || !it->second)
					{
						isValid = false;
						break;
					}
					++numMods;
				}

				if (!isValid)
					continue;

				validPossibilities.push_back(std::make_pair(numMods, &possibility));
			}

			// find the possibility with the highest number of mods (todo?: sorting out multiple possibilities with the same number of mods)
			int highestNumMods = -1;
			ParseDefinitions::DataVariablePossibility* bestPossibility = nullptr;
			for (auto& [numMods, possibility] : validPossibilities)
			{
				if (numMods > highestNumMods)
				{
					highestNumMods = numMods;
					bestPossibility = possibility;
				}
			}

			variable.currentChosenPossibility = bestPossibility;
			// no valid possibility for this variable, skip loading it
			if (bestPossibility == nullptr)
				continue;

			FlattenedDataVariable flatVariable;

			flatVariable.m_name = m_strings[variable.name];
			flatVariable.m_type = variable.type;
			switch (variable.type)
			{
			case VarType::INT:
				flatVariable.m_value = bestPossibility->value.asInt;
				break;
			case VarType::FLOAT:
				flatVariable.m_value = bestPossibility->value.asFloat;
				break;
			case VarType::BOOL:
				flatVariable.m_value = bestPossibility->value.asBool;
				break;
			case VarType::STRING:
			case VarType::ENUM:
				flatVariable.m_value = m_strings[bestPossibility->value.asInt];
				break;
			default:
				spdlog::error("Invalid VarType {} for variable {}", variable.type, flatVariable.m_name);
				return;
			}

			auto [foundVar, success] = m_flattened.emplace(STR_HASH(flatVariable.m_name), flatVariable);
			if (!success)
			{
				spdlog::error("Duplicate persistence var: {} is defined twice (instead of two possibilities)", foundVar->second.m_name);
				return;
			}
		}

		// add empty entries for any var defs that we don't have data for
		auto& pdef = *PersistentVarDefinitionData::GetInstance();

		for (auto& varDef : pdef.GetVars())
		{
			const size_t hash = STR_HASH(varDef.GetIdentifier());
			// vanilla defs that aren't enum values will never need modded data
			// todo: unless they are in a group with a modded value
			if (varDef.IsVanillaDef() && varDef.GetType() != VarType::ENUM)
				continue;

			auto [pair, isNew] = m_flattened.emplace(hash, FlattenedDataVariable());
			auto& variable = pair->second;
			if (isNew)
			{
				spdlog::warn("Var {} not found in persistent data, creating entry.", varDef.GetIdentifier());
				variable.m_name = varDef.GetIdentifier();
				variable.m_type = varDef.GetType();
				ResetToDefault(variable);
			}
			else
			{
				// validate type
				if (varDef.GetType() != variable.m_type)
				{
					spdlog::error(
						"{} pdef/pdata type mismatch ({} vs {}) data will be lost.", variable.m_name, varDef.GetType(), variable.m_type);
					variable.m_type = varDef.GetType();
					ResetToDefault(variable);
				}
				// validate string length
				if (varDef.GetType() == VarType::STRING && std::get<std::string>(variable.m_value).length() > varDef.GetStringSize())
				{
					spdlog::error("{} string data is beyond allowed length of {}. Resetting.", variable.m_name, varDef.GetStringSize());
					ResetToDefault(variable);
				}
			}
		}
	}

	void PersistenceDataInstance::CommitChanges()
	{
		// todo: remember to create groups if they are needed

		// go through each variable
		// check if should be grouped
		// add to group if group exists
		// create group if group doesnt exist

		// remember to override the currently selected possibility or create one if one didnt exist

		for (auto& [hash, variable] : m_flattened)
		{
			// todo: account for grouped variables.

			// find existing variable
			auto pred = [&](ParseDefinitions::DataVariable& it) -> bool
			{
				return m_strings[it.name] == variable.m_name;
			};

			auto it = std::find_if(m_variables.begin(), m_variables.end(), pred);

			if (it == m_variables.end())
			{
				// didnt find, need to add a new variable
				ParseDefinitions::DataVariable newVar {};
				newVar.name = m_strings.size();
				m_strings.push_back(variable.m_name);
				newVar.type = variable.m_type;
				newVar.currentChosenPossibility = nullptr;
				m_variables.push_back(newVar);
				it = --m_variables.end();
			}

			// update possibilities

			// create a new possibility if none are currently chosen
			if (it->currentChosenPossibility == nullptr)
			{
				it->possibilities.push_back({});
				it->currentChosenPossibility = &it->possibilities.back();
			}

			auto& chosenPossibility = *it->currentChosenPossibility;
			// update value
			switch (variable.m_type)
			{
			case VarType::INT:
				chosenPossibility.value.asInt = std::get<int>(variable.m_value);
				break;
			case VarType::FLOAT:
				chosenPossibility.value.asFloat = std::get<float>(variable.m_value);
				break;
			case VarType::BOOL:
				chosenPossibility.value.asBool = std::get<bool>(variable.m_value);
				break;
			case VarType::STRING:
				chosenPossibility.value.asString = m_strings.size();
				m_strings.push_back(std::get<std::string>(variable.m_value));
				break;
			case VarType::ENUM:
				chosenPossibility.value.asEnum = m_strings.size();
				m_strings.push_back(std::get<std::string>(variable.m_value));
				break;
			default:
				spdlog::error("Invalid VarType: {}", variable.m_type);
				continue;
			}

			// todo: update dependency (if any)
			chosenPossibility.dependency = -1;

			// remove any conflicting possibilities (current data takes precendence)
			auto removePred = [&](ParseDefinitions::DataVariablePossibility& possibility) -> bool
			{
				if (&possibility == &chosenPossibility)
					return false;
				if (possibility.dependency == -1)
					return possibility.dependency == chosenPossibility.dependency;
				// have to check the strings since we haven't yet cleaned up any duplicates etc.
				std::string& str1 = m_strings[possibility.dependency];
				std::string& str2 = m_strings[chosenPossibility.dependency];
				return str1 == str2;
			};
			auto& possibilities = it->possibilities;
			possibilities.erase(std::remove_if(possibilities.begin(), possibilities.end(), removePred), possibilities.end());
		}

		// todo: clean up strings vector and fix StrIdx values accordingly

		// iterate over all strings
		for (auto firstIt = m_strings.begin(); firstIt != m_strings.end(); ++firstIt)
		{
			// remove any strings past it that match it
			for (auto secondIt = firstIt + 1; secondIt != m_strings.end(); ++secondIt)
			{
				if (*secondIt != *firstIt)
					continue;

				StrIdx removeTarget = secondIt - m_strings.begin();
				StrIdx replaceWith = firstIt - m_strings.begin();

				// when removing a string, find any StrIdx values that reference it, and change them to point to the other one
				// also decrement all other StrIdx values that are > the index
				for (auto& variable : m_variables)
				{
					for (auto& possibility : variable.possibilities)
					{
						if (possibility.dependency == removeTarget)
							possibility.dependency = replaceWith;
						else if (possibility.dependency > removeTarget)
							--possibility.dependency;

						if (variable.type == VarType::STRING)
						{
							if (possibility.value.asString == removeTarget)
								possibility.value.asString = replaceWith;
							else if (possibility.value.asString > removeTarget)
								--possibility.value.asString;
						}
						else if (variable.type == VarType::ENUM)
						{
							if (possibility.value.asEnum == removeTarget)
								possibility.value.asEnum = replaceWith;
							else if (possibility.value.asEnum > removeTarget)
								--possibility.value.asEnum;
						}
					}
				}
				for (auto& group : m_groups)
				{
					for (auto& possibility : group.possibilities)
					{
						for (StrIdx& dependency : possibility.dependencies)
						{
							if (dependency == removeTarget)
								dependency = replaceWith;
							else if (dependency > removeTarget)
								--dependency;
						}

						for (auto& member : possibility.members)
						{
							if (member.type == VarType::STRING)
							{
								if (member.value.asString == removeTarget)
									member.value.asString = replaceWith;
								else if (member.value.asString > removeTarget)
									--member.value.asString;
							}
							else if (member.type == VarType::ENUM)
							{
								if (member.value.asEnum == removeTarget)
									member.value.asEnum = replaceWith;
								else if (member.value.asEnum > removeTarget)
									--member.value.asEnum;
							}
						}
					}
				}

				// decrement to avoid skipping one next iteration since we are removing ourselves
				m_strings.erase(secondIt--);
			}
		}
	}

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

	PersistenceDataInstance* PersistentVarData::GetDataForPlayer(CBaseClient* player)
	{
		auto it = m_persistenceData.find(player);
		if (it != m_persistenceData.end())
			return it->second.get();
		return nullptr;
	}

	bool PersistentVarData::AddClient(CBaseClient* client)
	{
		auto [pair, isNew] = m_persistenceData.emplace(client, std::make_shared<PersistenceDataInstance>(PersistenceDataInstance()));
		if (!isNew)
		{
			spdlog::error("Client {} is already registered in modded pdata", client->m_Name);
			return false;
		}
		return true;
	}

	bool PersistentVarData::LoadData(CBaseClient* client, std::istream& stream)
	{
		auto it = m_persistenceData.find(client);
		if (it == m_persistenceData.end())
		{
			spdlog::error("Client {} is not registered in modded pdata", client->m_Name);
			return false;
		}

		auto* dataInstance = it->second.get();
		if (!dataInstance->ParseFile(stream))
		{
			spdlog::error("Failed to parse modded pdata stream for client {}", client->m_Name);
			return false;
		}

		return true;
	}

	void PersistentVarData::ProcessData(CBaseClient* client, std::vector<Mod>& loadedMods)
	{
		auto it = m_persistenceData.find(client);
		if (it == m_persistenceData.end())
		{
			spdlog::error("Client {} is not registered in modded pdata", client->m_Name);
			return;
		}

		auto* dataInstance = it->second.get();
		dataInstance->ProcessData(loadedMods);
	}


	bool PersistentVarData::WriteData(CBaseClient* client, std::ostream& stream)
	{
		auto it = m_persistenceData.find(client);
		if (it == m_persistenceData.end())
		{
			spdlog::error("Client {} is not registered in modded pdata", client->m_Name);
			return false;
		}

		auto* dataInstance = it->second.get();
		if (!dataInstance->ToStream(stream))
		{
			spdlog::error("Failed to write modded pdata to stream for client {}", client->m_Name);
			return false;
		}

		return true;
	}

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