#include "base/pool.h"
#include "base/os.h"

#ifndef POOL_RESERVE_GRANULARITY
#define POOL_RESERVE_GRANULARITY Megabytes(64)
#endif

#ifndef POOL_COMMIT_GRANULARITY
#define POOL_COMMIT_GRANULARITY Kilobytes(64)
#endif

#ifndef POOL_PUSH_GRANULARITY
#define POOL_PUSH_GRANULARITY (16)
#endif

Pool *pool_alloc(u64 size)
{
  u64 page_size = os_page_size();
  u64 reserve_granularity = AlignPow2(POOL_RESERVE_GRANULARITY, page_size);
  u64 commit_granularity = AlignPow2(POOL_COMMIT_GRANULARITY, page_size);
  u64 reserve_size = AlignPow2(size, reserve_granularity);
  u64 initial_commit = AlignPow2(sizeof(Pool), commit_granularity);

  void *addr = os_reserve(reserve_size);
  os_commit(addr, initial_commit);

  Pool *pool = (Pool *)addr;
  {
    pool->pos = sizeof(Pool);
    pool->committed = initial_commit;
    pool->reserved = reserve_size;
    pool->commit_granularity = commit_granularity;
  }
  return pool;
}

void pool_release(Pool *pool)
{
  os_release(pool, pool->reserved);
}

void *pool_push(Pool *pool, u64 size)
{
  u64 cur_pos = AlignPow2(pool->pos, POOL_PUSH_GRANULARITY);
  u64 next_pos = cur_pos + size;

  if (pool->committed < next_pos)
  {
    u64 req_committed = AlignPow2(next_pos, pool->commit_granularity);
    u64 next_committed = Min(req_committed, pool->reserved);
    os_commit((u8 *)pool + pool->committed, next_committed - pool->committed);
    pool->committed = next_committed;
  }

  void *result = 0;
  if (Likely(pool->committed >= next_pos))
  {
    result = (u8 *)pool + cur_pos;
    pool->pos = next_pos;
  }
  return result;
}

void pool_pop(Pool *pool, u64 n)
{
  if (pool->pos >= n)
  {
    pool->pos = Max(pool->pos - n, sizeof(Pool));
  }
}

void pool_clear(Pool *pool)
{
  pool->pos = sizeof(Pool);
}

Scratch pool_scratch_begin(Pool *pool)
{
  Scratch result = {
      .pool = pool,
      .pos = pool->pos,
  };
  return result;
}

void pool_scratch_end(Scratch scratch)
{
  scratch.pool->pos = Max(scratch.pos, sizeof(Pool));
}