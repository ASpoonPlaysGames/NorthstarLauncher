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

	const char* WHITESPACE_CHARS = " \t\n";
	const char* VALID_IDENTIFIER_CHARS = "aAbBcCdDeEfFgGhHiIjJkKlLmMnNoOpPqQrRsStTuUvVwWxXyYzZ0123456789_";
	std::string_view ENUM_START = "$ENUM_START "; // ends with a space character since the identifier must be directly afterwards
	std::string_view ENUM_END = "$ENUM_END";
	std::string_view STRUCT_START = "$STRUCT_START "; // ends with a space character since the identifier must be directly afterwards
	std::string_view STRUCT_END = "$STRUCT_END";

	// trims leading whitespace, comments, and trailing whitespace before comments
	const std::regex LINE_TRIM_RGX("^\\s*(.*?)\\s*(?:\\/\\/.*)?$");
	const std::regex ENUM_START_RGX("\\$ENUM_START ([a-zA-Z_]+)");

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

	bool PersistentVarDefinitionData::ParsePersistence(std::stringstream& stream, const char* owningModName)
	{
		ParseDefinitions::EnumDef* currentEnum = nullptr;
		ParseDefinitions::StructDef* currentStruct = nullptr;

		std::string currentLine;
		while (std::getline(stream, currentLine))
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

	bool PersistentVarDefinitionData::ParseVarDefinition(const std::string& line, const char* owningModName, std::map<size_t, ParseDefinitions::VarDef>& parentStruct)
	{

	}

} // namespace ModdedPersistence
