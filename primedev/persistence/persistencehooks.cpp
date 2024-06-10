#include "persistencehooks.h"
#include "persistenceapi.h"

REPLACE_SQCLASSFUNC(GetPersistentVarAsInt, CPlayer, ScriptContext::SERVER)
{
	auto& sq = *g_pSquirrel<context>;

	auto argString = sq.getstring(sqvm, 1);
	spdlog::info("SERVER GetPersistentVarAsInt called: '{}'", argString);

	void* player = nullptr;
	sq.getthisentity(sqvm, &player);

	auto result = ModdedPersistence::GetModdedPersistentVar(player, argString);
	if (!result.success)
	{
		spdlog::warn("Failed to find modded var {}. This is fine if the vanilla one works :)", argString); // remove this eventually when modded systems have been tested thoroughly
		return sq.m_funcOriginals["CPlayer.GetPersistentVarAsInt"](sqvm);
	}

	switch (result.type)
	{
	case ModdedPersistence::VarType_int:
		sq.pushinteger(sqvm, result.value.asInt);
		return SQRESULT_NOTNULL;
	case ModdedPersistence::VarType_bool:
		sq.pushinteger(sqvm, static_cast<int>(result.value.asBool));
		return SQRESULT_NOTNULL;
	case ModdedPersistence::VarType_float:
		sq.pushinteger(sqvm, static_cast<int>(result.value.asFloat));
		return SQRESULT_NOTNULL;
	case ModdedPersistence::VarType_string:
	case ModdedPersistence::VarType_enum:
		sq.raiseerror(sqvm, fmt::format("Invalid var to retrieve as int: '{}'", argString))
		return SQRESULT_ERROR;
	}

	sq.raiseerror(sqvm, fmt::format("Unknown persistent var type. var: {} type: {}", argString, result.type))
	return SQRESULT_ERROR;
}

REPLACE_SQCLASSFUNC(GetPersistentVar, CPlayer, ScriptContext::SERVER)
{
	auto& sq = *g_pSquirrel<context>;

	auto argString = sq.getstring(sqvm, 1);
	spdlog::info("SERVER GetPersistentVar called: '{}'", argString);

	void* player = nullptr;
	sq.getthisentity(sqvm, &player);

	auto result = ModdedPersistence::GetModdedPersistentVar(player, argString);
	if (!result.success)
	{
		spdlog::warn("Failed to find modded var {}. This is fine if the vanilla one works :)", argString; // remove this eventually when modded systems have been tested thoroughly
		return sq.m_funcOriginals["CPlayer.GetPersistentVar"](sqvm);
	}

	switch (result.type)
	{
	case ModdedPersistence::VarType_bool:
		sq.pushbool(sqvm, result.value.asBool);
		return SQRESULT_NOTNULL;
	case ModdedPersistence::VarType_int:
		sq.pushinteger(sqvm, result.value.asInt);
		return SQRESULT_NOTNULL;
	case ModdedPersistence::VarType_float:
		sq.pushfloat(sqvm, result.value.asFloat);
		return SQRESULT_NOTNULL;
	case ModdedPersistence::VarType_string:
	case ModdedPersistence::VarType_enum:
		sq.pushstring(sqvm, result.value.asString);
		return SQRESULT_NOTNULL;
	}

	sq.raiseerror(sqvm, fmt::format("Unknown persistent var type. var: {} type: {}", argString, result.type))
	return SQRESULT_ERROR;
}

REPLACE_SQCLASSFUNC(SetPersistentVar, CPlayer, ScriptContext::SERVER)
{
	auto& sq = *g_pSquirrel<context>;
	spdlog::info("SERVER SetPersistentVar called: '{}' '{}'", sq.getstring(sqvm, 1));


	return sq.m_funcOriginals["CPlayer.SetPersistentVar"](sqvm);
}
