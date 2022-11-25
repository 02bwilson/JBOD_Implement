#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "cache.h"
#include "jbod.h"

static cache_entry_t *cache = NULL;
static int cache_size = 0;
static int num_queries = 0;
static int num_hits = 0;
static int num_insert = 0; 

int cache_create(int num_entries) {
  // Catch invalid paramaters 
  if(cache != NULL || num_entries > 4096 || num_entries < 2){ return -1; }
  cache_size = num_entries; 
  num_insert = 0;
  cache = malloc(cache_size * sizeof(cache_entry_t)); 
  for(int i = 0; i < cache_size; i++){
    (cache+i)->valid = false;
    (cache+i)->num_accesses = 0;
    (cache+i)->block_num = -1;
    (cache+i)->disk_num = -1;
  }
  return 1; 
}

int cache_destroy(void) {
  // If cache is already null. we cant free it 
  if(cache == NULL){ return -1; }
  // Free the memory and set to null so no dangling pointer 
  free(cache); 
  cache = NULL; 
  // Reset cache size
  cache_size = 0; 
  return 1;
}

int cache_lookup(int disk_num, int block_num, uint8_t *buf) {
  // Check invalid paramaters 
  if(cache == NULL || cache_size == 0 || num_insert == 0 || buf == NULL ){ return -1; }
  num_queries++;
  // Find the block were looking for
  for(int i = 0; i < cache_size; i++){

    cache_entry_t *ce = cache+i;
    
    if(ce->disk_num == disk_num && ce->block_num == block_num){
      num_hits++;
      ce->num_accesses += 1;
      // Copy to buffer
      memcpy(buf, ce->block, 256);
      return 1;
    }
  }
  return -1;
}

void cache_update(int disk_num, int block_num, const uint8_t *buf) {
  // Look for entry and copy data
  for(int i = 0; i < cache_size; i++){
    if((cache+i)->disk_num == disk_num && (cache+i)->block_num == block_num){
      memcpy((cache+i)->block, buf, 256);
    }
  }
}

int cache_insert(int disk_num, int block_num, const uint8_t *buf) {
  // Check invald paramaters
  if(cache == NULL || buf == NULL || disk_num > 16 || disk_num < 0 || block_num < 0|| block_num > 256){ return -1; }
  // Temp memory to check if entry is same as old one
  u_int8_t tb[256];
  if(cache_lookup(disk_num, block_num, tb) == 1){
    for(int i = 0; i < 256; i++){
      // Check if there is any change in memory
      if(buf[i] != tb[i]){
        cache_update(disk_num, block_num, buf);
        return 1;
      }
    }
    
    return -1;
  }

  // If no more spots, evict least used cache item. 
  else if (num_insert == cache_size -1){
    cache_entry_t *lfu = malloc(sizeof(cache_entry_t));
    lfu->num_accesses = 9999999;
    for(int i = 0; i < cache_size; i++){
      if(lfu->num_accesses > (cache+i)->num_accesses){
        lfu = cache+i;
      }
    }
    memcpy(lfu->block, buf, 256);
    lfu->num_accesses = 1;
    lfu->block_num = block_num;
    lfu->disk_num = disk_num;
    return 1;
  }
  else{
    // If there is space, find next available spot
    int j = 0;
    while((cache+j)->valid == true){
      j++;
    }
    cache_entry_t *ce = cache+j;
    memcpy(ce->block, buf, 256);
    ce->num_accesses = 1;
    ce->block_num = block_num;
    ce->disk_num = disk_num;
    ce->valid = true;
    num_insert++;
    return 1;
  }
}

bool cache_enabled(void) {
	return cache != NULL && cache_size > 0;
}

void cache_print_hit_rate(void) {
	fprintf(stderr, "num_hits: %d, num_queries: %d\n", num_hits, num_queries);
	fprintf(stderr, "Hit rate: %5.1f%%\n", 100 * (float) num_hits / num_queries);
}
