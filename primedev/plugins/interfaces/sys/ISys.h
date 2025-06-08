#ifndef ILOGGING_H
#define ILOGGING_H

#define SYS_VERSION "NSSys002"

enum class LogLevel : int
{
	INFO = 0,
	WARN,
	ERR,
};

class ISys
{
public:
	virtual void Log(HMODULE handle, LogLevel level, char* msg) = 0;
	virtual void Unload(HMODULE handle) = 0;
	virtual void Reload(HMODULE handle) = 0;
	// Gets the profile prefix (e.g. R2Northstar). It is recommended for plugins to make a copy of the string if storing it.
	virtual const char* GetProfilePrefix() = 0;
};

#endif
