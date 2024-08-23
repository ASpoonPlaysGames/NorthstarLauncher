#include "persistencedef.h"
#include <string>
#include <regex>

namespace ModdedPersistence
{
	const char* ID_CHARS = "aAbBcCdDeEfFgGhHiIjJkKlLmMnNoOpPqQrRsStTuUvVwWxXyYzZ0123456789_";
#define ID_RGX R"([a-zA-Z_][a-zA-Z0-9_]*)" // first character must be a non-digit

	// trims leading and trailing whitespace
	const std::regex LINE_TRIM_RGX(R"(^\s*(.*?)\s*$)");
	const std::regex ENUM_START_RGX(R"(^\$ENUM_START ()" ID_RGX R"()$)");
	const std::regex ENUM_END_RGX(R"(^\$ENUM_END$)");
	const std::regex STRUCT_START_RGX(R"(^\$STRUCT_START ()" ID_RGX R"()$)");
	const std::regex STRUCT_END_RGX(R"(^\$STRUCT_END$)");
	const std::regex VAR_DEF_RGX(R"(^()" ID_RGX R"()(?:\{(\d+)\})?\s+()" ID_RGX R"()(?:\[(\d+|)" ID_RGX R"()\])?$)");
	const std::regex DUMB_ARRAY_THING_RGX(R"(([^\]])(?=\.|$))");

	namespace ParseDefinitions
	{
		VarDef::VarDef(
			const std::string& type,
			const int nativeArraySize,
			const std::string& identifier,
			const std::string& arraySize,
			const std::string& owner)
			: m_type(type), m_nativeArraySize(nativeArraySize), m_identifier(identifier), m_arraySize(arraySize), m_owner(owner)
		{
			m_printableString = type;
			if (nativeArraySize > 0)
				m_printableString.append(std::format("{{{}}}", nativeArraySize));
			m_printableString.append(std::format(" {}", identifier));
			if (!arraySize.empty())
				m_printableString.append(std::format("[{}]", arraySize));
			m_printableString.append(std::format(" ({})", owner.empty() ? "Vanilla" : owner));
		}

		TypeDef::TypeDef(const char* identifier) : m_identifier(identifier) {}

		StructDef::StructDef(const char* identifier) : TypeDef(identifier) {}

		EnumDef::EnumDef(const char* identifier) : TypeDef(identifier) {}

		int EnumDef::GetMemberIndex(const char* identifier) const
		{
			for (int i = 0; i < m_members.size(); ++i)
			{
				if (m_members[i].identifier == identifier)
					return i;
			}

			return -1;
		}

		const char* EnumDef::GetMemberName(int index) const
		{
			assert_msg(index < m_members.size(), "EnumDef::GetMemberName | Index out of bounds");

			return m_members[index].identifier.c_str();
		}

		const char* EnumDef::GetMemberOwner(int index) const
		{
			assert_msg(index < m_members.size(), "EnumDef::GetMemberOwner | Index out of bounds");

			return m_members[index].owner.c_str();
		}

		void EnumDef::AddMember(EnumMember member)
		{
			const char* idStr = member.identifier.c_str();
			int existingIndex = GetMemberIndex(idStr);
			if (existingIndex != -1)
			{
				spdlog::error("EnumDef::AddMember | Member already exists: {} ({})", idStr, GetMemberOwner(existingIndex));
				return;
			}

			m_members.push_back(member);
		}
	} // namespace ParseDefinitions

	PersistentVarDefinition::PersistentVarDefinition(VarType type, std::string identifier, std::vector<std::string>& dependencies)
		: m_type(type)
		, m_identifier(identifier)
		, m_dependencies(dependencies)
		, m_enumType(nullptr)
	{
	}

	PersistentVarDefinition::PersistentVarDefinition(
		VarType type, std::string identifier, std::vector<std::string>& dependencies, const ParseDefinitions::EnumDef* enumType)
		: m_type(type)
		, m_identifier(identifier)
		, m_dependencies(dependencies)
		, m_enumType(enumType)
	{
	}

	PersistentVarDefinitionData* PersistentVarDefinitionData::GetInstance()
	{
		static PersistentVarDefinitionData* instance = nullptr;
		if (instance == nullptr)
		{
			instance = new PersistentVarDefinitionData();
		}

		return instance;
	}

	void PersistentVarDefinitionData::LogVarDefinitions()
	{
		spdlog::info("\n\n");
		spdlog::info("LOGGING RAW VAR DEFINITIONS");
		spdlog::info("\n\n");
		spdlog::info("ENUMS:");
		for (auto& it : m_types)
		{
			auto* enumDef = dynamic_cast<ParseDefinitions::EnumDef*>(it.second.get());
			if (enumDef == nullptr)
				continue;

			spdlog::info("{}", enumDef->GetIdentifier());
			int memberCount = enumDef->GetMemberCount();
			for (int i = 0; i < memberCount; ++i)
			{
				spdlog::info("\t{} ({})", enumDef->GetMemberName(i), enumDef->GetMemberOwner(i));
			}
			spdlog::info("");
		}
		spdlog::info("STRUCTS:");
		for (auto& it : m_types)
		{
			auto* structDef = dynamic_cast<ParseDefinitions::StructDef*>(it.second.get());
			if (structDef == nullptr)
				continue;

			spdlog::info("{}", structDef->GetIdentifier());

			for (auto& [hash, member] : structDef->GetMembers())
			{
				spdlog::info("\t{}", member.ToString());
			}
			spdlog::info("");
		}
		spdlog::info("VARS:");
		for (auto& [hash, var] : m_vars)
		{
			spdlog::info("{}", var.ToString());
		}
		spdlog::info("\n\n");
	}

	PersistentVarDefinition* PersistentVarDefinitionData::FindVarDefinition(const char* name)
	{
		// for some reason, vanilla allows a def like "int xp" to be accessed though "xp[0]"? bit weird but ig i have to support it.
		std::string fixedName = std::regex_replace(name, DUMB_ARRAY_THING_RGX, "$1[0]");


		const size_t nameHash = STR_HASH(fixedName);
		auto var = m_persistentVarDefs.find(nameHash);

		return var != m_persistentVarDefs.end() ? &var->second : nullptr;
	}

	bool PersistentVarDefinitionData::LoadPersistenceBase(std::stringstream& stream)
	{
		return ParsePersistence(stream, "");
	}

	bool PersistentVarDefinitionData::LoadPersistenceDiff(Mod& mod, std::stringstream& stream)
	{
		const char* modName = mod.Name.c_str();
		return ParsePersistence(stream, modName);
	}

	void PersistentVarDefinitionData::Finalise()
	{
		//LogVarDefinitions();

		// flatten variables and populate m_persistentVarDefs

		FlattenVariables();

		//spdlog::warn("\n\n");
		//spdlog::warn("PRINTING FLATTENED:");

		//for (auto& [hash, var] : m_persistentVarDefs)
		//{
		//	spdlog::warn(var.GetIdentifier());
		//}

		//spdlog::warn("\n\n");

		m_finalised = true;
	}

	void PersistentVarDefinitionData::Clear()
	{
		m_persistentVarDefs.clear();
		m_vars.clear();
		m_types.clear();
		// todo: clear persistent data as well since that will point to garbage now
		m_finalised = false;
	}

	ParseDefinitions::TypeDef* PersistentVarDefinitionData::GetTypeDefinition(const char* identifier)
	{
		const size_t idHash = STR_HASH(identifier);

		auto it = m_types.find(idHash);
		return it != m_types.end() ? it->second.get() : nullptr;
	}

	ParseDefinitions::TypeDef* PersistentVarDefinitionData::RegisterTypeDefinition(std::shared_ptr<ParseDefinitions::TypeDef> typeDef)
	{
		const char* identifier = typeDef->GetIdentifier();
		const size_t idHash = STR_HASH(identifier);

		// ensure that the identifier isn't already in use
		if (m_vars.find(idHash) != m_vars.end())
		{
			spdlog::error("Failed to register persistent type: Identifier {} is already defined as a persistent var.");
			return nullptr;
		}
		if (m_types.find(idHash) != m_types.end())
		{
			spdlog::error("Failed to register persistent type: Identifier {} is already defined as a persistent type.");
			return nullptr;
		}

		return m_types.emplace(idHash, typeDef).first->second.get();
	}

	ParseDefinitions::VarDef* PersistentVarDefinitionData::GetVarDefinition(const char* identifier)
	{
		const size_t idHash = STR_HASH(identifier);

		auto it = m_vars.find(idHash);
		return it != m_vars.end() ? &it->second : nullptr;
	}

	ParseDefinitions::VarDef* PersistentVarDefinitionData::RegisterVarDefinition(ParseDefinitions::VarDef varDef)
	{
		const char* identifier = varDef.GetIdentifier();
		const size_t idHash = STR_HASH(identifier);

		// ensure that the identifier isn't already in use
		if (m_vars.find(idHash) != m_vars.end())
		{
			spdlog::error("Failed to register persistent var: Identifier {} is already defined as a persistent var.");
			return nullptr;
		}
		if (m_types.find(idHash) != m_types.end())
		{
			spdlog::error("Failed to register persistent var: Identifier {} is already defined as a persistent type.");
			return nullptr;
		}

		return &m_vars.emplace(idHash, varDef).first->second;
	}

	bool PersistentVarDefinitionData::ParsePersistence(std::stringstream& stream, const char* owningModName)
	{
		ParseDefinitions::EnumDef* currentEnum = nullptr;
		ParseDefinitions::StructDef* currentStruct = nullptr;

		unsigned int lineNumber = 0;
		std::string currentLine;
		while (++lineNumber, std::getline(stream, currentLine))
		{
			// trim comments
			std::string trimmedLine = currentLine.substr(0, currentLine.find("//"));
			// trim whitespace
			{
				std::smatch matches;
				if (!std::regex_search(trimmedLine, matches, LINE_TRIM_RGX))
				{
					spdlog::error("Failed to match line trimming regex? Probable launcher bug D:");
					spdlog::error(currentLine);
					return false;
				}
				if (matches.size() != 2)
				{
					// note: the vector of matches includes the full matches and the captured groups like this
					// full, group1, group2, full, group1, group2, etc.
					// so because we have 1 capture group, we expect two entries, the first being the full match
					// and the second being the capture group.
					spdlog::error("Got {} regex matches (expected 2) for line trimming regex? Probable launcher bug D:", matches.size());
					spdlog::error(currentLine);
					for (auto& it : matches)
						spdlog::error(it.str());
					return false;
				}

				trimmedLine = matches[1];
			}
			// pure whitespace and/or comment line, nothing to parse
			if (trimmedLine.empty())
				continue;

			// parsing state machine

			if (trimmedLine.starts_with('$'))
			{
				// command sequences ($ENUM_START etc.)
				std::smatch commandMatches;
				if (std::regex_match(trimmedLine, commandMatches, ENUM_START_RGX))
				{
					if (currentEnum != nullptr || currentStruct != nullptr)
					{
						spdlog::error("Unexpected command sequence {} on line {} ({})", trimmedLine, lineNumber, owningModName);
						return false;
					}

					// find or create enum definition
					// todo: change signature to just take a string ref? str().c_str() is dumb
					ParseDefinitions::TypeDef* typeDef = GetTypeDefinition(commandMatches[1].str().c_str());
					if (typeDef == nullptr)
					{
						auto newEnumDef =
							std::make_shared<ParseDefinitions::EnumDef>(ParseDefinitions::EnumDef(commandMatches[1].str().c_str()));
						typeDef = RegisterTypeDefinition(newEnumDef);
					}

					auto* foundEnumDef = dynamic_cast<ParseDefinitions::EnumDef*>(typeDef);
					if (foundEnumDef == nullptr)
					{
						spdlog::error(
							"Invalid enum identifier: '{}' ({}) is already defined as a struct", commandMatches[1].str(), owningModName);
						return false;
					}

					// set currentEnum
					currentEnum = foundEnumDef;
				}
				else if (std::regex_match(trimmedLine, commandMatches, ENUM_END_RGX))
				{
					if (currentEnum == nullptr || currentStruct != nullptr)
					{
						spdlog::error("Unexpected command sequence {} on line {} ({})", trimmedLine, lineNumber, owningModName);
						return false;
					}

					currentEnum = nullptr;
				}
				else if (std::regex_match(trimmedLine, commandMatches, STRUCT_START_RGX))
				{
					if (currentEnum != nullptr || currentStruct != nullptr)
					{
						spdlog::error("Unexpected command sequence {} on line {} ({})", trimmedLine, lineNumber, owningModName);
						return false;
					}

					// find or create struct definition
					// todo: change signature to just take a string ref? str().c_str() is dumb
					ParseDefinitions::TypeDef* typeDef = GetTypeDefinition(commandMatches[1].str().c_str());
					if (typeDef == nullptr)
					{
						auto newStructDef =
							std::make_shared<ParseDefinitions::StructDef>(ParseDefinitions::StructDef(commandMatches[1].str().c_str()));
						typeDef = RegisterTypeDefinition(newStructDef);
					}

					auto* foundStructDef = dynamic_cast<ParseDefinitions::StructDef*>(typeDef);
					if (foundStructDef == nullptr)
					{
						spdlog::error(
							"Invalid struct identifier: '{}' ({}) is already defined as an enum", commandMatches[1].str(), owningModName);
						return false;
					}

					// set currentEnum
					currentStruct = foundStructDef;
				}
				else if (std::regex_match(trimmedLine, commandMatches, STRUCT_END_RGX))
				{
					if (currentEnum != nullptr || currentStruct == nullptr)
					{
						spdlog::error("Unexpected command sequence {} on line {} ({})", trimmedLine, lineNumber, owningModName);
						return false;
					}

					currentStruct = nullptr;
				}
				else
				{
					spdlog::error("Invalid command sequence '{}' on line {} ({})", trimmedLine, lineNumber, owningModName);
					return false;
				}
			}
			else if (currentEnum != nullptr)
			{
				// enum member parsing
				if (!ParseEnumMember(trimmedLine, owningModName, *currentEnum))
				{
					spdlog::error("Failed parsing enum member on line {} ({})", lineNumber, owningModName);
					return false;
				}
			}
			else
			{
				// var parsing
				if (!ParseVarDefinition(trimmedLine, owningModName, currentStruct != nullptr ? currentStruct->GetMembers() : m_vars))
				{
					spdlog::error("Failed parsing var definition on line {} ({})", lineNumber, owningModName);
					return false;
				}
			}
		}

		return true;
	}

	bool
	PersistentVarDefinitionData::ParseEnumMember(const std::string& line, const char* owningModName, ParseDefinitions::EnumDef& parentEnum)
	{
		const size_t firstInvalid = line.find_first_not_of(ID_CHARS);
		if (firstInvalid != std::string::npos)
		{
			spdlog::error("Invalid character {} in enum member '{}'", line[firstInvalid], line);
			return false;
		}

		const int index = parentEnum.GetMemberIndex(line.c_str());
		if (index != -1)
		{
			spdlog::error("Invalid enum member {}, member already exists in {}", line, parentEnum.GetMemberOwner(index));
			return false;
		}

		ParseDefinitions::EnumMember member;
		member.identifier = line;
		member.owner = owningModName;

		parentEnum.AddMember(member);
		return true;
	}

	bool PersistentVarDefinitionData::ParseVarDefinition(
		const std::string& line, const char* owningModName, std::map<size_t, ParseDefinitions::VarDef>& targetMap)
	{
		std::smatch matches;
		if (!std::regex_search(line, matches, VAR_DEF_RGX))
		{
			spdlog::error("Invalid var definition: line doesn't match var definition regex");
			spdlog::error(line);
			return false;
		}

		if (matches.size() != 5)
		{
			// note: the vector of matches includes the full matches and the captured groups like this
			// full, group1, group2, full, group1, group2, etc.
			spdlog::error("Got {} regex matches (expected 5) for var definition regex? Probable launcher bug D:", matches.size());
			spdlog::error(line);
			for (auto& it : matches)
				spdlog::error(it.str());
			return false;
		}

		const std::string type = matches[1];
		const std::string nativeArraySize = matches[2];
		const std::string identifier = matches[3];
		const std::string arraySize = matches[4];

		int nativeArraySizeInt = 0;
		if (!nativeArraySize.empty())
			nativeArraySizeInt = std::stoi(nativeArraySize);

		ParseDefinitions::VarDef varDef(type, nativeArraySizeInt, identifier, arraySize, owningModName);

		auto [foundVarDef, emplaceSuccess] = targetMap.emplace(STR_HASH(identifier), varDef);
		if (!emplaceSuccess)
		{
			spdlog::error("Var name collision: '{}' already defined in {}", identifier, foundVarDef->second.GetOwner());
			spdlog::error(line);
			return false;
		}

		return true;
	}

	void PersistentVarDefinitionData::FlattenVariables()
	{
		GatherVariables(m_vars, m_persistentVarDefs, "", {}, {});
	}

	void PersistentVarDefinitionData::GatherVariables(
		const std::map<size_t, ParseDefinitions::VarDef>& sourceVarDefs,
		std::map<size_t, PersistentVarDefinition>& targetVarDefs,
		std::string idPrefix,
		std::vector<std::string> dependentMods,
		std::vector<size_t> structStack)
	{
		for (auto& [idHash, varDef] : sourceVarDefs)
		{
			std::string varDefIdStr = std::format("{}{}", idPrefix, varDef.GetIdentifier());
			std::vector<std::string> varDependentMods = dependentMods;
			if (varDef.GetOwner() == "") // no mod owner, vanilla def
				varDependentMods.push_back(varDef.GetOwner());

			// find array size
			std::vector<std::string> arrayMembers;
			std::string arraySize = varDef.GetArraySize();
			if (arraySize.empty())
			{
				arrayMembers = {"0"};
			}
			else if (auto* typeDef = GetTypeDefinition(arraySize.c_str()); typeDef != nullptr)
			{
				// found type, make sure its en enum
				const ParseDefinitions::EnumDef* enumDef = dynamic_cast<ParseDefinitions::EnumDef*>(typeDef);
				if (enumDef == nullptr)
				{
					spdlog::error("Invalid array size '{}' for variable '{}' ({})", arraySize, varDefIdStr, varDef.GetOwner());
					spdlog::error(varDef.ToString());
					continue;
				}

				// add each enum member to array member strings
				const int memberCount = enumDef->GetMemberCount();
				for (int i = 0; i < memberCount; ++i)
					arrayMembers.push_back(enumDef->GetMemberName(i));
			}
			else if (arraySize.find_first_not_of("0123456789") == std::string::npos)
			{
				// found number, construct array member strings
				const int arraySizeInt = std::stoi(arraySize);
				for (int i = 0; i < arraySizeInt; ++i)
					arrayMembers.push_back(std::to_string(i));
			}

			// loop over array members
			for (auto& arrayMember : arrayMembers)
			{
				const char* typeStr = varDef.GetType();
				std::string idStr = varDefIdStr;
				idStr.append(std::format("[{}]", arrayMember));

				VarType targetType = VarType::INVALID;

				// no switch on string in C++ :(
				if (!strcmp(typeStr, "bool"))
				{
					targetType = VarType::BOOL;
				}
				else if (!strcmp(typeStr, "int"))
				{
					targetType = VarType::INT;
				}
				else if (!strcmp(typeStr, "float"))
				{
					targetType = VarType::FLOAT;
				}
				else if (!strcmp(typeStr, "string"))
				{
					targetType = VarType::STRING;
				}

				if (targetType != VarType::INVALID)
				{
					PersistentVarDefinition toAdd(targetType, idStr, varDependentMods);
					if (targetType == VarType::STRING)
						toAdd.SetStringSize(varDef.GetNativeArraySize());

					targetVarDefs.emplace(STR_HASH(idStr), toAdd);

					continue;
				}

				auto* typeDef = GetTypeDefinition(typeStr);

				if (typeDef == nullptr)
				{
					spdlog::error("Var type not found: {}", typeStr);
					continue;
				}

				const ParseDefinitions::EnumDef* enumDef = dynamic_cast<ParseDefinitions::EnumDef*>(typeDef);
				if (enumDef != nullptr)
				{
					PersistentVarDefinition toAdd(VarType::ENUM, idStr, varDependentMods, enumDef);
					targetVarDefs.emplace(STR_HASH(idStr), toAdd);
					continue;
				}

				const ParseDefinitions::StructDef* structDef = dynamic_cast<ParseDefinitions::StructDef*>(typeDef);
				if (structDef != nullptr)
				{
					const size_t curStructId = STR_HASH(typeStr);
					// check for looping definitions
					for (size_t structId : structStack)
					{
						if (structId == curStructId)
						{
							spdlog::error("Circular struct definition detected: {} defines a member of it's own type", typeStr);
							continue;
						}
					}

					structStack.push_back(curStructId);
					GatherVariables(structDef->GetMembers(), targetVarDefs, idStr + ".", varDependentMods, structStack);
					structStack.pop_back();
					continue;
				}
			}
		}
	}

} // namespace ModdedPersistence
