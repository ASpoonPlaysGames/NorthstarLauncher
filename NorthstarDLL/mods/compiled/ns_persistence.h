#pragma once

#include "pch.h"
#include <fstream>
#include <filesystem>
#include <string>
#include <thread>

struct Header
{
	int magic;
	int version;
	// a sourceId is a mod identifier, data with different sourceIds should be stored separately on MS
	// a mod doesn't necessarily have to use it's name for a sourceId, and a mod may have multiple sourceIds
	// the sourceId must be defined at the top of a pdiff file
	int sourceIdCount;
	int sourceIdStringSize;
	int dataCount;
};

struct PersistentVarHeader
{
	// which sourceId this variable was declared by
	int declarationSourceIdIndex;
	// which sourceId the value of this variable belongs to (used for enum types and such)
	int valueSourceIdIndex;
	// note: structs are not valid types here, they get split into different PersistentVar for each member
	// enum types are treated like string types, but with some validation (no bad enum values)
	int typeSize;
	// the full persistenceId that script uses to get/set the value of this PersistentVar
	// (the string that you give to GetPersistentVarAsInt)
	// arrays arent actually real, they just get split into thing[0], thing[1] etc. etc.
	int persistenceIdSize;
	// imposes a limit of 256 (including null terminator) chars in a string
	// nothing else should ever get above (or honestly anywhere near) that,
	// since array/struct members get different PersistentVar
	int rawDataSize;
};

class PersistentVar
{
public:
	PersistentVar(PersistentVarHeader* header)
	{
		hdr           = header;
		type          = (char*)header + sizeof(PersistentVarHeader);
		persistenceId = (char*)header + sizeof(PersistentVarHeader) + header->typeSize;
		rawData       = (char*)header + sizeof(PersistentVarHeader) + header->typeSize + header->persistenceIdSize;
	}

	size_t Size()
	{
		return sizeof(PersistentVarHeader) + hdr->typeSize + hdr->persistenceIdSize + hdr->rawDataSize;
	}

	std::string GetPersistenceString()
	{
		return persistenceId;
	}

	void SetType(std::string newType)
	{
		type = newType;
	}

private:
	PersistentVarHeader* hdr;
	std::string type;
	std::string persistenceId;
	std::string rawData;	
};

// this should be completely redone, and just serve as an intermediary that loads data into the PersistentVarInterfaces
class NSPersistenceManager
{
public:
	bool LoadFromStream(std::ifstream* stream);

	bool HasVar(const char* persistenceString);

private:
	const char* ReadSourceFromSourceId(int index);

	std::vector<char> m_data;
	std::vector<int*> m_sourceOffsets;
	std::vector<char*> m_sources;
	std::vector<PersistentVar> m_vars;
};

////////////////////////////////////////////////////////////////

// the interface that is used to query and set a persistent var
class PersistentVarInterface
{
public:
	virtual bool CanRetrieveAsInt() = 0;
	virtual std::string GetType() = 0;
	virtual bool SetValue(const char* buf, size_t bufSize) = 0; // maybe not the best way, i could use a string perhaps and parse? seems bad tho
	virtual std::vector<std::string> GetContexts() { return m_contexts; };
	virtual std::string GetValueContext() { return m_valueContext; };
	virtual std::string GetIdentifier() { return m_identifier; };

protected:
	std::vector<std::string> m_contexts;
	std::string m_valueContext = "";
	std::string m_identifier;
};

class PersistentVarInt : PersistentVarInterface
{
public:
	PersistentVarInt(std::string identifier, int value);

	bool CanRetrieveAsInt() override { return true; };
	std::string GetType() override { return "int"; };
	bool SetValue(const char* buf, size_t bufSize) override;

private:
	int m_value;
};

class PersistentVarFloat : PersistentVarInterface
{
public:
	PersistentVarFloat(std::string identifier, float value);

	bool CanRetrieveAsInt() override { return false; };
	std::string GetType() override { return "float"; };
	bool SetValue(const char* buf, size_t bufSize) override;

private:
	float m_value;
};

class PersistentVarBool : PersistentVarInterface
{
public:
	PersistentVarBool(std::string identifier, bool value);

	bool CanRetrieveAsInt() override { return true; };
	std::string GetType() override { return "bool"; };
	bool SetValue(const char* buf, size_t bufSize) override;

private:
	bool m_value;
};

class PersistentVarString : PersistentVarInterface
{
public:
	PersistentVarString(std::string identifier, std::string value);

	bool CanRetrieveAsInt() override { return false; };
	std::string GetType() override { return "string"; };
	bool SetValue(const char* buf, size_t bufSize) override;

private:
	std::string m_value;
};

class PersistentVarEnum : PersistentVarInterface
{
public:
	PersistentVarEnum(std::string enumName, std::string identifier, std::string value);

	bool CanRetrieveAsInt() override { return true; };
	std::string GetType() override { return m_enumName; };
	bool SetValue(const char* buf, size_t bufSize) override;

private:
	bool SetValue(std::string value);
	std::string m_enumName;
	std::string m_value;
};

////////////////////////////////////////////////////////////////

struct PersistentVarDef
{
	std::string type;
	std::string identifier;
	std::string arraySize;
};

struct PersistentStructDef
{
	std::string name;
	std::vector<PersistentVarDef> vars;
};

struct PersistentEnumMember
{
	// enum members track the context that they belong to
	std::string context;
	std::string value;
};

typedef std::vector<PersistentEnumMember> PersistentEnumDef;

struct ParseResult
{
	bool success = false;
	std::string error;
	std::string line;
	size_t offset = 0;
};

struct PersistentContext
{
	// stores the different struct defs that can be used
	std::vector<PersistentStructDef> structDefs;
	// stores var defs
	std::vector<PersistentVarDef> varDefs;
};

// handles loading pdiffs, querying things like array counts, enum indices, etc.
class NSPersistenceDefinitionManager
{
public:
	bool LoadPdiff(fs::path filePath, std::string context);
	bool EnumExists(std::string name); // enums are shared across all contexts
	bool StructExists(std::string context, std::string name);
	void PrintPdiffs();
	void Clear();
	bool EnumContainsMember(std::string enumName, std::string memberName);
	std::string GetEnumMemberContext(std::string enumName, std::string memberName);
	void SetupVars();

private:
	void LogParseResult(ParseResult& result);

	bool TryParseEnum(std::string context, std::stringstream& enumStream, PersistentEnumDef* out);
	ParseResult ParseEnum(std::string context, std::stringstream& enumStream, PersistentEnumDef* out);

	bool TryParseStruct(std::string context, std::stringstream& structStream, PersistentStructDef* out);
	ParseResult ParseStruct(std::string context, std::stringstream& structStream, PersistentStructDef* out);

	bool TryParseVar(std::string context, std::string line, PersistentVarDef* out);
	ParseResult ParseVar(std::string context, std::string line, PersistentVarDef* out);

	bool IsValidType(std::string context, std::string type);
	bool IsValidIdentifier(std::string identifier);
	bool IsValidArraySize(std::string arraySize);

	void HandleVarArray(std::string context, std::string prefix, PersistentVarDef& def);
	void HandleStructChildren(std::string context, std::string prefix, PersistentVarDef& def);
	//void AddVar(std::string type, std::string identifier);

	static constexpr const char* m_whitespaceChars = " \n\r\t\f\v";
	// basically alphanumeric and _ but i may want to add more to this later so constexpr
	static constexpr const char* m_tokenChars = "AaBbCcDdEeFfGgHhIiJjKkLlMmNnOoPpQqRrSsTtUuVvWwXxYyZz0123456789_";

	std::map<std::string, PersistentContext> m_contexts;
	// stores the different enum defs from all contexts (enums are shared)
	std::map<std::string, PersistentEnumDef> m_enumDefs;

	std::vector<PersistentVarInterface*> m_vars;
	
};

NSPersistenceManager* g_persistenceManager;
NSPersistenceDefinitionManager* g_persistenceDefinitionManager;
