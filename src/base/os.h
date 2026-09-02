#ifndef BASE_OS_H
#define BASE_OS_H

#include "base/common.h"

int os_init();

u64 os_page_size();

void *os_reserve(u64 size);
void os_commit(void *addr, u64 size);
void os_release(void *addr, u64 size);

#endif // BASE_OS_H