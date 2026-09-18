#include "PCH.h"
#include "settings.h"
#include <SimpleIni.h>
#include <SKSEMenuFramework.h>

using namespace SKSE;
using namespace SKSE::log;
using namespace SKSE::stl;

static bool ini_bool(CSimpleIniA& ini, const char* section, const char* key, bool def) {
    return ini.GetLongValue(section, key, def ? 1L : 0L) != 0;
}

static float ini_float(CSimpleIniA& ini, const char* section, const char* key, float def) {
    return static_cast<float>(ini.GetDoubleValue(section, key, def));
}

namespace settings {
    
}