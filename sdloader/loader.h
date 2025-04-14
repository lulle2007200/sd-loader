#ifndef _LOADER_H
#define _LOADER_H

#include <utils/types.h>

void reloc_and_start_payload(void *addr, u32 size);

#endif