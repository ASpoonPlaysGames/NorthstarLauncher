#pragma once

#include "engine/hoststate.h"
#include "core/convar/concommand.h"

#include "ns_persistence.h"

// loads the persistence from a stream
bool NSPersistenceManager::LoadFromStream(std::ifstream* stream)
{
	stream->seekg(0, std::ios::end);
	std::streamsize size = stream->tellg();
	stream->seekg(0, std::ios::beg);

	m_data = std::vector<char>(size);
	if (!stream->read(m_data.data(), size))
	{
		spdlog::error("Failed to read persistence from file!");
		return false;
	}

	// we've already read the data, we can close the stream now
	stream->close();

	// todo: refactor this to use the stream I think
	// also don't just use m_data and pointers, bad for strings and stuff
	// actually instantiate variables and stuff

	Header* hdr = (Header*)&m_data[0];

	m_sourceOffsets = std::vector<int*>(hdr->sourceIdCount);
	m_sources = std::vector<char*>(hdr->sourceIdCount);
	for (int i = 0; i < hdr->sourceIdCount; ++i)
	{
		m_sourceOffsets[i] = (int*)&m_data[sizeof(Header) + i * sizeof(int)];
		m_sources[i] = &m_data[*m_sourceOffsets[i] + sizeof(Header) + hdr->sourceIdCount * sizeof(int)];
	}

	size_t index = sizeof(Header);
	index += hdr->sourceIdCount * sizeof(int);
	index += hdr->sourceIdStringSize;
	for (int i = 0; i < hdr->dataCount; ++i)
	{
		PersistentVarHeader* varHdr = (PersistentVarHeader*)&m_data[index];
		PersistentVar datum = PersistentVar(varHdr);
		m_vars.push_back(datum);

		index += datum.Size();
	}

	spdlog::info("Loaded!");
	return true;
}

const char* NSPersistenceManager::ReadSourceFromSourceId(int index)
{
	return m_sources[index];
}

// should ideally only get called when persistence is initialising
bool NSPersistenceManager::HasVar(const char* persistenceString)
{
	for (PersistentVar& var : m_vars)
	{
		if (strcmp(persistenceString, var.GetPersistenceString().c_str()))
			continue;
		return true;
	}
	return false;
}

///////////////////////////////////////////////////////////////////////////////////////////

bool PersistentVarInt::SetValue(const char* buf, size_t bufSize)
{
	// ints are 4 bytes
	if (bufSize != 4)
		return false;

	int newValue = *(int*)buf;
	m_value = newValue;
	return true;
}

bool PersistentVarFloat::SetValue(const char* buf, size_t bufSize)
{
	// floats are 4 bytes
	if (bufSize != 4)
		return false;

	float newValue = *(float*)buf;
	m_value = newValue;
	return true;
}

bool PersistentVarBool::SetValue(const char* buf, size_t bufSize)
{
	// ints are 1 byte
	if (bufSize != 1)
		return false;

	bool newValue = *(bool*)buf;
	m_value = newValue;
	return true;
}

bool PersistentVarString::SetValue(const char* buf, size_t bufSize)
{
	// string are max 256 bytes
	if (bufSize > 256)
		return false;

	std::string newValue(buf, bufSize);
	m_value = newValue;
	return true;
}

PersistentVarEnum::PersistentVarEnum(std::string enumName, std::string identifier, std::string value)
{
	m_enumName = enumName;
	SetValue(value);
}

bool PersistentVarEnum::SetValue(const char* buf, size_t bufSize)
{
	// enums are secretly just strings, and strings are max 256 bytes
	if (bufSize > 256)
		return false;

	std::string newValue(buf, bufSize);

	return SetValue(newValue);
}

bool PersistentVarEnum::SetValue(std::string value)
{
	if (!g_persistenceDefinitionManager->EnumContainsMember(m_enumName, value))
		return false;

	m_valueContext = g_persistenceDefinitionManager->GetEnumMemberContext(m_enumName, value);

	m_value = value;
	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////

bool NSPersistenceDefinitionManager::LoadPdiff(fs::path filePath, std::string contextStr)
{
	spdlog::info("Loading pdiff from file: {}", filePath.string().c_str());
	if (m_contexts.find(contextStr) == m_contexts.end())
	{
		spdlog::info("Found new persistence context '{}'", contextStr);
		PersistentContext context;
		m_contexts.emplace(contextStr, context);
	}
	PersistentContext& context = m_contexts[contextStr];

	std::ifstream stream(filePath);
	std::stringstream pdiffStream;
	pdiffStream << stream.rdbuf();
	stream.close();

	std::string currentLine;
	// parse line by line
	while (std::getline(pdiffStream, currentLine))
	{
		// trim leading whitespace
		size_t start = currentLine.find_first_not_of(m_whitespaceChars);
		// ignore comments
		size_t end = currentLine.find("//");
		if (end == std::string::npos)
			end = currentLine.size() - 1; // last char

		// skip empty lines 
		if (!currentLine.size() || !currentLine.compare(start, 2, "//"))
			continue;

		if (!currentLine.compare(start, start + 13, "$STRUCT_START"))
		{
			PersistentStructDef structDef;

			size_t nameStart = currentLine.find_first_not_of(m_whitespaceChars, start + 13);
			if (nameStart == std::string::npos)
			{
				spdlog::error("Expected struct name, got end of line");
				return false;
			}
			size_t nameEnd = currentLine.find_first_of(m_whitespaceChars, nameStart);
			structDef.name = currentLine.substr(nameStart, nameEnd);

			// if the type already exists, we can't use this name
			if (IsValidType(contextStr, structDef.name))
			{
				spdlog::error("Invalid struct name '{}', maybe a name collision?", structDef.name);
				return false;
			}

			if (!TryParseStruct(contextStr, pdiffStream, &structDef))
				return false;

			context.structDefs.push_back(structDef);
		}
		else if (!currentLine.compare(start, start + 11, "$ENUM_START"))
		{
			size_t nameStart = currentLine.find_first_not_of(m_whitespaceChars, start + 11);
			if (nameStart == std::string::npos)
			{
				spdlog::error("Expected enum name, got end of line");
				return false;
			}
			size_t nameEnd = currentLine.find_first_of(m_whitespaceChars, nameStart);
			std::string name = currentLine.substr(nameStart, nameEnd);

			// should probably only access the vector once successfully parsed
			
			PersistentEnumDef enumDef;
			if (!TryParseEnum(contextStr, pdiffStream, &enumDef))
				return false;

			if (EnumExists(name))
			{
				// maybe this should be fatal for $ENUM_START
				spdlog::info("Enum '{}' already exists, appending...", name);
				m_enumDefs[name].insert(m_enumDefs[name].end(), enumDef.begin(), enumDef.end());
			}
			else
			{
				m_enumDefs.emplace(name, enumDef);
			}
		}
		else
		{
			PersistentVarDef var;
			if (!TryParseVar(contextStr, currentLine, &var))
				return false;

			context.varDefs.push_back(var);
		}
	}

	return true;
}

void NSPersistenceDefinitionManager::Clear()
{
	m_contexts.clear();
	m_enumDefs.clear();
	m_vars.clear();
}

void NSPersistenceDefinitionManager::PrintPdiffs()
{
	spdlog::info("ENUMS:");
	for (auto& enumPair : m_enumDefs)
	{
		spdlog::info("  - {}", enumPair.first);
		for (auto& member : enumPair.second)
		{
			spdlog::info("      - {} ({})", member.value, member.context);
		}
	}

	for (auto& contextPair : m_contexts)
	{
		spdlog::info("CONTEXT: {}", contextPair.first);
		spdlog::info("  - STRUCTS:");
		for (auto& def : contextPair.second.structDefs)
		{
			spdlog::info("    - {}", def.name);
			for (auto& var : def.vars)
			{
				spdlog::info("      - {} {} {}", var.type, var.identifier, var.arraySize);
			}
		}

		spdlog::info("  - VARS:");
		for (auto& var : contextPair.second.varDefs)
		{
			spdlog::info("      - {} {} {}", var.type, var.identifier, var.arraySize);
		}
	}
}

bool NSPersistenceDefinitionManager::TryParseEnum(std::string context, std::stringstream& structStream, PersistentEnumDef* out)
{
	ParseResult result = ParseEnum(context, structStream, out);
	if (!result.success)
	{
		spdlog::error("Failed to parse enum!");
		LogParseResult(result);
		return false;
	}

	return true;
}

ParseResult NSPersistenceDefinitionManager::ParseEnum(std::string context, std::stringstream& structStream, PersistentEnumDef* out)
{
	bool hasMembers = false;
	bool hasEnded = false;
	ParseResult result;

	std::string currentLine;

	while (std::getline(structStream, currentLine))
	{
		// trim leading whitespace
		size_t start = currentLine.find_first_not_of(m_whitespaceChars);
		// ignore comments
		size_t end = currentLine.find("//");
		if (end == std::string::npos)
			end = currentLine.size() - 1; // last char

		// skip empty lines
		if (!currentLine.size() || !currentLine.compare(start, 2, "//"))
			continue;

		if (!currentLine.compare(start, start + 11, "$ENUM_END"))
		{
			// exit if we are at the end of the struct
			hasEnded = true;
			break;
		}
		else
		{
			hasMembers = true;
			PersistentEnumMember member;
			size_t memberEnd = currentLine.find_first_not_of(m_tokenChars, start);
			size_t nextTokenChar = currentLine.find_first_not_of(m_whitespaceChars, memberEnd);

			if (nextTokenChar != std::string::npos && nextTokenChar < end)
			{
				result.error = "Expected end of line, got character.";
				result.line = currentLine;
				result.offset = nextTokenChar;
				result.success = false;
				return result;
			}

			std::string value = currentLine.substr(start, memberEnd);

			for (auto& it : *out)
			{
				if (it.value == value)
				{
					result.error = "Duplicate enum member '" + value + "'";
					result.line = currentLine;
					result.offset = start;
					result.success = false;
					return result;
				}
			}

			member.value = value;
			member.context = context;

			out->push_back(member);
		}
	}

	// empty enums aren't valid
	if (!hasMembers)
	{
		result.success = false;
		result.error = "Empty enum is not valid.";
		result.line = currentLine;
		result.offset = 0;
		return result;
	}

	if (!hasEnded)
	{
		result.success = false;
		result.error = "Expected '$ENUM_END', got eof.";
		result.line = currentLine;
		result.offset = 0;
		return result;
	}

	result.success = true;
	return result;
}

bool NSPersistenceDefinitionManager::TryParseStruct(std::string context, std::stringstream& structStream, PersistentStructDef* out)
{
	ParseResult result = ParseStruct(context, structStream, out);
	if (!result.success)
	{
		spdlog::error("Failed to parse struct!");
		LogParseResult(result);
		return false;
	}

	return true;
}

ParseResult NSPersistenceDefinitionManager::ParseStruct(std::string context, std::stringstream& structStream, PersistentStructDef* out)
{
	bool hasMembers = false;
	bool hasEnded = false;
	ParseResult result;

	std::string currentLine;

	while (std::getline(structStream, currentLine))
	{
		// trim leading whitespace
		size_t start = currentLine.find_first_not_of(m_whitespaceChars);
		// ignore comments
		size_t end = currentLine.find("//");
		if (end == std::string::npos)
			end = currentLine.size() - 1; // last char

		// skip empty lines
		if (!currentLine.size() || !currentLine.compare(start, 2, "//"))
			continue;

		if (!currentLine.compare(start, start + 11, "$STRUCT_END"))
		{
			// exit if we are at the end of the struct
			hasEnded = true;
			break;
		}
		else
		{
			hasMembers = true;
			PersistentVarDef var;
			if (!TryParseVar(context, currentLine, &var))
			{
				result.success = false;
				result.error = "Failed to parse var in struct.";
				result.line = currentLine;
				result.offset = 0;
				return result;
			}
			
			out->vars.push_back(var);
		}
	}

	// empty structs aren't valid
	if (!hasMembers)
	{
		result.success = false;
		result.error = "Empty struct is not valid.";
		result.line = currentLine;
		result.offset = 0;
		return result;
	}

	if (!hasEnded)
	{
		result.success = false;
		result.error = "Expected '$STRUCT_END', got eof.";
		result.line = currentLine;
		result.offset = 0;
		return result;
	}

	result.success = true;
	return result;
}

bool NSPersistenceDefinitionManager::TryParseVar(std::string context, std::string line, PersistentVarDef* out)
{
	ParseResult result = ParseVar(context, line, out);
	if (!result.success)
	{
		spdlog::error("Failed to parse persistent var!");
		LogParseResult(result);
		return false;
	}

	return true;
}

ParseResult NSPersistenceDefinitionManager::ParseVar(std::string context, std::string line, PersistentVarDef* out)
{
	ParseResult result;
	result.line = line;

	// trim leading whitespace
	size_t start = line.find_first_not_of(m_whitespaceChars);
	// ignore comments
	size_t end = line.find("//");
	if (end == std::string::npos)
		end = line.size(); // last char

	// trim away leading whitespace and comments
	std::string trimmed = line.substr(start, end - start);

	// get type
	out->type = trimmed.substr(0, trimmed.find_first_of(m_whitespaceChars, 0));
	if (!IsValidType(context, out->type))
	{
		result.success = false;
		result.error = "Invalid type '" + out->type + "'.";
		result.offset = start;
		return result;
	}

	// get identifier
	size_t identifierStart = trimmed.find_first_not_of(m_whitespaceChars, out->type.size());
	size_t identifierEnd = trimmed.find_first_of(m_whitespaceChars, identifierStart);

	bool isArray = false;
	// if we found [ before we found whitespace then this is an array type
	size_t arraySizeStart = trimmed.find_first_of("[", identifierStart);
	if (arraySizeStart != std::string::npos && identifierEnd > arraySizeStart)
	{
		isArray = true;
		identifierEnd = arraySizeStart++;
	}
	out->identifier = trimmed.substr(identifierStart, identifierEnd - identifierStart);
	if (!IsValidIdentifier(out->identifier))
	{
		result.success = false;
		result.error = "Invalid identifier '" + out->identifier + "'.";
		result.offset = start + identifierStart;
		return result;
	}

	// track this so we know where to start looking for the end of the line (for extra stuff that shouldn't be there)
	size_t lastRead = identifierEnd;

	// get array size (if needed)
	if (isArray)
	{
		size_t arraySizeEnd = trimmed.find_first_of("]", arraySizeStart);
		if (arraySizeEnd == std::string::npos)
		{
			result.success = false;
			result.error = "Expected ']', found end of line.";
			result.offset = start + arraySizeEnd;
			return result;
		}

		out->arraySize = trimmed.substr(arraySizeStart, arraySizeEnd - arraySizeStart);

		if (!IsValidArraySize(out->arraySize))
		{
			result.success = false;
			result.error = "Invalid array size '" + out->arraySize + "'.";
			result.offset = start + arraySizeStart;
			return result;
		}
		lastRead = arraySizeEnd + 1;
	}

	size_t next = trimmed.find_first_not_of(m_whitespaceChars, lastRead);
	if (next != std::string::npos)
	{
		result.success = false;
		result.error = "Expected end of line.";
		result.offset = start + next;
		return result;
	}

	result.success = true;
	return result;
}

void NSPersistenceDefinitionManager::LogParseResult(ParseResult& res)
{
	spdlog::error("Error: {}", res.error);
	spdlog::error("{}", res.line.c_str());
	// this doesn't align tabs properly... so this might end up looking very ugly in the future
	spdlog::error("{}^", std::string(res.offset == 0 ? 0 : res.offset - 1, ' '));
}

bool NSPersistenceDefinitionManager::EnumExists(std::string name)
{
	return m_enumDefs.find(name) != m_enumDefs.end();
}

bool NSPersistenceDefinitionManager::StructExists(std::string context, std::string name)
{
	if (m_contexts.find(context) == m_contexts.end())
		return false;

	for (auto& structDef : m_contexts[context].structDefs)
	{
		if (structDef.name == name)
			return true;
	}

	return false;
}

bool NSPersistenceDefinitionManager::IsValidType(std::string context, std::string type)
{
	// just gonna hardcode the basic types
	if (type == "int")
		return true;
	if (type == "float")
		return true;
	if (type == "bool")
		return true;
	if (type == "string")
		return true;

	// both enums and structs are valid types
	return EnumExists(type) || StructExists(context, type);
}

bool NSPersistenceDefinitionManager::IsValidIdentifier(std::string identifier)
{
	// check character set
	if (identifier.find_first_not_of(m_tokenChars) != std::string::npos)
		return false;

	// check against enum names
	for (auto& enumDef : m_enumDefs)
	{

	}

	// check against struct names from this context

	// check against var names from this context


	return true;
}

bool NSPersistenceDefinitionManager::IsValidArraySize(std::string arraySize)
{
	// stoi throws, atoi doesnt
	// 0 is not a valid array size
	if (std::atoi(arraySize.c_str()) > 0)
		return true;

	return EnumExists(arraySize);
}

bool NSPersistenceDefinitionManager::EnumContainsMember(std::string enumName, std::string memberName)
{
	if (!EnumExists(enumName))
		return false;

	for (auto& member : m_enumDefs[enumName])
	{
		if (member.value == memberName)
			return true;
	}

	return false;
}

std::string NSPersistenceDefinitionManager::GetEnumMemberContext(std::string enumName, std::string memberName)
{
	for (auto& it : m_enumDefs[enumName])
	{
		if (it.value == memberName)
			return it.context;
	}

	spdlog::error("Couldn't find enum member context: {} {}", enumName, memberName);
	return "";
}

void NSPersistenceDefinitionManager::SetupVars()
{
	for (auto& pair : m_contexts)
	{
		for (auto& var : pair.second.varDefs)
		{
			HandleVarArray(pair.first, "", var);
		}
	}
}

void NSPersistenceDefinitionManager::HandleVarArray(std::string context, std::string prefix, PersistentVarDef& def)
{
	std::string identifier = def.identifier;
	if (prefix != "")
		identifier = prefix + "." + def.identifier;

	if (IsValidArraySize(def.arraySize))
	{
		// an iterator would be better here
		if (EnumExists(def.arraySize))
		{
			for (auto& it : m_enumDefs[def.arraySize])
			{
				HandleStructChildren(context, std::format("{}[{}]", identifier, it.value), def);
			}
		}
		else
		{
			int size = std::atoi(def.arraySize.c_str());
			for (int i = 0; i < size; ++i)
			{
				HandleStructChildren(context, std::format("{}[{}]", identifier, i), def);
			}
		}
	}
	else
	{
		HandleStructChildren(context, identifier, def);
	}
}

void NSPersistenceDefinitionManager::HandleStructChildren(std::string context, std::string prefix, PersistentVarDef& def)
{
	//spdlog::info("({}) Adding var children: {} {} {} Prefix: {}", context, def.type, def.identifier, def.arraySize, prefix);
	if (!StructExists(context, def.type))
	{
		AddVar(context, def.type, prefix);
		return;
	}

	PersistentStructDef* structDef = nullptr; 
	for (auto& it : m_contexts[context].structDefs)
	{
		if (it.name == def.type)
		{
			structDef = &it;
			break;
		}
	}

	for (auto& it : structDef->vars)
	{
		HandleVarArray(context, prefix, it);
	}
	
}

void NSPersistenceDefinitionManager::AddVar(std::string context, std::string type, std::string identifier)
{
	spdlog::info("{} {} ({}) ", type, identifier, context);
}

void ConCommand_load_pdiff(const CCommand& args)
{
	spdlog::info("Loading pdiffs from files...");
	g_persistenceDefinitionManager->LoadPdiff("C:\\Users\\Jack\\Desktop\\Persistence Stuff\\Version 1\\cool.pdiff", "Test.Pdiff");
	g_persistenceDefinitionManager->LoadPdiff("C:\\Users\\Jack\\Desktop\\Persistence Stuff\\Version 1\\awesome.pdiff", "Second.Pdiff");
	//g_persistenceDefinitionManager->PrintPdiffs();
	g_persistenceDefinitionManager->SetupVars();
}

void ConCommand_clear_pdiff(const CCommand& args)
{
	spdlog::info("Clearing pdiffs...");
	g_persistenceDefinitionManager->Clear();
}

void ConCommand_load_persistence(const CCommand& args)
{
	spdlog::info("Loading persistence data from file...");
	std::ifstream stream("C:\\Users\\Jack\\Desktop\\Persistence Stuff\\Version 1\\Test.nspdata");
	g_persistenceManager->LoadFromStream(&stream);
}

ON_DLL_LOAD_RELIESON("engine.dll", NSPersistence, (ConCommand), (CModule module))
{
	g_persistenceManager           = new NSPersistenceManager();
	g_persistenceDefinitionManager = new NSPersistenceDefinitionManager();
	RegisterConCommand("ns_loadpersistence", ConCommand_load_persistence, "Loads persistence from file", FCVAR_NONE);
	RegisterConCommand("ns_loadpdiff",       ConCommand_load_pdiff,       "Loads pdiff from file",       FCVAR_NONE);
	RegisterConCommand("ns_clearpdiffs",     ConCommand_clear_pdiff,      "Clears loaded pdiffs",        FCVAR_NONE);
}
