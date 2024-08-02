#include "persistencedef.h"
#include <string>

namespace ModdedPersistence
{
	const std::string TEST_PDIFF_STRING =
"////////////////////////\
// BASIC TYPE TESTING //\
////////////////////////\
\
int test_int\
float test_float\
bool test_bool\
string{16} test_string\
int test_array[69]\
\
//////////////////\
// ENUM TESTING //\
//////////////////\
\
$ENUM_START test_enum_def\
	test_enum_def_0\
	test_enum_def_1\
$ENUM_END\
\
test_enum_def test_enum\
// since we parse types first before variables, this should have length of 3\
int test_enum_array[test_enum_def]\
\
// defining the same enum twice just concatenates them, but they cannot contain duplicate members\
$ENUM_START test_enum_def\
	test_enum_def_2\
$ENUM_END\
\
////////////////////\
// STRUCT TESTING //\
////////////////////\
\
$STRUCT_START test_struct_def\
	int test_struct_member_int\
	string{16} test_struct_member_string\
$STRUCT_END\
\
// this should also have the float member\
test_struct_def test_struct_instance\
\
// defining the same struct twice also concatenates them, but they cannot contain duplicate members\
$STRUCT_START test_struct_def\
	float test_struct_member_float\
$STRUCT_END\
";


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

	const char* WHITESPACE_CHARS = " \t\n";
	const char* VALID_IDENTIFIER_CHARS = "aAbBcCdDeEfFgGhHiIjJkKlLmMnNoOpPqQrRsStTuUvVwWxXyYzZ0123456789_";
	std::string_view ENUM_START = "$ENUM_START "; // ends with a space character since the identifier must be directly afterwards
	std::string_view ENUM_END = "$ENUM_END";
	std::string_view STRUCT_START = "$STRUCT_START "; // ends with a space character since the identifier must be directly afterwards
	std::string_view STRUCT_END = "$STRUCT_END";

	// if owningModName is nullptr, then this is vanilla persistence
	static bool ParsePersistence(std::stringstream& stream, const char* owningModName = nullptr)
	{
		auto& dataInstance = *PersistentVarDefinitionData::GetInstance();
		const char* loggingOwnerName = owningModName == nullptr ? "Vanilla (potential launcher bug?)" : owningModName;

		// temp
		spdlog::info("\n\n\n\nPARSING STREAM: {}", loggingOwnerName);

		ParseDefinitions::EnumDef* currentEnum = nullptr;
		ParseDefinitions::StructDef* currentStruct = nullptr;

		std::string currentLine;
		while (std::getline(stream, currentLine))
		{
			// trim leading whitespace and trailing comments
			const size_t firstNonWhitespace = currentLine.find_first_not_of(WHITESPACE_CHARS);
			const size_t commentStart = currentLine.find("//");
			std::string trimmedLine = currentLine.substr(firstNonWhitespace, commentStart - firstNonWhitespace);

			// entire line is whitespace and/or comment
			if (trimmedLine.length() == 0)
				continue;

			// temp
			spdlog::info(trimmedLine);

			if (currentEnum != nullptr)
			{
				// currently in an enum definition, parse enum member
				const size_t identifierStart = 0;
				const size_t identifierEnd = trimmedLine.find_first_of(WHITESPACE_CHARS, identifierStart);
				const std::string identifier = trimmedLine.substr(identifierStart, identifierEnd);
				const size_t firstInvalidIdentifierChar = identifier.find_first_not_of(VALID_IDENTIFIER_CHARS);
				if (firstInvalidIdentifierChar != std::string::npos)
				{
					spdlog::error("Error parsing persistence definition for {}", loggingOwnerName);
					spdlog::error("Invalid enum member '{}' in enum '{}': character '{}' is not valid.", identifier, currentEnum->GetName(), identifier[firstInvalidIdentifierChar]);
					return false;
				}

				const int memberIndex = currentEnum->GetMemberIndex(identifier);
				if (memberIndex == -1)
				{
					spdlog::error("Error parsing persistence definition for {}", loggingOwnerName);
					spdlog::error("Invalid enum member '{}' in enum '{}': duplicate member already defined in {}", identifier, currentEnum->GetName(), currentEnum->GetMemberOwner(memberIndex));
					return false;
				}

				currentEnum->AddMember(identifier, owningModName);
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
				currentEnum = &dataInstance.GetEnumDefinition(identifier, true);
			}
			else if (trimmedLine.starts_with(STRUCT_START))
			{
				// parse struct definition

				// get struct identifier

				// find or create struct definition

				// add members to struct definition
			}

		}

		return true;
	}

	bool PersistentVarDefinitionData::LoadPersistenceBase(std::stringstream& stream)
	{
		return ParsePersistence(stream, nullptr);
	}

	bool PersistentVarDefinitionData::LoadPersistenceDiff(Mod& mod, std::stringstream& stream)
	{
		const char* modName = mod.Name.c_str();
		return ParsePersistence(stream, modName);
	}

	void PersistentVarDefinitionData::Finalise()
	{
		// flatten variables and populate m_persistentVarDefs

		m_finalised = true;
	}

	void PersistentVarDefinitionData::Clear()
	{
		m_persistentVarDefs.clear();
		// todo: clear persistent data as well since that will point to garbage now
		m_finalised = false;
	}
}
