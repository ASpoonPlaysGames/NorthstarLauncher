#pragma once
#include "persistencedef.h"
#include "mods/modmanager.h"
#include "squirrel/squirrel.h"
#include <variant>
#include <map>
#include <string>
#include "engine/r2engine.h"

namespace ModdedPersistence
{
	class PersistenceDataInstance;
	class PersistentVariable;
	class PersistentGroup;

	// enums use string in stored data, but are converted to int for querying
	using PersistentVarTypeVariant = std::variant<bool, int, float, std::string>;
	using StrIdx = unsigned int;
	constexpr char NSPDATA_MAGIC[4] = {'N', 'S', 'P', 'D'};

	namespace ParseDefinitions
	{
		// Raw file structs
#pragma pack(push, 1)
		union DataVariableValue
		{
			bool asBool;
			int asInt;
			float asFloat;
			StrIdx asString;
			StrIdx asEnum;
		};

		struct DataHeader
		{
			unsigned int version; // I don't really see this changing, but it's there i guess
			char magic[4];

			unsigned int stringsOffset;
			unsigned int stringCount;
			unsigned int variablesOffset;
			unsigned int variableCount;
			unsigned int groupsOffset;
			unsigned int groupCount;
		};

		struct DataVariablePossibility
		{
			StrIdx dependency;
			DataVariableValue value;
		};

		struct DataVariableHeader
		{
			VarType type;
			StrIdx name;
			unsigned int possibilityCount;
		};

		struct DataVariable
		{
			VarType type;
			StrIdx name;
			std::vector<DataVariablePossibility> possibilities;
		};

		struct DataGroupMember
		{
			VarType type;
			StrIdx name;
			DataVariableValue value;
		};

		struct DataGroupPossibilityHeader
		{
			unsigned int dependencyCount;
			unsigned int memberCount;
		};

		struct DataGroupPossibility
		{
			std::vector<StrIdx> dependencies;
			std::vector<DataGroupMember> members;
		};

		struct DataGroupHeader
		{
			unsigned int possibilityCount;
		};

		struct DataGroup
		{
			std::vector<DataGroupPossibility> possibilities;
		};

#pragma pack(pop)

	} // namespace ParseDefinitions

	using FlattenedDataVariableValue = std::variant<bool, int, float, std::string>;

	struct FlattenedDataVariable
	{
		std::string m_name; // debug really
		std::string m_group; // not implemented yet
		VarType m_type;
		FlattenedDataVariableValue m_value;
	};

	// A client's entire modded persistence data
	class PersistenceDataInstance
	{
	public:
		PersistenceDataInstance();

		bool ParseFile(std::istream& stream);
		void ProcessData(std::vector<Mod>& loadedMods);

		bool ToStream(std::ostream& stream);
	private:
		// Gets all groups and variables to sort out their possibilities, sorting conflicts etc.
		void CommitChanges();

		// raw file stuff
		std::vector<std::string> m_strings;
		std::vector<ParseDefinitions::DataVariable> m_variables;
		std::vector<ParseDefinitions::DataGroup> m_groups;

		// actual data that we interface with
		std::map<size_t, FlattenedDataVariable> m_flattened;

		bool m_finalised = false;
	};
	// interface for getting and setting persistent variables
	class PersistentVarData
	{
	public:
		static PersistentVarData* GetInstance();
		PersistenceDataInstance* GetDataForPlayer(CBaseClient* player);

		bool AddClient(CBaseClient* client);
		bool LoadData(CBaseClient* client, std::istream& stream);
		bool WriteData(CBaseClient* client, std::ostream& stream);
		void ClearData(CBaseClient* client);
		void ClearAllData();

	private:
		PersistentVarData() = default;

		// key being something related to the client, todo
		std::unordered_map<CBaseClient*, std::shared_ptr<PersistenceDataInstance>> m_persistenceData;
	};

} // namespace ModdedPersistence
