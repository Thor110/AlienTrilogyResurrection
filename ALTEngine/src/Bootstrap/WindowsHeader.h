#pragma once

// <windows.h> without the macros that clobber the engine's own names:
// GDI's DrawText and Rectangle, and min/max (which break std::min, std::max
// and std::clamp). Guarded, since MinGW defines NOMINMAX itself.
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOGDI
#define NOGDI
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
