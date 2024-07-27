#include "persistencedef.h"
#include "macros.h"

namespace ModdedPersistence
{
	InvalidVarDefQueryResult::InvalidVarDefQueryResult(const char* name)
		: m_name(name)
	{}

	ValidVarDefQueryResult::ValidVarDefQueryResult(const char* name, const VarType type)
		: m_name(name)
		, m_nameHash(STR_HASH(name))
	{}
}
