#include "persistencehooks.h"
#include "persistenceapi.h"
#include "squirrel/squirrel.h"

REPLACE_SQCLASSFUNC(GetPersistentVarAsInt, CPlayer, ScriptContext::SERVER)
{
	auto& sq = *g_pSquirrel<context>;

	const SQChar* argString = sq.getstring(sqvm, 1);
	spdlog::info("SERVER GetPersistentVarAsInt called: '{}'", argString);

	void* player = nullptr;
	sq.getthisentity(sqvm, &player);

	// find var in modded pdef
	// if not found, return vanilla function result
	// if found, check if var can be got as an int
	// if valid, get as int and push to squirrel
	// if not valid, error

	return sq.m_funcOriginals["CPlayer.GetPersistentVarAsInt"](sqvm);
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
	spdlog::info("SERVER SetPersistentVar called: '{}' '{}'", sq.getstring(sqvm, 1));

	void* player = nullptr;
	sq.getthisentity(sqvm, &player);

	// find var in modded pdef
	// if not found, call vanilla function and return
	// if found, check var typing against argument (how?)
	// sqvm->_stackOfCurrentFunction[argIndex] to get sqObject
	// write into argument pointer

	return sq.m_funcOriginals["CPlayer.SetPersistentVar"](sqvm);
}
