#pragma once
#include <string>

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
		case VarType::FLOAT:
		case VarType::BOOL:
		case VarType::ENUM:
			return true;
		case VarType::STRING:
		case VarType::INVALID:
			return false;
		}
	}

	// interface representing a var definition in pdef
	class VarDefQueryResult
	{
	public:
		virtual const bool IsValidAsInteger() const = 0;
		virtual const VarType GetType() const = 0;
		virtual const char* GetName() const = 0;
	};

	// represents an invalid var
	class InvalidVarDefQueryResult : public VarDefQueryResult
	{
	public: // VarDef implementation
		const bool IsValidAsInteger() const override { return false; }
		const VarType GetType() const override { return VarType::INVALID; }
		const char* GetName() const override { return m_name.c_str(); }

	public:
		InvalidVarDefQueryResult(const char* name);
	private:
		const std::string m_name;
	};

	class ValidVarDefQueryResult : public VarDefQueryResult
	{
	public: // VarDef implementation
		const bool IsValidAsInteger() const override;
		const VarType GetType() const override { return m_type; }
		const char* GetName() const override { return m_name.c_str(); }

	public:
		ValidVarDefQueryResult(const char* name, const VarType type);
	private:
		const std::string m_name;
		const size_t m_nameHash;
		const VarType m_type;
	};
}
