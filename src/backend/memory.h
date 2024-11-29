#ifndef MEMORY_H
#define MEMORY_H

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 199309L
#endif

#include "stdint.h"
#include "stdbool.h"
#include "stdlib.h"
#include "stdio.h"
#include "time.h"

#define DATA_BASE 0x10000000
#define MEMORY_SIZE 0x50000000 + 1 // Also used as end from which stack grows downward
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

// typedef enum Cache_Byte_State {
//     NORMAL=0,
//     HIT=1,
//     MISS=2,
//     DIRTY_NORMAL=3,
//     DIRTY_HIT=4,
//     DIRTY_MISS=5,
// } Cache_Byte_State;

// typedef struct CacheDebugInfo {
//     size_t last_update;
//     size_t last_update_size;
//     Cache_Byte_State* info_table;
// } CacheDebugInfo;

typedef struct CacheConfig {
    uint64_t n_lines;
    uint64_t n_blocks;
    uint64_t block_size; 
    uint64_t associativity;
    uint64_t tag_shift;
    ReplacementPolicy replacement_policy;
    WritePolicy write_policy;
    FILE* trace_file;
    char trace_file_name[300];
    bool write_allocate;
    bool has_cache;
} CacheConfig;

typedef struct CacheStats {
    uint64_t access_count;
    uint64_t hit_count;
    uint64_t miss_count;
    uint64_t writebacks;
    double hit_rate;
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
    // CacheDebugInfo debug_info;
    uint8_t* cache;
    uint8_t data[MEMORY_SIZE]; 
} Memory;

// Cache line be like:
// Valid bit - Dirty Bit - Tag Bits - Timestamps?

Memory* new_vmem(CacheConfig cache_config);

uint8_t read_data_byte(Memory* mem, uint64_t addr);

uint16_t read_data_halfword(Memory* mem, uint64_t addr);

uint32_t read_data_word(Memory* mem, uint64_t addr);

uint64_t read_data_doubleword(Memory* mem, uint64_t addr);

void reset_cache(Memory* memory);

void write_data_byte(Memory* mem, uint64_t addr, uint8_t data);

void write_data_halfword(Memory* mem, uint64_t addr, uint16_t data);

void write_data_word(Memory* mem, uint64_t addr, uint32_t data);

void write_data_doubleword(Memory* mem, uint64_t addr, uint64_t data);

void free_vmem(Memory* Memory);

void invalidate_cache(Memory* memory);

void dump_cache(Memory* memory, FILE* f); 

CacheConfig read_cache_config(FILE* fp);

#endif