#ifndef __UTILS_H
#define __UTILS_H

typedef enum VkResult VkResult;

#define STRINGIFY(s) STR(s)
#define STR(s) #s

#define VK_EXECUTE_REQUIRE_SUCCESS(cmd) { \
	VkResult __result__ = (cmd);\
	if (__result__ != VK_SUCCESS) {\
		PrintResult(__result__);\
		exit(1);\
	}\
}

void PrintResult(VkResult result);

#endif//__UTILS_H
