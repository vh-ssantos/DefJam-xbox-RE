// Idioma dos textos do port: automático pelo idioma da interface do Windows (português -> PT, o resto -> inglês).
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "common/lang.h"

namespace defjam {

uint32_t g_language = 0;

bool Portuguese() {
    if (g_language == 1) return true;
    if (g_language == 2) return false;
    static const bool windows_pt = PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_PORTUGUESE;
    return windows_pt;
}

}  // namespace defjam
