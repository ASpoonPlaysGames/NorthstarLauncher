#ifndef IPLUGIN_MODLOADER_H
#define IPLUGIN_MODLOADER_H

#define PLUGIN_MODLOADER_VERSION "PluginModLoader001"

class IPluginModLoader
{
public:
	virtual uint32_t LoadMods() = 0; // load all mod manifests and return the count
	virtual const char* GetModManifest(uint32_t index) = 0; // get a mod's manifest by index
	// todo: get special load func by string?
};

#endif
