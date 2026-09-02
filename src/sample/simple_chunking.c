#include "base/blake3.h"
#include "base/common.h"
#include "base/ds.h"
#include "base/os.h"
#include "base/pool.h"

#include "sample/cdc.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef MIN_CHUNK_LEN
#define MIN_CHUNK_LEN Kilobytes(8)
#endif

#ifndef AVG_CHUNK_LEN
#define AVG_CHUNK_LEN Kilobytes(16)
#endif

#ifndef CHUNK_HASH_LEN
#define CHUNK_HASH_LEN (BLAKE3_OUT_LEN)
#endif

#define scratch_begin() pool_scratch_begin(_scr_pool)
#define scratch_end(scratch) pool_scratch_end(scratch)

typedef struct SC_ChunkHash SC_ChunkHash;
struct SC_ChunkHash
{
  u8 *hash;
  u64 chunk_off;
  u64 chunk_len;
};

typedef struct SC_FileManifest SC_FileManifest;
struct SC_FileManifest
{
  u64 hash_len;
  u64 chunk_hash_count;
  SC_ChunkHash *chunk_hashes;
};

typedef struct SC_File SC_File;
struct SC_File
{
  SC_File *next;
  const char *path;
  u8 *data;
  u64 data_len;
  SC_FileManifest manifest;
};

int main(int argc, char **argv)
{
  if (argc < 2)
  {
    fprintf(stderr, "Usage: %s FILE...\n", argv[0]);
    return 1;
  }

  int err = 0;
  if ((err = os_init()) != 0)
  {
    return err;
  }

  CDC_ChunkConfig chunk_cfg;
  if ((err = cdc_chunk_cfg_init(MIN_CHUNK_LEN, AVG_CHUNK_LEN, 2, &chunk_cfg)) != 0)
  {
    fprintf(stderr, "Failed to compute FastCDC masks for average chunk size %i\n", MIN_CHUNK_LEN);
    return err;
  }

  Pool *pool = pool_alloc(Gigabytes(16));
  Pool *_scr_pool = pool_alloc(Megabytes(64));

  // Read all files into memory
  SC_File *first_file = 0;
  SC_File *last_file = 0;
  for (int i = 1; i < argc; ++i)
  {
    struct stat sb = {0};
    lstat(argv[i], &sb);

    if ((sb.st_mode & S_IFREG) == 0)
    {
      printf("skipping %s, not regular file\n", argv[i]);
      continue;
    }

    u8 *data = pool_push(pool, sb.st_size);
    memset(data, 0, sb.st_size);

    int fd = open(argv[i], O_RDONLY);
    u64 bytes_read = 0;
    for (; bytes_read < sb.st_size;)
    {
      int nread = pread(fd, data + bytes_read, sb.st_size - bytes_read, bytes_read);
      if (nread >= 0)
      {
        bytes_read += nread;
      }
      else if (errno != EINTR)
      {
        break;
      }
    }
    close(fd);

    if (bytes_read == sb.st_size)
    {
      SC_File *n = pool_push(pool, sizeof(SC_File));
      {
        n->data = data;
        n->data_len = bytes_read;
        n->path = argv[i];
        n->manifest.hash_len = CHUNK_HASH_LEN;
      }
      QueuePush(first_file, last_file, n);
    }
    else
    {
      printf("failed to read file %s (%lu/%lu bytes read)\n", argv[i], bytes_read, sb.st_size);
    }
  }

  // Generate file manifests (chunk and compute chunk hashes)
  Scratch scratch = scratch_begin();
  for (SC_File *f = first_file; f != 0; f = f->next)
  {
    typedef struct HashNode HashNode;
    struct HashNode
    {
      HashNode *next;
      SC_ChunkHash chunk_hash;
    };
    HashNode *first_hash_node = 0;
    HashNode *last_hash_node = 0;
    u64 hash_node_count = 0;

    CDC_ChunkIter iter;
    cdc_chunk_iter_begin(&chunk_cfg, f->data, f->data_len, &iter);
    for (CDC_Chunk chunk; cdc_chunk_iter_next(&iter, &chunk);)
    {
      u8 *hash = pool_push(pool, f->manifest.hash_len);

      blake3_hasher hasher;
      blake3_hasher_init(&hasher);
      blake3_hasher_update(&hasher, &f->data[chunk.off], chunk.len);
      blake3_hasher_finalize(&hasher, hash, f->manifest.hash_len);

      HashNode *n = pool_push(scratch.pool, sizeof(HashNode));
      {
        memset(n, 0, sizeof(HashNode));
        n->chunk_hash.chunk_off = chunk.off;
        n->chunk_hash.chunk_len = chunk.len;
        n->chunk_hash.hash = hash;
      }
      QueuePush(first_hash_node, last_hash_node, n);
      ++hash_node_count;
    }
    cdc_chunk_iter_end(&iter);

    if (hash_node_count > 0)
    {
      f->manifest.chunk_hashes = pool_push(pool, sizeof(SC_ChunkHash) * hash_node_count);
      for (HashNode *n = first_hash_node; n != 0; n = n->next)
      {
        memcpy(&f->manifest.chunk_hashes[f->manifest.chunk_hash_count], &n->chunk_hash, sizeof(SC_ChunkHash));
        ++f->manifest.chunk_hash_count;
      }
      printf("%lu hashes for file %s\n", f->manifest.chunk_hash_count, f->path);
    }
  }
  scratch_end(scratch);

  // Fill a hash table to track unique data chunks 
  typedef struct ChunkTableEntry ChunkTableEntry;
  struct ChunkTableEntry
  {
    ChunkTableEntry *next;
    u8 *hash;
    u64 hash_len;
    u64 chunk_off;
    u64 chunk_len;
    u64 dupe_count;
  };
  typedef struct ChunkTableSlot ChunkTableSlot;
  struct ChunkTableSlot
  {
    ChunkTableEntry *first;
    ChunkTableEntry *last;
  };
  
  u64 chunk_table_slots = 512;
  ChunkTableSlot *chunk_table = pool_push(pool, chunk_table_slots * sizeof(ChunkTableSlot));
  memset(chunk_table, 0, chunk_table_slots * sizeof(ChunkTableSlot));

  for (SC_File *f = first_file; f != 0; f = f->next)
  {
    for (u64 i = 0; i < f->manifest.chunk_hash_count; ++i)
    {
      SC_ChunkHash *chunk_hash = &f->manifest.chunk_hashes[i];
      u64 slot_hash = *(u64 *)chunk_hash->hash; // NOTE: `CHUNK_HASH_LEN` really should be > 8 bytes!
      u64 slot_idx = slot_hash % chunk_table_slots;
      ChunkTableSlot *slot = &chunk_table[slot_idx];
      ChunkTableEntry *entry = 0;
      for (ChunkTableEntry *n = slot->first; n != 0; n = n->next)
      {
        if (n->hash_len == f->manifest.hash_len && memcmp(n->hash, chunk_hash->hash, n->hash_len) == 0)
        {
          entry = n;
          break;
        }
      }
      if (entry == 0)
      {
        entry = pool_push(pool, sizeof(ChunkTableEntry));
        {
          entry->hash_len = f->manifest.hash_len;
          entry->hash = pool_push(pool, entry->hash_len);
          memcpy(entry->hash, chunk_hash->hash, entry->hash_len);
          entry->chunk_off = chunk_hash->chunk_off;
          entry->chunk_len = chunk_hash->chunk_len;
          entry->dupe_count = 0;
        }
        QueuePush(slot->first, slot->last, entry);
      }
      else
      {
        ++entry->dupe_count;
      }
    }
  }

  // Identify saved data for an imaginary sync
  u64 total_duplicate_chunks = 0;
  u64 total_unique_chunks = 0;
  u64 duplicate_data_len = 0;
  for (u64 i = 0; i < chunk_table_slots; ++i)
  {
    ChunkTableSlot *slot = &chunk_table[i];
    for (ChunkTableEntry *n = slot->first; n != 0; n = n->next)
    {
      if (n->dupe_count > 0)
      {
        total_duplicate_chunks += n->dupe_count;
        duplicate_data_len += n->chunk_len * n->dupe_count;
      }
      else
      {
        ++total_unique_chunks;
      }
    }
  }

  printf("%lu unique chunks\n", total_unique_chunks);
  printf("%lu duplicate chunks, saving up to %lu bytes (%lu KiB)\n", total_duplicate_chunks, duplicate_data_len,
         duplicate_data_len >> 10);

  return 0;
}