#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "cache.h"
#include "jbod.h"

static cache_entry_t *cache = NULL;
static int cache_size = 0;
static int clock = 0;
static int num_queries = 0;
static int num_hits = 0;

int cache_create(int num_entries)
{
  // ensure cache is not null and num entries is within bounds
  if (cache != NULL || num_entries < 2 || num_entries > 4096)
  {
    return -1;
  }
  // use malloc to allocate memory space in the cache using num entries
  cache = malloc(num_entries * sizeof(cache_entry_t));
  // check again if cache is null
  if (cache == NULL)
  {
    return -1;
  }
  for (int i = 0; i < num_entries; i++)
  {
    cache[i].valid = false; // new cache entries are intialized as invalid
  }

  // change cache size to size of num entries
  cache_size = num_entries;
  clock = 0;
  num_queries = 0;
  num_hits = 0;

  return 1;
}

int cache_destroy(void)
{
  // make sure the cache is not null, if it is return -1
  if (cache == NULL)
  {
    return -1;
  }

  // free the memory allocated
  free(cache);
  // reset variables
  cache = NULL;
  cache_size = 0;
  clock = 0;
  num_queries = 0;
  num_hits = 0;

  return 0;
}

int cache_lookup(int disk_num, int block_num, uint8_t *buf)
{
  // make sure buf is not NUll, , if it is return -1 for failure
  if (buf == NULL)
  {
    return -1;
  }
  // increment num_queries
  num_queries++;

  for (int i = 0; i < cache_size; i++)
  {
    if (cache[i].valid && cache[i].disk_num == disk_num && cache[i].block_num == block_num)
    {
      memcpy(buf, cache[i].block, JBOD_BLOCK_SIZE);
      cache[i].clock_accesses = ++clock; // update clock_accesses using clock tick
      num_hits++;                        // update the hit count
      return 1;                          // success
    }
  }
  return -1; // if the conditional is false, return -1 for failure
}

void cache_update(int disk_num, int block_num, const uint8_t *buf)
{
  for (int i = 0; i < cache_size; i++)
  {
    if (cache[i].valid && cache[i].disk_num == disk_num && cache[i].block_num == block_num) // check if entry is valid and matches disk/block nums
    {
      memcpy(cache[i].block, buf, JBOD_BLOCK_SIZE); // if an entry is found that matches, update the cache by copying from src buf
      cache[i].clock_accesses = ++clock;            // update clock_accesses using clock tick
      break;                                        // exit loop after entry is found/updated
    }
  }
}

int cache_insert(int disk_num, int block_num, const uint8_t *buf)
{
  // make sure cache and buf are not NUll, if they are, return -1
  if (cache == NULL || buf == NULL)
  {
    return -1;
  }
  // make sure disk_num and block_num are within the bounds
  if (disk_num < 0 || disk_num >= JBOD_NUM_DISKS ||
      block_num < 0 || block_num >= JBOD_NUM_BLOCKS_PER_DISK)
  {
    return -1;
  }

  // Here we can check for any existing entries, and if an entry already exists, return -1
  for (int i = 0; i < cache_size; i++)
  {
    if (cache[i].valid && cache[i].disk_num == disk_num && cache[i].block_num == block_num)
    {
      return -1;
    }
  }
  // loop to check if the block already exists
  for (int i = 0; i < cache_size; i++)
  {
    if (cache[i].valid && cache[i].disk_num == disk_num && cache[i].block_num == block_num)
    {
      // update the blocks content in the cache if it is found, using memcpy, copy src buf to destination in the cache.
      memcpy(cache[i].block, buf, JBOD_BLOCK_SIZE);
      cache[i].clock_accesses = ++clock; // update clock access time by incrementing clock
      return 0;                          // return 0 for success
    }
  }

  int replace_entry = -1; // cache entry will be replaced using this variable which will be used as an index
  int highest_clock_accesses = -1;
  for (int i = 0; i < cache_size; i++)
  {
    if (!cache[i].valid)
    {
      replace_entry = i; // invalid entry found, replace it
      break;
    }
    else if (cache[i].clock_accesses > highest_clock_accesses)
    {
      // entry with the highest clock value found, replace it based on MRU
      highest_clock_accesses = cache[i].clock_accesses;
      replace_entry = i;
    }
  }

  if (replace_entry != -1)
  {
    cache[replace_entry].valid = true;        // sets the entry to be replaced as valid
    cache[replace_entry].disk_num = disk_num; // update disk and block nums
    cache[replace_entry].block_num = block_num;
    memcpy(cache[replace_entry].block, buf, JBOD_BLOCK_SIZE); // block data copied from buffer into the cache
    cache[replace_entry].clock_accesses = ++clock;
    return 1; // returns 1 for success
  }
  return -1; // return -1 if replace_ientry is -1 (no replacements found)
}

// return true if cache is enabled
bool cache_enabled(void)
{
  return cache != NULL;
}

void cache_print_hit_rate(void)
{
  fprintf(stderr, "num_hits: %d, num_queries: %d\n", num_hits, num_queries);
  fprintf(stderr, "Hit rate: %5.1f%%\n", 100 * (float)num_hits / num_queries);
}

int cache_resize(int new_num_entries)
{
  // check bounds for num entries
  if (new_num_entries < 2 || new_num_entries > 4096)
  {
    return -1;
  }

  // allocate new space for num entries parameter
  cache_entry_t *new_cache = realloc(cache, new_num_entries * sizeof(cache_entry_t));
  if (new_cache == NULL)
  {
    return -1;
  }
  // check num entries to see if greater than current cache size
  if (new_num_entries > cache_size)
  {
    for (int i = cache_size; i < new_num_entries; i++)
    {
      new_cache[i].valid = false; // new cache entries are intialized as invalid
    }
  }
  cache = new_cache;            // change cache size to new cache
  cache_size = new_num_entries; // set the size equal to the size of the num entries parameter
  return 0;                     // return 0 for success
}