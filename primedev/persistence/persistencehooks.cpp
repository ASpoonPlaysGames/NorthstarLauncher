#include "persistencehooks.h"
#include "persistencedef.h"
#include "persistencedata.h"
#include "squirrel/squirrel.h"

using namespace ModdedPersistence;

template <ScriptContext context>
static CBaseClient* GetClientEntity(HSQUIRRELVM sqvm)
{
	auto& sq = *g_pSquirrel<context>;

	__int64 unk = 0;
	if (!sq.getthisentity(sqvm, &unk))
	{
		spdlog::error("Couldn't getthisentity?");
		return nullptr;
	}
	// no clue, copied from disassembly
	int index = *(int*)(unk + 0x58) - 1;

	return &g_pClientArray[index];
}

REPLACE_SQCLASSFUNC(GetPersistentVarAsInt, CPlayer, ScriptContext::SERVER)
{
	auto& varData = *PersistentVarData::GetInstance();
	auto& varDefData = *PersistentVarDefinitionData::GetInstance();
	auto& sq = *g_pSquirrel<context>;

	const SQChar* argString = sq.getstring(sqvm, 1);
	const size_t argStringHash = STR_HASH(argString);

	spdlog::info("SERVER GetPersistentVarAsInt called: '{}'", argString);

	CBaseClient* player = GetClientEntity<context>(sqvm);

	auto* playerData = varData.GetDataForPlayer(player);
	if (playerData == nullptr)
		spdlog::error("no modded data found for player {}", player->m_Name);

	// todo: (long term)
	// if we have the var def but no value for it, grab the vanilla value (if one exists) and write it to the modded data
	// that way we slowly move over all of the player's persistence over to the modded system


	// find var in modded pdef
	PersistentVarDefinition* varDef = varDefData.FindVarDefinition(argString);
	// todo: nuke this logic, eventually we should *only* use modded persistence systems so that we handle proper ownership of enum index values
	// if not found, return vanilla function result, let vanilla persistence handle this
	// addendum: fall back on vanilla only if we dont have a value in modded + it exists in pdef as a pure vanilla def
	//if (varDef == nullptr)
	//	return sq.m_funcOriginals["CPlayer.GetPersistentVarAsInt"](sqvm);
	if (varDef == nullptr)
		spdlog::error("Couldn't find def {}?", argString);

	// todo: always prefer a modded value if one exists.
	if (varDef == nullptr || (varDef->IsVanillaDef() && varDef->GetType() != VarType::ENUM))
		return sq.m_funcOriginals["CPlayer.GetPersistentVarAsInt"](sqvm);

	// if found, check if var can be got as an int
	if (IsValidAsInteger(varDef->GetType()))
	{
		// if valid, get as int and push to squirrel
		auto* var = playerData->GetVariable(STR_HASH(varDef->GetIdentifier()));
		if (var == nullptr)
		{
			spdlog::error("UHH WHY CANT I FIND THIS VAR?????? {}", varDef->GetIdentifier());
			return SQRESULT_NULL;
		}
		spdlog::warn("found var: {}", var->m_name);
		sq.pushinteger(sqvm, std::get<int>(var->m_value));
		return SQRESULT_NOTNULL;
	}

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
	auto& varData = *PersistentVarData::GetInstance();
	auto& varDefData = *PersistentVarDefinitionData::GetInstance();
	auto& sq = *g_pSquirrel<context>;

	const SQChar* argString = sq.getstring(sqvm, 1);
	const size_t argStringHash = STR_HASH(argString);

	spdlog::info("SERVER SetPersistentVar called: '{}'", sq.getstring(sqvm, 1));

	CBaseClient* player = GetClientEntity<context>(sqvm);
	auto* playerData = varData.GetDataForPlayer(player);
	if (playerData == nullptr)
		spdlog::error("no modded data found for player {}", player->m_Name);

	// find var in modded pdef
	PersistentVarDefinition* varDef = varDefData.FindVarDefinition(argString);


	// if not found, call vanilla function and return


	// if found, check var typing against argument (how?)
	// sqvm->_stackOfCurrentFunction[argIndex] to get sqObject
	// write into argument pointer

	return sq.m_funcOriginals["CPlayer.SetPersistentVar"](sqvm);

	//spdlog::warn("Invalid modded var to retrieve as int '{}'", argString);
	//return SQRESULT_NULL;
}
