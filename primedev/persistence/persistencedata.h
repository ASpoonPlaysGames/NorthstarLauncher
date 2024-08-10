#pragma once
#include "persistencedef.h"
#include "squirrel/squirrel.h"
#include <variant>
#include <map>
#include <string>

namespace ModdedPersistence
{
	// enums use int
	using PersistentVarTypeVariant = std::variant<bool, int, float, std::string>;

	// defines a fully-formed persistent datum for a player along with an interface for accessing
	// the datum and pushing to the sqvm
	class PersistentVar
	{
	public:
		PersistentVar(PersistentVarDefinition* def, PersistentVarTypeVariant val);
		VarType GetType() { return m_definition->m_type; }
		int GetAsInteger();

	private:
		PersistentVarTypeVariant m_value;
		PersistentVarDefinition* m_definition;
	};

	// interface for getting and setting persistent variables
	class PersistentVarData
	{
	public:
		static PersistentVarData* GetInstance();

		PersistentVar& GetVar(void* player, const char* name);
		bool PersistenceAvailable(void* player);

		// loads persistence for the given player
		bool Load(void* player, void* data);
		// clears all loaded persistence data for the given player
		void Clear(void* player);
		// clears all loaded persistence data
		void ClearAll();

	private:
		PersistentVarData() = default;

		std::map<void*, std::map<size_t, PersistentVar>> m_persistentVars;
	};
}
