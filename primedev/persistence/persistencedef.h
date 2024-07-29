#pragma once
#include "mods/modmanager.h"
#include <map>
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


	private:
		PersistentVarDefinitionData() = default;

		bool m_finalised = false;
		// stores the current modded pdef, reloaded on map change
		std::map<size_t, PersistentVarDefinition> m_persistentVarDefs;
	};
}
