#include "logging/logging.h"
#include "logging/crashhandler.h"
#include "core/memalloc.h"
#include "core/vanilla.h"
#include "config/profile.h"
#include "plugins/plugins.h"
#include "plugins/pluginmanager.h"
#include "util/version.h"
#include "util/wininfo.h"
#include "squirrel/squirrel.h"
#include "server/serverpresence.h"

#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"
#include "rapidjson/error/en.h"

#include <string.h>
#include <filesystem>
#include <util/utils.h>

BOOL APIENTRY DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved)
{
	switch (dwReason)
	{
	case DLL_PROCESS_ATTACH:
		g_NorthstarModule = hModule;
		break;
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
		break;
	case DLL_PROCESS_DETACH:
		SpdLog_Shutdown();
		break;
	}

	return TRUE;
}

bool InitialiseNorthstar()
{
	static bool bInitialised = false;
	if (bInitialised)
		return false;

	bInitialised = true;

	// Initilaze working profile directory
	InitialiseNorthstarPrefix();

	// Initilaze log directory
	g_svLogDirectory = fmt::format("{:s}\\logs", GetNorthstarPrefix());

	// Checks if we can write into install directory
	SpdLog_PreInit();

	// Make sure we have a console window
	Console_Init();

	// Init logging
	SpdLog_Init();
	SpdLog_CreateLoggers();

	g_pCrashHandler = new CCrashHandler();
	bool bAllFatal = strstr(GetCommandLineA(), "-crash_handle_all") != NULL;
	g_pCrashHandler->SetAllFatal(bAllFatal);

	// determine if we are in vanilla-compatibility mode
	g_pVanillaCompatibility = new VanillaCompatibility();
	g_pVanillaCompatibility->SetVanillaCompatibility(strstr(GetCommandLineA(), "-vanilla") != NULL);

	// Write launcher version to log
	Sys_PrintOSVer();

	InitialiseVersion();

	InstallInitialHooks();

	g_pServerPresence = new ServerPresenceManager();

	g_pPluginManager = new PluginManager();
	g_pPluginManager->LoadPlugins();

	InitialiseSquirrelManagers();

	// Fix some users' failure to connect to respawn datacenters
	SetEnvironmentVariableA("OPENSSL_ia32cap", "~0x200000200000000");

	curl_global_init_mem(CURL_GLOBAL_DEFAULT, _malloc_base, _free_base, _realloc_base, _strdup_base, _calloc_base);

	// run callbacks for any libraries that are already loaded by now
	CallAllPendingDLLLoadCallbacks();

	return true;
}
