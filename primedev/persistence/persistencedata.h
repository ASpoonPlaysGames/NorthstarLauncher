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
	constexpr char NSPDATA_MAGIC[4] = {'N', 'S', 'P', 'D'};

#pragma pack(push, 1)
	struct PersistenceDataHeader
	{
		unsigned int version; // I don't really see this changing, but it's there i guess
		char magic[4];

		unsigned int dependenciesOffset;
		unsigned int dependencyCount;
		unsigned int variablesOffset;
		unsigned int variableCount;
		unsigned int groupsOffset;
		unsigned int groupCount;
		unsigned int identifiersOffset;
		unsigned int identifiersCount;
	};
#pragma pack(pop)

	class PersistencePossibility
	{
		// gets a bit array containing the required mod dependencies
		virtual std::vector<bool> GetDependencies() = 0;
	};

	class PersistentVariablePossibility : public PersistencePossibility
	{
	public:
		static bool FromStream(std::istream& stream, PersistentVariablePossibility& out);
		bool ToStream(std::ostream& stream);

		std::vector<bool> GetDependencies() override;

	private:
		// mod dependency for enum value vars
		int m_dependency;
		VarType m_type;
		// todo: could this be a union?
		PersistentVarTypeVariant m_value;

		friend class PersistentVariable;
	};

	class PersistentVariable
	{
	public:
		static bool FromStream(std::istream& stream, PersistentVariable& out);
		bool ToStream(std::ostream& stream);

		// adds a possibility, or replaces one if they have matching dependencies
		void AddPossibility(PersistentVariablePossibility& possibility);
		// selects the best possibility based on the enabled dependencies
		// note: prefers the "most specific" possibility (the one with the most dependencies)
		PersistentVariablePossibility& SelectPossibility();

	private:
		// should be a reference to a member of m_identifiers in PersistenceDataInstance
		std::string& m_identifier;
		std::vector<PersistentVariablePossibility> m_possibilities;

		PersistentVariablePossibility& m_selectedPossibility;
	};

	// A group possibility is a collection of variable possibilities that must be loaded all together.
	// The group is dependent on all of the variables' dependencies.
	// A group is defined in pdef as a file scope struct instance.
	class PersistentGroupPossibility : public PersistencePossibility
	{
	public:
		static bool FromStream(std::istream& stream, PersistentGroupPossibility& out);
		bool ToStream(std::ostream& stream);

		std::vector<bool> GetDependencies() override;

	private:
		std::vector<PersistentVariablePossibility> m_members;

		friend class PersistentGroup;
	};

	class PersistentGroup
	{
	public:
		static bool FromStream(std::istream& stream, PersistentGroup& out);
		bool ToStream(std::ostream& stream);

		// adds a possibility, or replaces one if they have matching dependencies
		void AddPossibility(PersistentGroupPossibility& possibility);
		// selects the best possibility based on the enabled dependencies
		// note: prefers the "most specific" possibility (the one with the most dependencies)
		PersistentGroupPossibility& SelectPossibility();

	private:
		// identifier of the instance, not the type identifier
		// should be a reference to a member of m_identifiers in PersistenceDataInstance
		std::string& m_identifier;
		std::vector<PersistentGroupPossibility> m_possibilities;

		PersistentGroupPossibility& m_selectedPossibility;
	};

	// A client's entire modded persistence data
	class PersistenceDataInstance
	{
	public:
		// Gets the index of a mod dependency, adding it if it didn't exist
		int GetDependencyIndex(const char* dependency);

		static bool FromStream(std::istream& stream, PersistenceDataInstance& out);
		bool ToStream(std::ostream& stream);

		// Uses the enabled mod names to select persistent variables
		void Finalise(std::vector<std::string> loadedModNames);

	private:
		// Gets all groups and variables to sort out their possibilities, sorting conflicts etc.
		void CommitChanges();

		std::vector<std::string> m_dependencies;
		std::vector<PersistentVariable> m_variables;
		std::vector<PersistentGroup> m_groups;
		std::vector<std::string> m_identifiers;

		bool m_finalised = false;
		// selected possibilities after finalising, key being the hash of the flattened identifier
		std::map<size_t, PersistentVariablePossibility&> m_selectedVariables;
		// bitfield of m_dependencies, showing which ones are currently enabled
		std::vector<bool> m_enabledDependencies;
	};

	// defines a fully-formed persistent datum for a player along with an interface for accessing
	// the datum and pushing to the sqvm
	// class PersistentVar
	//{
	// public:
	//	PersistentVar(PersistentVarDefinition* def, PersistentVarTypeVariant val);
	//	VarType GetType() { return m_definition->m_type; }
	//	int GetAsInteger();

	// private:
	//	PersistentVarTypeVariant m_value;
	//	PersistentVarDefinition* m_definition;
	// };

	// interface for getting and setting persistent variables
	class PersistentVarData
	{
	public:
		static PersistentVarData* GetInstance();

		// todo: delete
		// PersistentVar& GetVar(void* player, const char* name);
		// bool PersistenceAvailable(void* player);

		// loads persistence for the given player
		bool Load(void* player, void* data);
		// clears all loaded persistence data for the given player
		void Clear(void* player);
		// clears all loaded persistence data
		void ClearAll();

	private:
		PersistentVarData() = default;

		// std::map<void*, std::map<size_t, PersistentVar>> m_persistentVars;

		// key being something related to the client, todo
		std::unordered_map<void*, std::shared_ptr<PersistenceDataInstance>> m_persistenceData;
	};
} // namespace ModdedPersistence
