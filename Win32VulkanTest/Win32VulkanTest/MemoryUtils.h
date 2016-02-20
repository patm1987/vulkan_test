
#ifndef __MEMORY_UTILS_H
#define __MEMORY_UTILS_H

#define SAFE_FREE(x) {if((x) != NULL){free((x)); (x) = NULL;}}
#define SAFE_ALLOCATE_ARRAY(type,count) (type*)malloc(sizeof(type)*(count))

#endif//__MEMORY_UTILS_H
