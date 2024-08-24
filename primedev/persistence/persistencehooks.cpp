#include "persistencehooks.h"
#include "persistencedata.h"
#include "persistencedef.h"
#include "squirrel/squirrel.h"

using namespace ModdedPersistence;

REPLACE_SQCLASSFUNC(GetPersistentVarAsInt, CPlayer, ScriptContext::SERVER)
{
	auto& varData = *PersistentVarData::GetInstance();
	auto& varDefData = *PersistentVarDefinitionData::GetInstance();
	auto& sq = *g_pSquirrel<context>;

	const SQChar* argString = sq.getstring(sqvm, 1);
	const size_t argStringHash = STR_HASH(argString);

	spdlog::info("SERVER GetPersistentVar called: '{}'", argString);

	CBasePlayer* player = nullptr;
	sq.getthisentity(sqvm, &player); // does this actually get the right type?

	//auto* playerData = varData.GetDataForPlayer(player);
	//if (playerData == nullptr)
	//	spdlog::error("no modded data found for player {}", player->m_nPlayerIndex);

	// find var in modded pdef
	PersistentVarDefinition* varDef = varDefData.FindVarDefinition(argString);
	// todo: nuke this logic, eventually we should *only* use modded persistence systems so that we handle proper ownership of enum index values
	// if not found, return vanilla function result, let vanilla persistence handle this
	// addendum: fall back on vanilla only if we dont have a value in modded + it exists in pdef as a pure vanilla def
	//if (varDef == nullptr)
	//	return sq.m_funcOriginals["CPlayer.GetPersistentVarAsInt"](sqvm);
	if (varDef == nullptr)
		spdlog::error("Couldn't find def {}?", argString);

	// todo: if type is enum check modded first, before getting vanilla as a fallback
	if (varDef == nullptr || (varDef->IsVanillaDef() && varDef->GetType() != VarType::ENUM))
		return sq.m_funcOriginals["CPlayer.GetPersistentVarAsInt"](sqvm);

	// if found, check if var can be got as an int
	//if (IsValidAsInteger(varDef->GetType()))
	//{
	//	// if valid, get as int and push to squirrel
	//	PersistentVar& varValue = varData.GetVar(player, argString);
	//	sq.pushinteger(sqvm, varValue.GetAsInteger());
	//	return SQRESULT_NOTNULL;
	//}

	spdlog::warn("Invalid modded var to retrieve as int '{}'", argString);
	return SQRESULT_NULL;
	
}

REPLACE_SQCLASSFUNC(GetPersistentVar, CPlayer, ScriptContext::SERVER)
{
	auto& sq = *g_pSquirrel<context>;

	const SQChar* argString = sq.getstring(sqvm, 1);
	spdlog::info("SERVER GetPersistentVar called: '{}'", argString);

	void* player = nullptr;
	sq.getthisentity(sqvm, &player);

	// find var in modded pdef
	// if not found, return vanilla function result
	// if found, push to squirrel

	return sq.m_funcOriginals["CPlayer.GetPersistentVar"](sqvm);
}

REPLACE_SQCLASSFUNC(SetPersistentVar, CPlayer, ScriptContext::SERVER)
{
	auto& sq = *g_pSquirrel<context>;

	const SQChar* argString = sq.getstring(sqvm, 1);
	spdlog::info("SERVER SetPersistentVar called: '{}'", sq.getstring(sqvm, 1));

	void* player = nullptr;
	sq.getthisentity(sqvm, &player);

	// find var in modded pdef
	// if not found, call vanilla function and return
	// if found, check var typing against argument (how?)
	// sqvm->_stackOfCurrentFunction[argIndex] to get sqObject
	// write into argument pointer

	return sq.m_funcOriginals["CPlayer.SetPersistentVar"](sqvm);
}
