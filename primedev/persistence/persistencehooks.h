#include "squirrel/squirrel.h"


// TODO:
// UI
// [ ] GetPersistentVar
// [ ] GetPersistentVarAsInt
//   and more (querying pdef)
// CLIENT (class funcs)
//   same as UI
// SERVER (class funcs)
// [X] GetPersistentVar
// [X] GetPersistentVarAsInt
// [X] SetPersistentVar
//   and more (querying pdef)
//
// CLIENT NETMESSAGE STUFF
// SERVER NETMESSAGE STUFF (if needed)

// personal plan:
// get basic hooks ready, getting and setting. If simple enough, get hooks for pdef queries as well.
// come up with a decent API for this (use clientcommands to set persistence temporarily)
// investigate net messages for communication between server and client

// These hooks are meant to filter down input and feed into the custom pdata system

// it should be noted that some native functions go directly into the interface, so we can't hook them (well we can, but i cba)
// these are fairly benign cases e.g. xp, gen, etc. where we would end up defaulting to vanilla persistence anyway, so not a big deal



// have a map of CBaseEntity* to persistent data for servers to use
// have a single instance of persistent data for local client to use (no splitscreen, womp womp)
// have a single instance of persistence manager (has pdef and helper functions) - loaded from mod files
