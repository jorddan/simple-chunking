#ifndef SAMPLE_CDC_H
#define SAMPLE_CDC_H

#include "base/common.h"

typedef struct CDC_ChunkConfig CDC_ChunkConfig;
struct CDC_ChunkConfig
{
  u64 min_chunk_len;
  u64 avg_chunk_len;
  u64 mask_sm;
  u64 mask_lg;
};

typedef struct CDC_ChunkIter CDC_ChunkIter;
struct CDC_ChunkIter
{
  CDC_ChunkConfig cfg;
  u8 *data;
  u64 data_len;
  u64 gear_hash;
  u64 chunk_off;
  u64 chunk_len;
};

typedef struct CDC_Chunk CDC_Chunk;
struct CDC_Chunk
{
  u64 off;
  u64 len;
};

int  cdc_chunk_cfg_init(u64 min_chunk_len, u64 avg_chunk_len, u8 norm_level, CDC_ChunkConfig *cfg_out);

void cdc_chunk_iter_begin(CDC_ChunkConfig *cfg, u8 *data, u64 data_len, CDC_ChunkIter *iter_out);
bool cdc_chunk_iter_next(CDC_ChunkIter *iter, CDC_Chunk *chunk_out);
void cdc_chunk_iter_end(CDC_ChunkIter *iter);

#endif // SAMPLE_CDC_H