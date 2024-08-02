#pragma once
#include "mods/modmanager.h"
#include <map>
#include <vector>
#include <sstream>

namespace ModdedPersistence
{
	enum class VarType : int
	{
		INT,
		FLOAT,
		BOOL,
		STRING,
		ENUM,

		INVALID = -1
	};

	static bool IsValidAsInteger(const VarType type)
	{
		switch (type)
		{
		case VarType::INT:
		case VarType::BOOL:
		case VarType::ENUM:
			return true;
		case VarType::FLOAT:
		case VarType::STRING:
		case VarType::INVALID:
			return false;
		}
		assert_msg(false, "IsValidAsInteger - Unreachable VarType found?");
		return false;
	}

	namespace ParseDefinitions
	{
		// information about a parsed variable
		class VarDef
		{
			std::string m_identifier;
			std::string m_typeIdentifier;
			std::string m_arraySize; // can be either a number or an enum identifier

			std::string m_owner;
		};

		// information about a parsed struct
		// note: structs don't have owning mods, their members do
		class StructDef
		{
			std::string m_identifier;
			std::vector<VarDef> m_members;
		};

		struct EnumMember
		{
			std::string identifier;
			std::string owner;
		};

		// information about a parsed enum
		// note: enums don't have owning mods, their members do
		class EnumDef
		{
		public:
			const char* GetName() { return m_identifier.c_str(); }
			int GetMemberCount();
			int GetMemberIndex(std::string identifier);
			const char* GetMemberName(int index);
			const char* GetMemberOwner(int index);
			void AddMember(std::string identifier, std::string owner);
		private:
			std::string m_identifier;
			std::vector<EnumMember> m_members;
		};
	}

	// flattened variable
	class PersistentVarDefinition
	{
	public:
		PersistentVarDefinition(VarType type);
		VarType GetType() { return m_type; }
	private:
		VarType m_type = VarType::INVALID;

		friend class PersistentVar;
	};

	// interface for accessing persistent var definitions
	class PersistentVarDefinitionData
	{
	public:
		static PersistentVarDefinitionData* GetInstance();

		PersistentVarDefinition* FindVarDefinition(const char* name);

		// functions for handling parsing files

		// loads base pdef from a stream (marks variables and types as vanilla)
		bool LoadPersistenceBase(std::stringstream& stream);
		// loads pdiff for the given Mod from a stream (marks variables and types as owned by the mod)
		bool LoadPersistenceDiff(Mod& mod, std::stringstream& stream);
		// finalises the loaded persistence, flattens vars, etc.
		// no more changes can be made to the modded persistence definition after this is called
		void Finalise();
		// clears all loaded pdiff (and therefore all loaded modded persistence data)
		void Clear();

		ParseDefinitions::EnumDef& GetEnumDefinition(std::string identifier, bool createIfNotFound = false);


	private:
		PersistentVarDefinitionData() = default;

		bool m_finalised = false;
		// stores the current modded pdef, reloaded on map change
		std::map<size_t, PersistentVarDefinition> m_persistentVarDefs;

		std::vector<ParseDefinitions::EnumDef> m_enums;
	};
}
