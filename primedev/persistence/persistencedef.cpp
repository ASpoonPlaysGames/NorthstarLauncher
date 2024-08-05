#include "persistencedef.h"
#include <string>

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

	const char* WHITESPACE_CHARS = " \t\n";
	const char* VALID_IDENTIFIER_CHARS = "aAbBcCdDeEfFgGhHiIjJkKlLmMnNoOpPqQrRsStTuUvVwWxXyYzZ0123456789_";
	std::string_view ENUM_START = "$ENUM_START "; // ends with a space character since the identifier must be directly afterwards
	std::string_view ENUM_END = "$ENUM_END";
	std::string_view STRUCT_START = "$STRUCT_START "; // ends with a space character since the identifier must be directly afterwards
	std::string_view STRUCT_END = "$STRUCT_END";

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
		spdlog::info("\n\n\n\nENUMS:");
		for (auto& it : m_types)
		{
			auto* enumDef = dynamic_cast<ParseDefinitions::EnumDef*>(&it.second);
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
		return it != m_types.end() ? &it->second : nullptr;
	}

	ParseDefinitions::TypeDef* PersistentVarDefinitionData::RegisterTypeDefinition(ParseDefinitions::TypeDef typeDef)
	{
		const char* identifier = typeDef.GetIdentifier();
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

		return &m_types.emplace(idHash, typeDef).first->second;
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

	// if owningModName is an empty string, then this is vanilla persistence
	// note: i hate this function, its messy and bad
	bool PersistentVarDefinitionData::ParsePersistence(std::stringstream& stream, const char* owningModName)
	{
		auto& dataInstance = *PersistentVarDefinitionData::GetInstance();
		const char* loggingOwnerName = strcmp(owningModName, "") ? owningModName : "Vanilla (potential launcher bug?)";

		// temp
		spdlog::info("\n\n\n\nPARSING STREAM: {}", loggingOwnerName);

		// one or neither of these can be non-nullptr at once
		ParseDefinitions::EnumDef* currentEnum = nullptr;
		ParseDefinitions::StructDef* currentStruct = nullptr;

		std::string currentLine;
		while (std::getline(stream, currentLine))
		{
			// trim leading whitespace and trailing comments
			const size_t firstNonWhitespace = currentLine.find_first_not_of(WHITESPACE_CHARS);
			if (firstNonWhitespace == std::string::npos)
				continue; // only whitespace
			const size_t commentStart = currentLine.find("//");
			std::string trimmedLine = currentLine.substr(firstNonWhitespace, commentStart - firstNonWhitespace);

			// entire line is whitespace and/or comment
			if (trimmedLine.length() == 0)
				continue;

			// temp
			spdlog::info(trimmedLine);

			if (currentEnum != nullptr)
			{
				// check for enum end
				if (trimmedLine.starts_with(ENUM_END))
				{
					currentEnum = nullptr;
					continue;
				}

				// currently in an enum definition, parse enum member
				auto [identifier, firstInvalidCharIndex] = ParseTypeIdentifier(trimmedLine, 0);
				if (firstInvalidCharIndex != std::string::npos)
				{
					spdlog::error("Error parsing persistence definition for {}", loggingOwnerName);
					spdlog::error(
						"Invalid enum member '{}' in enum '{}': character '{}' is not valid.",
						identifier,
						currentEnum->GetIdentifier(),
						identifier[firstInvalidCharIndex]);
					return false;
				}

				const int memberIndex = currentEnum->GetMemberIndex(identifier.c_str());
				if (memberIndex != -1)
				{
					spdlog::error("Error parsing persistence definition for {}", loggingOwnerName);
					spdlog::error(
						"Invalid enum member '{}' in enum '{}': duplicate member already defined in {}",
						identifier,
						currentEnum->GetIdentifier(),
						currentEnum->GetMemberOwner(memberIndex));
					return false;
				}

				ParseDefinitions::EnumMember member = ParseDefinitions::EnumMember();
				member.identifier = identifier;
				member.owner = owningModName;
				currentEnum->AddMember(member);
			}
			else if (currentStruct != nullptr && trimmedLine.starts_with(STRUCT_END))
			{
				currentStruct = nullptr;
				continue;
			}
			else if (trimmedLine.starts_with(ENUM_START))
			{
				// can't define enums inside of other enums
				if (currentEnum != nullptr)
				{
					spdlog::error("Error parsing persistence definition for {}", loggingOwnerName);
					spdlog::error("Trying to define a new enum inside of an enum: '{}'", currentEnum->GetIdentifier());
					return false;
				}

				// can't define enums inside a struct definition
				if (currentStruct != nullptr)
				{
					spdlog::error("Error parsing persistence definition for {}", loggingOwnerName);
					spdlog::error("Trying to define a new enum inside of a struct: '{}'", currentStruct->GetIdentifier());
					return false;
				}

				// get enum identifier
				auto [identifier, firstInvalidCharIndex] = ParseTypeIdentifier(trimmedLine, ENUM_START.length());
				if (firstInvalidCharIndex != std::string::npos)
				{
					spdlog::error("Error parsing persistence definition for {}", loggingOwnerName);
					spdlog::error(
						"Invalid enum identifier '{}': character '{}' is not valid.", identifier, identifier[firstInvalidCharIndex]);
					return false;
				}

				// find or create enum definition (parsing members starts next iteration)
				auto* typeDef = dataInstance.GetTypeDefinition(identifier.c_str());
				if (typeDef == nullptr)
					typeDef = dataInstance.RegisterTypeDefinition(ParseDefinitions::EnumDef(identifier.c_str()));

				// duplicate enum definitions are ok, (they just get concatenated) but we can't have conflicts between a struct and an enum
				auto* enumDef = dynamic_cast<ParseDefinitions::EnumDef*>(typeDef);
				if (enumDef == nullptr)
				{
					spdlog::error("Error parsing persistence definition for {}", loggingOwnerName);
					spdlog::error("Invalid enum identifier '{}': identifier already exists as a struct", identifier);
					return false;
				}

				currentEnum = enumDef;
				continue;
			}
			else if (trimmedLine.starts_with(STRUCT_START))
			{
				// can't define structs inside an enum definition
				if (currentEnum != nullptr)
				{
					spdlog::error("Error parsing persistence definition for {}", loggingOwnerName);
					spdlog::error("Trying to define a new struct inside of an enum: '{}'", currentEnum->GetIdentifier());
					return false;
				}

				// can't define structs inside of other structs
				if (currentStruct != nullptr)
				{
					spdlog::error("Error parsing persistence definition for {}", loggingOwnerName);
					spdlog::error("Trying to define a new struct inside of a struct: '{}'", currentStruct->GetIdentifier());
					return false;
				}

				// get struct identifier
				auto [identifier, firstInvalidCharIndex] = ParseTypeIdentifier(trimmedLine, STRUCT_START.length());
				if (firstInvalidCharIndex != std::string::npos)
				{
					spdlog::error("Error parsing persistence definition for {}", loggingOwnerName);
					spdlog::error(
						"Invalid struct identifier '{}': character '{}' is not valid.", identifier, identifier[firstInvalidCharIndex]);
					return false;
				}

				// find or create enum definition (parsing members starts next iteration)
				auto* typeDef = dataInstance.GetTypeDefinition(identifier.c_str());
				if (typeDef == nullptr)
					typeDef = dataInstance.RegisterTypeDefinition(ParseDefinitions::StructDef(identifier.c_str()));

				// duplicate struct definitions are ok, (they just get concatenated) but we can't have conflicts between a struct and an
				// enum
				auto* structDef = dynamic_cast<ParseDefinitions::StructDef*>(typeDef);
				if (structDef == nullptr)
				{
					spdlog::error("Error parsing persistence definition for {}", loggingOwnerName);
					spdlog::error("Invalid struct identifier '{}': identifier already exists as an enum", identifier);
					return false;
				}

				currentStruct = structDef;
				continue;
			}
			else
			{
				// general var parsing, adds to currentStruct if non-nullptr

				// parse type identifier. no validity checks here, checked later

				// parse identifier

				// parse array size (optional)

			}
		}

		// temp, for logging
		dataInstance.Finalise();

		return true;
	}

	std::pair<std::string, const size_t> PersistentVarDefinitionData::ParseTypeIdentifier(std::string& line, const size_t identifierStart)
	{
		const size_t identifierEnd = line.find_first_of(WHITESPACE_CHARS, identifierStart);
		const std::string identifier = line.substr(identifierStart, identifierEnd);
		const size_t firstInvalidIdentifierChar = identifier.find_first_not_of(VALID_IDENTIFIER_CHARS);

		return std::make_pair(identifier, firstInvalidIdentifierChar);
	}

} // namespace ModdedPersistence
