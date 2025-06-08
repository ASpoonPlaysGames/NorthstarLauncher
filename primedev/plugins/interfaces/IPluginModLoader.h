#ifndef IPLUGIN_MODLOADER_H
#define IPLUGIN_MODLOADER_H

#define PLUGIN_MODLOADER_VERSION "PluginModLoader001"

class IPluginModLoader
{
public:
	virtual uint32_t RefreshModPaths() = 0; // collects all possible mod locations, returning the count
	virtual const char* GetModDirectory(uint32_t index) = 0; // gets a mod location by index
};

#endif
