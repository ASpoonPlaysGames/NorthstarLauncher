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
		public:
			std::string m_type;
			int m_nativeArraySize; // used for strings e.g. string{16}
			std::string m_identifier;
			std::string m_arraySize; // can be either a number or an enum identifier

			std::string m_owner;
		};

		class TypeDef
		{
		public:
			const char* GetIdentifier() const { return m_identifier.c_str(); }
			virtual void GatherChildren(VarDef parent) {};

		protected:
			TypeDef(const char* identifier);
		private:
			const std::string m_identifier;
		};

		// information about a parsed struct
		// note: structs don't have owning mods, their members do
		class StructDef : public TypeDef
		{
		public:
			StructDef(const char* identifier);
			int GetMemberCount() { return m_members.size(); }
			std::map<size_t, ParseDefinitions::VarDef>& GetMembers() { return m_members; }

			void AddMember(VarDef member);
		private:
			std::map<size_t, ParseDefinitions::VarDef> m_members;
		};

		struct EnumMember
		{
			std::string identifier;
			std::string owner;
		};

		// information about a parsed enum
		// note: enums don't have owning mods, their members do
		class EnumDef : public TypeDef
		{
		public:
			EnumDef(const char* identifier);
			int GetMemberCount() { return m_members.size(); }
			int GetMemberIndex(const char* identifier);
			const char* GetMemberName(int index);
			const char* GetMemberOwner(int index);
			void AddMember(EnumMember member);
		private:
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

		// Gets a type definition with the given identifier, returns nullptr if no such type exists
		ParseDefinitions::TypeDef* GetTypeDefinition(const char* identifier);
		// Registers a type definition, returns nullptr if a var or type with the same identifier exists
		ParseDefinitions::TypeDef* RegisterTypeDefinition(std::shared_ptr<ParseDefinitions::TypeDef> typeDef);

		// Gets a var definition with the given identifier, returns nullptr if no such type exists
		ParseDefinitions::VarDef* GetVarDefinition(const char* identifier);
		// Registers a var definition, returns nullptr if a var or type with the same identifier exists
		ParseDefinitions::VarDef* RegisterVarDefinition(ParseDefinitions::VarDef varDef);


	private:
		PersistentVarDefinitionData() = default;

		const size_t GetHash(std::string identifier) const { return STR_HASH(identifier); }
		bool ParsePersistence(std::stringstream& stream, const char* owningModName = "");
		bool ParseEnumMember(const std::string& line, const char* owningModName, ParseDefinitions::EnumDef& parentEnum);
		bool ParseVarDefinition(const std::string& line, const char* owningModName, std::map<size_t, ParseDefinitions::VarDef>& targetMap);

		bool m_finalised = false;
		// stores the current modded pdef, reloaded on map change
		std::map<size_t, PersistentVarDefinition> m_persistentVarDefs;

		// all currently known persistence types (except basic types e.g int)
		std::map<size_t, std::shared_ptr<ParseDefinitions::TypeDef>> m_types;
		// all currently known persistent var defs (not flattened, raw parsing)
		std::map<size_t, ParseDefinitions::VarDef> m_vars;
	};
}
