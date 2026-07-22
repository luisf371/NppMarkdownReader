#include "PluginGlobals.h"

HINSTANCE g_moduleInstance = nullptr;

BOOL APIENTRY DllMain(const HMODULE module, const DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_moduleInstance = module;
        DisableThreadLibraryCalls(module);
    }
    return TRUE;
}
