#include "persistenceapi.h"

namespace ModdedPersistence
{

static std::string toString(ValueTypeUnion value, PersistentVarType fromType)
	{
	switch (fromType)
	{
	case PersistentVarType_bool:
		return std::to_string(value.asBool);
	case PersistentVarType_float:
		return std::to_string(value.asFloat);
	case PersistentVarType_int:
		return std::to_string(value.asInt);
	default:
		return "";
	}
}

QueryResult GetModdedPersistentVar(void* player, const char* key)
{
	QueryResult result = QueryResult();
	// check that the var exists in pdef
	auto* varDef = g_pDef.GetVar(key);
	if (varDef == nullptr)
	{
		result.valueStr = "Var doesn't exist in pdef";
		return result;
	}

	// get player's pdata
	auto it = g_pData.find(player);
	if (it == g_pData.end())
	{
		result.valueStr = "Couldn't find pdata for player";
		return result;
	}

	// query player's pdata
	auto& playerData = it->second;
	if (varDef->type == PersistentVarType_string || varDef->type == PersistentVarType_enum)
	{
		result.valueStr = playerData.GetStringValue(key);
		if (varDef->type == PersistentVarType_enum)
		{
			int memberIndex = g_pDef.GetEnumMemberIndex(varDef->enumName, key);
			if (memberIndex == -1)
			{
				result.valueStr = "Enum member not present in enum? Code bug?";
				return result;
			}
			result.value.asInt = memberIndex;
		}
	}
	else
	{
		const int* value = playerData.GetValue(key);
		result.value = value == nullptr ? 0 : *value;
		result.valueStr = toString(result.value, varDef->type);
	}

	result.valueType = varDef->type;
	return result;
}

template<>
bool SetModdedPersistentVar(void* player, const char* key, const char* value)
{
	return false;
}

template <typename T>
bool SetModdedPersistentVar(void* player, const char* key, T value)
{
	return false;
}

}
