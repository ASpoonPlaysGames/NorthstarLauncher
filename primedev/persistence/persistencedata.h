#include "persistencedef.h"

namespace ModdedPersistence
{
	const int MAX_STRING_LENGTH = 128;
	const int MAX_ENUM_MEMBER_LENGTH = 56;

	class Variable
	{

	};

	class BoolVar : Variable
	{
		bool value;
	};

	class IntVar : Variable
	{
		int value;
	};

	class FloatVar : Variable
	{
		float value;
	};

	class StringVar : Variable
	{
		unsigned char length;
		char value[MAX_STRING_LENGTH];
	};

	class EnumVar : Variable
	{
		unsigned char length;
		char value[MAX_ENUM_MEMBER_LENGTH];
	};
}
