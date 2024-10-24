#ifndef MEMORY_H
#define MEMORY_H

#include "stdint.h"
#include "stdbool.h"
#include "stdlib.h"
#include "time.h"

#define DATA_BASE 0x10000
#define MEMORY_SIZE 0x50000 + 1 // Also used as end from which stack grows downward
#define VALID (uint8_t) 0b1000
#define DIRTY (uint8_t) 0b0100


typedef enum ReplacementPolicy {
    FIFO,
    LRU,
    RANDOM
} ReplacementPolicy;

typedef enum WritePolicy {
    WriteThrough,
    WriteBack
} WritePolicy;

typedef struct CacheConfig {
    uint64_t n_lines;
    uint64_t block_size;
    uint64_t associativity;
    ReplacementPolicy replacement_policy;
    WritePolicy write_policy;
    bool write_allocate;
    bool has_cache;
} CacheConfig;

typedef struct CacheStats {
    uint64_t access_count;
    uint64_t hit_count;
    uint64_t miss_count;
    uint64_t writebacks;
    long double hit_rate;
} CacheStats;

typedef struct CacheMasks {
    uint64_t offset;
    uint64_t index;
    uint64_t tag;
    time_t timestamp_offset;
    size_t block_offset;
    size_t data_offset;
} CacheMasks;

typedef struct Memory {
    CacheConfig cache_config;
    CacheStats cache_stats;
    CacheMasks masks;
    uint8_t* cache;
    uint8_t data[MEMORY_SIZE]; 
} Memory;

// Cache line be like:
// Valid bit - Dirty Bit - Tag Bits - Timestamps?

Memory* new_vmem(CacheConfig cache_config);

uint8_t read_data_byte(Memory* mem, uint64_t addr);

void write_data_byte(Memory* mem, uint64_t addr, uint8_t data);

void free_vmem(Memory* Memory);

#endif