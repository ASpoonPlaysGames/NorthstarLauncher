#include "persistencedef.h"
#include <string>

namespace ModdedPersistence
{
	const std::string TEST_PDIFF_STRING =
"////////////////////////\n\
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

	// if owningModName is an empty string, then this is vanilla persistence
	static bool ParsePersistence(std::stringstream& stream, const char* owningModName = "")
	{
		auto& dataInstance = *PersistentVarDefinitionData::GetInstance();
		const char* loggingOwnerName = strcmp(owningModName, "") ? owningModName : "Vanilla (potential launcher bug?)";

		// temp
		spdlog::info("\n\n\n\nPARSING STREAM: {}", loggingOwnerName);

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
				const size_t identifierStart = 0;
				const size_t identifierEnd = trimmedLine.find_first_of(WHITESPACE_CHARS, identifierStart);
				const std::string identifier = trimmedLine.substr(identifierStart, identifierEnd);
				const size_t firstInvalidIdentifierChar = identifier.find_first_not_of(VALID_IDENTIFIER_CHARS);
				if (firstInvalidIdentifierChar != std::string::npos)
				{
					spdlog::error("Error parsing persistence definition for {}", loggingOwnerName);
					spdlog::error("Invalid enum member '{}' in enum '{}': character '{}' is not valid.", identifier, currentEnum->GetIdentifier(), identifier[firstInvalidIdentifierChar]);
					return false;
				}

				const int memberIndex = currentEnum->GetMemberIndex(identifier.c_str());
				if (memberIndex != -1)
				{
					spdlog::error("Error parsing persistence definition for {}", loggingOwnerName);
					spdlog::error("Invalid enum member '{}' in enum '{}': duplicate member already defined in {}", identifier, currentEnum->GetIdentifier(), currentEnum->GetMemberOwner(memberIndex));
					return false;
				}

				ParseDefinitions::EnumMember member = ParseDefinitions::EnumMember();
				member.identifier = identifier;
				member.owner = owningModName;
				currentEnum->AddMember(member);
			}
			else if (trimmedLine.starts_with(ENUM_START))
			{
				// parse enum definition

				// get enum identifier
				const size_t identifierStart = ENUM_START.length();
				const size_t identifierEnd = trimmedLine.find_first_of(WHITESPACE_CHARS, identifierStart);
				const std::string identifier = trimmedLine.substr(identifierStart, identifierEnd);
				const size_t firstInvalidIdentifierChar = identifier.find_first_not_of(VALID_IDENTIFIER_CHARS);
				if (firstInvalidIdentifierChar != std::string::npos)
				{
					spdlog::error("Error parsing persistence definition for {}", loggingOwnerName);
					spdlog::error("Invalid enum identifier '{}': character '{}' is not valid.", identifier, identifier[firstInvalidIdentifierChar]);
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
			}
			else if (trimmedLine.starts_with(STRUCT_START))
			{
				// parse struct definition

				// get struct identifier
				const size_t identifierStart = STRUCT_START.length();
				const size_t identifierEnd = trimmedLine.find_first_of(WHITESPACE_CHARS, identifierStart);
				const std::string identifier = trimmedLine.substr(identifierStart, identifierEnd);
				const size_t firstInvalidIdentifierChar = identifier.find_first_not_of(VALID_IDENTIFIER_CHARS);
				if (firstInvalidIdentifierChar != std::string::npos)
				{
					spdlog::error("Error parsing persistence definition for {}", loggingOwnerName);
					spdlog::error("Invalid struct identifier '{}': character '{}' is not valid.", identifier, identifier[firstInvalidIdentifierChar]);
					return false;
				}

				// find or create struct definition

				// add members to struct definition
			}

		}

		// temp, for logging
		dataInstance.Finalise();

		return true;
	}

	namespace ParseDefinitions
	{
		TypeDef::TypeDef(const char* identifier)
			: m_identifier(identifier)
		{ }

		StructDef::StructDef(const char* identifier)
			: TypeDef(identifier)
		{ }

		EnumDef::EnumDef(const char* identifier)
			: TypeDef(identifier)
		{ }

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
	}

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

}
