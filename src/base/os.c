#include "base/os.h"

#include <sys/mman.h>
#include <unistd.h>

typedef struct OS_State OS_State;
struct OS_State
{
  u64 page_size;
};

static OS_State os;

int os_init()
{
  os.page_size = sysconf(_SC_PAGE_SIZE);
  return 0;
}

u64 os_page_size()
{
  return os.page_size;
}

void *os_reserve(u64 size)
{
  return mmap(0, size, PROT_NONE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
}

void os_commit(void *addr, u64 size)
{
  mprotect(addr, size, PROT_READ | PROT_WRITE);
}

void os_release(void *addr, u64 size)
{
  munmap(addr, 0);
}