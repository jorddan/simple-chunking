#ifndef BASE_POOL_H
#define BASE_POOL_H

#include "base/common.h"

typedef struct Pool Pool;
struct Pool
{
  u64 pos;
  u64 committed;
  u64 reserved;
  u64 commit_granularity;
};

typedef struct Scratch Scratch;
struct Scratch
{
  Pool *pool;
  u64 pos;
};

Pool *pool_alloc(u64 capacity);
void  pool_release(Pool *pool);

void *pool_push(Pool *pool, u64 size);
void  pool_pop(Pool *pool, u64 n);
void  pool_clear(Pool *pool);

Scratch pool_scratch_begin(Pool *pool);
void    pool_scratch_end(Scratch scratch);

#endif // BASE_POOL_H