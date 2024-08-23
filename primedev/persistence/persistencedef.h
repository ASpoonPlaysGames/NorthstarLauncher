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
			VarDef(
				const std::string& type,
				const int nativeArraySize,
				const std::string& identifier,
				const std::string& arraySize,
				const std::string& owner);
			const std::string& ToString() const
			{
				return m_printableString;
			}
			const char* GetIdentifier() const
			{
				return m_identifier.c_str();
			}
			const char* GetOwner() const
			{
				return m_owner.c_str();
			}
			const char* GetType() const
			{
				return m_type.c_str();
			}
			const char* GetArraySize() const
			{
				return m_arraySize.c_str();
			}
			const int GetNativeArraySize() const
			{
				return m_nativeArraySize;
			}

		private:
			std::string m_type;
			int m_nativeArraySize = -1; // used for strings e.g. string{16}
			std::string m_identifier;
			std::string m_arraySize; // can be either a number or an enum identifier

			std::string m_owner;

			std::string m_printableString;
		};

		class TypeDef
		{
		public:
			const char* GetIdentifier() const
			{
				return m_identifier.c_str();
			}
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
			int GetMemberCount()
			{
				return m_members.size();
			}
			std::map<size_t, ParseDefinitions::VarDef>& GetMembers()
			{
				return m_members;
			}
			const std::map<size_t, ParseDefinitions::VarDef>& GetMembers() const
			{
				return m_members;
			}

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
			int GetMemberCount() const
			{
				return m_members.size();
			}
			int GetMemberIndex(const char* identifier) const;
			const char* GetMemberName(int index) const;
			const char* GetMemberOwner(int index) const;
			void AddMember(EnumMember member);

		private:
			std::vector<EnumMember> m_members;
		};
	} // namespace ParseDefinitions

	// flattened variable
	class PersistentVarDefinition
	{
	public:
		PersistentVarDefinition(VarType type, std::string identifier, std::vector<std::string>& dependencies);
		PersistentVarDefinition(VarType type, std::string identifier, std::vector<std::string>& dependencies, const ParseDefinitions::EnumDef* enumType);
		VarType GetType() const
		{
			return m_type;
		}
		const char* GetIdentifier() const
		{
			return m_identifier.c_str();
		}
		int GetStringSize() const
		{
			return m_stringSize;
		}
		void SetStringSize(int size) { m_stringSize = size; }
		bool IsVanillaDef() const
		{
			return m_dependencies.empty();
		}

	private:
		VarType m_type = VarType::INVALID;
		std::string m_identifier;
		const ParseDefinitions::EnumDef* m_enumType;
		std::vector<std::string> m_dependencies;
		int m_stringSize;

		friend class PersistentVar;
	};

	// interface for accessing persistent var definitions
	class PersistentVarDefinitionData
	{
	public:
		static PersistentVarDefinitionData* GetInstance();
		void LogVarDefinitions();

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

		// Gets a var definition with the given identifier, returns nullptr if no such var exists
		ParseDefinitions::VarDef* GetVarDefinition(const char* identifier);
		// Registers a var definition, returns nullptr if a var or type with the same identifier exists
		ParseDefinitions::VarDef* RegisterVarDefinition(ParseDefinitions::VarDef varDef);

		const std::map<size_t, PersistentVarDefinition>& GetFlattenedVars() const { return m_persistentVarDefs; }

	private:
		PersistentVarDefinitionData() = default;

		bool ParsePersistence(std::stringstream& stream, const char* owningModName = "");
		bool ParseEnumMember(const std::string& line, const char* owningModName, ParseDefinitions::EnumDef& parentEnum);
		bool ParseVarDefinition(const std::string& line, const char* owningModName, std::map<size_t, ParseDefinitions::VarDef>& targetMap);

		void FlattenVariables();
		void GatherVariables(
			const std::map<size_t, ParseDefinitions::VarDef>& sourceVarDefs,
			std::map<size_t, PersistentVarDefinition>& targetVarDefs,
			std::string idPrefix,
			std::vector<std::string> dependentMods,
			std::vector<size_t> structStack);

		bool m_finalised = false;
		// stores the current modded pdef, reloaded on map change
		std::map<size_t, PersistentVarDefinition> m_persistentVarDefs;
		// todo: change this to be a vector of the definitions, alongside a map of hashes to pointers/references
		// then on flattening, make aliases for enum arrays (they can be indexed by string or by integer)

		// all currently known persistence types (except basic types e.g int)
		std::map<size_t, std::shared_ptr<ParseDefinitions::TypeDef>> m_types;
		// all currently known persistent var defs (not flattened, raw parsing)
		std::map<size_t, ParseDefinitions::VarDef> m_vars;

		friend class PersistenceDataInstance;
	};
} // namespace ModdedPersistence
