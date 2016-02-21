// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers

#define VK_USE_PLATFORM_WIN32_KHR
#define NOMINMAX // remove windows' min() and max()

// Windows Header Files:
#include <windows.h>

// C RunTime Header Files
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>

// TODO: reference additional headers your program requires here
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <vulkan/vulkan.h>
#pragma comment(lib, "vulkan-1.lib")
