#include "persistencedef.h"
#include <string>
#include <regex>

namespace ModdedPersistence
{
	// todo: remove temp testing string and/or read from file
	const std::string TEST_PDIFF_STRING = "////////////////////////\n\
// BASIC TYPE TESTING //\n\
////////////////////////\n\
\n\
int test_int\n\
float test_float\n\
bool test_bool\n\
string{16} test_string\n\
int test_array[69]\n\
\n\
//////////////////\n\
// ENUM TESTING //\n\
//////////////////\n\
\n\
$ENUM_START test_enum_def\n\
	test_enum_def_0\n\
	test_enum_def_1\n\
$ENUM_END\n\
\n\
test_enum_def test_enum\n\
// since we parse types first before variables, this should have length of 3\n\
int test_enum_array[test_enum_def]\n\
\n\
// defining the same enum twice just concatenates them, but they cannot contain duplicate members\n\
$ENUM_START test_enum_def\n\
	test_enum_def_2\n\
$ENUM_END\n\
\n\
////////////////////\n\
// STRUCT TESTING //\n\
////////////////////\n\
\n\
$STRUCT_START test_struct_def\n\
	int test_struct_member_int\n\
	string{16} test_struct_member_string\n\
$STRUCT_END\n\
\n\
// this should also have the float member\n\
test_struct_def test_struct_instance\n\
\n\
// defining the same struct twice also concatenates them, but they cannot contain duplicate members\n\
$STRUCT_START test_struct_def\n\
	float test_struct_member_float\n\
$STRUCT_END\n\
";

	const char* VALID_IDENTIFIER_CHARS = "aAbBcCdDeEfFgGhHiIjJkKlLmMnNoOpPqQrRsStTuUvVwWxXyYzZ0123456789_";

	#define ID_RGX "[a-zA-Z0-9_]+"

	// trims leading whitespace, comments, and trailing whitespace before comments
	const std::regex LINE_TRIM_RGX("^\\s*(.*?)\\s*(?:\\/\\/.*)?$");
	const std::regex ENUM_START_RGX("^\\$ENUM_START (" ID_RGX ")$");
	const std::regex ENUM_END_RGX("^\\$ENUM_END$");
	const std::regex STRUCT_START_RGX("^\\$STRUCT_START (" ID_RGX ")$");
	const std::regex STRUCT_END_RGX("^\\$STRUCT_END$");
	const std::regex VAR_DEF_RGX("^(" ID_RGX ")(?:\\{(\\d+)\\})?\\s+(" ID_RGX ")(?:\\[(\\d+|" ID_RGX ")\\])?$");

	namespace ParseDefinitions
	{
		TypeDef::TypeDef(const char* identifier) : m_identifier(identifier) {}

		StructDef::StructDef(const char* identifier) : TypeDef(identifier) {}

		EnumDef::EnumDef(const char* identifier) : TypeDef(identifier) {}

		int EnumDef::GetMemberIndex(const char* identifier)
		{
			for (int i = 0; i < m_members.size(); ++i)
			{
				if (m_members[i].identifier == identifier)
					return i;
			}

			return -1;
		}

		const char* EnumDef::GetMemberName(int index)
		{
			assert_msg(index < m_members.size(), "EnumDef::GetMemberName | Index out of bounds");

			return m_members[index].identifier.c_str();
		}

		const char* EnumDef::GetMemberOwner(int index)
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

			// todo: remove hardcoded file
			std::stringstream stream(TEST_PDIFF_STRING);
			instance->LoadPersistenceBase(stream);
			instance->Finalise();
		}

		return instance;
	}

	PersistentVarDefinition* PersistentVarDefinitionData::FindVarDefinition(const char* name)
	{
		const size_t nameHash = STR_HASH(name);
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
		// temp: logging all of the members
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
		spdlog::info("\n\n");
		spdlog::info("STRUCTS:");
		for (auto& it : m_types)
		{
			auto* structDef = dynamic_cast<ParseDefinitions::StructDef*>(it.second.get());
			if (structDef == nullptr)
				continue;

			spdlog::info("{}", structDef->GetIdentifier());

			for (auto& [hash, member] : structDef->GetMembers())
			{
				spdlog::info("\t{}{{{}}} {}[{}] ({})", member.m_type, member.m_nativeArraySize, member.m_identifier, member.m_arraySize, member.m_owner);
			}
			spdlog::info("");
		}
		spdlog::info("\n\n");
		spdlog::info("VARS:");
		for (auto& [hash, var] : m_vars)
		{
			spdlog::info("{}{{{}}} {}[{}] ({})", var.m_type, var.m_nativeArraySize, var.m_identifier, var.m_arraySize, var.m_owner);
		}
		spdlog::info("\n\n");

		// flatten variables and populate m_persistentVarDefs

		m_finalised = true;
	}

	void PersistentVarDefinitionData::Clear()
	{
		m_persistentVarDefs.clear();
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
		const char* identifier = varDef.m_identifier.c_str();
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
			// trim whitespace and comments
			std::smatch matches;
			if (!std::regex_search(currentLine, matches, LINE_TRIM_RGX))
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

			std::string trimmedLine = matches[1];
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
						auto newEnumDef = std::make_shared<ParseDefinitions::EnumDef>(ParseDefinitions::EnumDef(commandMatches[1].str().c_str()));
						typeDef = RegisterTypeDefinition(newEnumDef);
					}

					auto* foundEnumDef = dynamic_cast<ParseDefinitions::EnumDef*>(typeDef);
					if (foundEnumDef == nullptr)
					{
						spdlog::error("Invalid enum identifier: '{}' ({}) is already defined as a struct", commandMatches[1].str(), owningModName);
						return false;
					}

					// set currentEnum
					currentEnum = foundEnumDef;
				}
				else if(std::regex_match(trimmedLine, commandMatches, ENUM_END_RGX))
				{
					if (currentEnum == nullptr || currentStruct != nullptr)
					{
						spdlog::error("Unexpected command sequence {} on line {} ({})", trimmedLine, lineNumber, owningModName);
						return false;
					}

					currentEnum = nullptr;
				}
				else if(std::regex_match(trimmedLine, commandMatches, STRUCT_START_RGX))
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
						auto newStructDef = std::make_shared<ParseDefinitions::StructDef>(ParseDefinitions::StructDef(commandMatches[1].str().c_str()));
						typeDef = RegisterTypeDefinition(newStructDef);
					}

					auto* foundStructDef = dynamic_cast<ParseDefinitions::StructDef*>(typeDef);
					if (foundStructDef == nullptr)
					{
						spdlog::error("Invalid struct identifier: '{}' ({}) is already defined as an enum", commandMatches[1].str(), owningModName);
						return false;
					}

					// set currentEnum
					currentStruct = foundStructDef;
				}
				else if(std::regex_match(trimmedLine, commandMatches, STRUCT_END_RGX))
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

	bool PersistentVarDefinitionData::ParseEnumMember(const std::string& line, const char* owningModName, ParseDefinitions::EnumDef& parentEnum)
	{
		const size_t firstInvalid = line.find_first_not_of(VALID_IDENTIFIER_CHARS);
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

	bool PersistentVarDefinitionData::ParseVarDefinition(const std::string& line, const char* owningModName, std::map<size_t, ParseDefinitions::VarDef>& targetMap)
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

		ParseDefinitions::VarDef varDef;
		varDef.m_type = type;
		if (!nativeArraySize.empty())
			varDef.m_nativeArraySize = std::stoi(nativeArraySize);
		else
			varDef.m_nativeArraySize = 1;
		varDef.m_identifier = identifier;
		varDef.m_arraySize = arraySize;
		varDef.m_owner = owningModName;

		auto [foundVarDef, emplaceSuccess] = targetMap.emplace(GetHash(identifier), varDef);
		if (!emplaceSuccess)
		{
			spdlog::error("Var name collision: '{}' already defined in {}", identifier, foundVarDef->second.m_owner);
			spdlog::error(line);
			return false;
		}

		return true;
	}

} // namespace ModdedPersistence
