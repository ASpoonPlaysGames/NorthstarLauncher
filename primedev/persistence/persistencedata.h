#pragma once
#include "persistencedef.h"
#include "squirrel/squirrel.h"
#include <variant>
#include <map>
#include <string>

namespace ModdedPersistence
{
	class PersistenceDataInstance;
	class PersistentVariable;
	class PersistentGroup;


	// enums use string in stored data, but are converted to int for querying
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
		PersistentVariablePossibility(PersistentVariable& parent);

		static bool FromStream(std::istream& stream, PersistentVariablePossibility& out);
		bool ToStream(std::ostream& stream);

		std::vector<bool> GetDependencies() override;

	private:
		PersistentVariable& m_parent;
		// mod dependency for enum value vars
		int m_dependencyIndex = 0;
		// todo: could this be a union?
		PersistentVarTypeVariant m_value;

		friend class PersistentVariable;
	};

	class PersistentVariable
	{
	public:
		PersistentVariable(PersistenceDataInstance& parent);

		static bool FromStream(std::istream& stream, PersistentVariable& out);
		bool ToStream(std::ostream& stream);

		// adds a possibility, or replaces one if they have matching dependencies
		void AddPossibility(PersistentVariablePossibility& possibility);
		// selects the best possibility based on the enabled dependencies
		// note: prefers the "most specific" possibility (the one with the most dependencies)
		PersistentVariablePossibility& GetBestPossibility();

	private:
		PersistenceDataInstance& m_parent;

		unsigned int m_identifierIndex = 0;
		VarType m_type = VarType::INVALID;
		std::vector<PersistentVariablePossibility> m_possibilities = {};

		PersistentVariablePossibility* m_selectedPossibility = nullptr;

		friend class PersistentVariablePossibility;
	};

	// A group possibility is a collection of variable possibilities that must be loaded all together.
	// The group is dependent on all of the variables' dependencies.
	// A group is defined in pdef as a file scope struct instance.
	class PersistentGroupPossibility : public PersistencePossibility
	{
	public:
		PersistentGroupPossibility(PersistentGroup& parent);

		static bool FromStream(std::istream& stream, PersistentGroupPossibility& out);
		bool ToStream(std::ostream& stream);

		std::vector<bool> GetDependencies() override;

	private:
		PersistentGroup& m_parent;

		std::vector<PersistentVariablePossibility> m_members;

		friend class PersistentGroup;
	};

	class PersistentGroup
	{
	public:
		PersistentGroup(PersistenceDataInstance& parent);

		static bool FromStream(std::istream& stream, PersistentGroup& out);
		bool ToStream(std::ostream& stream);

		// adds a possibility, or replaces one if they have matching dependencies
		void AddPossibility(PersistentGroupPossibility& possibility);
		// selects the best possibility based on the enabled dependencies
		// note: prefers the "most specific" possibility (the one with the most dependencies)
		PersistentGroupPossibility& GetBestPossibility();

	private:
		PersistenceDataInstance& m_parent;

		// identifier of the instance, not the type identifier
		unsigned int m_identifierIndex = 0;
		std::vector<PersistentGroupPossibility> m_possibilities = {};

		PersistentGroupPossibility* m_selectedPossibility = nullptr;

		friend class PersistentGroupPossibility;
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

		friend class PersistentVariable;
		friend class PersistentVariablePossibility;
		friend class PersistentGroup;
		friend class PersistentGroupPossibility;
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
