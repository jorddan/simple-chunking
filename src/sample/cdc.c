#include "sample/cdc.h"

#include "sample/cdc_hash_table.ex.c"

#include <string.h>

static int cdc_compute_masks(u64 avg_chunk_len, u8 norm_level, u64 *mask_sm_out, u64 *mask_lg_out);

int cdc_chunk_cfg_init(u64 min_chunk_len, u64 avg_chunk_len, u8 norm_level, CDC_ChunkConfig *cfg_out)
{
  int err = cdc_compute_masks(avg_chunk_len, norm_level, &cfg_out->mask_sm, &cfg_out->mask_lg);
  cfg_out->min_chunk_len = min_chunk_len;
  cfg_out->avg_chunk_len = avg_chunk_len;
  return err;
}

void cdc_chunk_iter_begin(CDC_ChunkConfig *cfg, u8 *data, u64 data_len, CDC_ChunkIter *iter_out)
{
  memcpy(&iter_out->cfg, cfg, sizeof(CDC_ChunkConfig));
  iter_out->data = data;
  iter_out->data_len = data_len;
  iter_out->gear_hash = 0;
  iter_out->chunk_off = 0;
  iter_out->chunk_len = 0;
}

bool cdc_chunk_iter_next(CDC_ChunkIter *iter, CDC_Chunk *chunk_out)
{
  for (u64 i = iter->chunk_off; i < iter->data_len; ++i, ++iter->chunk_len)
  {
    iter->gear_hash = (iter->gear_hash << 1) + cdc_hash_table[iter->data[i]];

    if (iter->chunk_len < iter->cfg.min_chunk_len)
    {
      continue;
    }

    // Use the small mask (more active bits) until we reach our desired average chunk length
    u64 mask = iter->chunk_len < iter->cfg.avg_chunk_len ? iter->cfg.mask_sm : iter->cfg.mask_lg;

    if ((iter->gear_hash & mask) == 0)
    {
      chunk_out->off = iter->chunk_off;
      chunk_out->len = iter->chunk_len;

      iter->gear_hash = 0;
      iter->chunk_len = 0;
      iter->chunk_off = i + 1;
      return true;
    }
  }
  if (iter->chunk_len > 0)
  {
    chunk_out->off = iter->chunk_off;
    chunk_out->len = iter->chunk_len;

    iter->gear_hash = 0;
    iter->chunk_len = 0;
    iter->chunk_off = iter->data_len;
    return true;
  }
  return false;
}

void cdc_chunk_iter_end(CDC_ChunkIter *iter)
{
}

int cdc_compute_masks(u64 avg_chunk_len, u8 norm_level, u64 *mask_sm_out, u64 *mask_lg_out)
{
  // Creates two 64 bit masks with roughly evenly dispersed active bits as recommended by the FastCDC paper.
  // Average chunk lengths are treated as a power of two.

  int n = 63 - __builtin_clzll(avg_chunk_len);

  if (n - norm_level <= 0 || n + norm_level > 64)
  {
    *mask_sm_out = 0;
    *mask_lg_out = 0;
    return 1;
  }

  u64 n_sm = n + norm_level;
  u64 n_lg = n - norm_level;
  s64 err_sm = 0;
  s64 err_lg = 0;
  u64 mask_sm = 0;
  u64 mask_lg = 0;

  for (int i = 0; i < 64; ++i)
  {
    mask_sm <<= 1;
    mask_lg <<= 1;

    err_sm -= n_sm;
    err_lg -= n_lg;

    if (err_sm < 0)
    {
      mask_sm |= 1;
      err_sm += 64;
    }

    if (err_lg < 0)
    {
      mask_lg |= 1;
      err_lg += 64;
    }
  }

  *mask_sm_out = mask_sm;
  *mask_lg_out = mask_lg;
  return 0;
}