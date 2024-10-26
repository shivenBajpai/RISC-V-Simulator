#include "memory.h"
#include "stdlib.h"
#include "string.h"
#include "../frontend/frontend.h"

Memory* new_vmem(CacheConfig cache_config) {
    Memory* mem = malloc(sizeof(Memory));
    if (!mem) return NULL;

    if (cache_config.has_cache) {
        mem->masks.block_offset = sizeof(uint8_t) + sizeof(uint64_t) + sizeof(uint8_t) * cache_config.block_size + (cache_config.replacement_policy == RANDOM?0:sizeof(time_t));
        mem->cache = calloc(cache_config.n_blocks, mem->masks.block_offset);
        if (!mem->cache) return NULL;

        mem->masks.offset = (cache_config.block_size - 1);
        mem->masks.index = (cache_config.n_lines - 1) * cache_config.block_size;
        mem->masks.tag = -1 & !mem->masks.index & mem->masks.offset;
        mem->masks.data_offset = sizeof(uint8_t) + sizeof(uint64_t);
        mem->masks.timestamp_offset = mem->masks.data_offset + sizeof(uint64_t);
    } else {
        mem->cache = NULL;
    }

    mem->cache_config = cache_config;

    mem->cache_stats.access_count = 0;
    mem->cache_stats.hit_count = 0;
    mem->cache_stats.miss_count = 0;
    mem->cache_stats.hit_rate = 0;
    mem->cache_stats.writebacks = 0;

    return mem;
}

uint8_t* find_or_replace(Memory* mem, uint64_t addr, bool allocate) {
    int victim = -1;
    
    mem->cache_stats.access_count += 1;

    uint64_t offset = addr & mem->masks.offset;
    uint64_t index = addr & mem->masks.index;
    uint64_t tag = addr & mem->masks.offset;
    
    uint8_t* line_ptr = mem->cache + mem->masks.block_offset * index * mem->cache_config.associativity;

    for (int i=0; i<mem->cache_config.associativity; i++) {
        if (!(line_ptr[i*mem->masks.block_offset] & VALID)) {
            victim = i; 
            continue;
        }
        if (*(uint64_t*)(line_ptr + i*mem->masks.block_offset +1) != tag) continue;

        // Hit!
        mem->cache_stats.hit_count += 1;
        if (mem->cache_config.replacement_policy == LRU) *(line_ptr + i*mem->masks.block_offset + mem->masks.timestamp_offset) = time(NULL);
        return (line_ptr + i*mem->masks.block_offset + mem->masks.data_offset + offset);
    }

    mem->cache_stats.miss_count += 1;
    if (!allocate) return NULL;

    if (victim == -1) {
        switch (mem->cache_config.replacement_policy) {
            case RANDOM:
                victim = rand() | (mem->cache_config.associativity -1);
                break;
            
            case FIFO:
            case LRU:
                uint64_t earliest_time = *(line_ptr+mem->masks.timestamp_offset);
                int replacement_index = 0;

                for (int i=1; i<mem->cache_config.associativity; i++) {
                    if (line_ptr[i*mem->masks.block_offset + mem->masks.timestamp_offset] < earliest_time) {
                        earliest_time = line_ptr[i*mem->masks.block_offset + mem->masks.timestamp_offset];
                        replacement_index = i;
                    }
                }
                break;                
        }
    }

    line_ptr += victim*mem->masks.block_offset;

    if ((*line_ptr & VALID) && (*line_ptr & DIRTY) && mem->cache_config.write_policy == WriteBack) {
        uint64_t ret_addr = *(uint64_t*) (line_ptr+1) | index;
        memcpy(mem->data+ret_addr, line_ptr + mem->masks.data_offset, mem->cache_config.block_size);
        memcpy(line_ptr + mem->masks.data_offset, mem->data+(addr&~mem->masks.offset), mem->cache_config.block_size);
        mem->cache_stats.writebacks += 1;
    }

    *line_ptr = VALID;
    *(uint64_t*)(line_ptr+1) = tag;
    memcpy(line_ptr+mem->masks.data_offset, mem->data+(addr&~mem->masks.offset), mem->cache_config.block_size);
    *(line_ptr + mem->masks.timestamp_offset) = time(NULL);

    return (line_ptr + mem->masks.data_offset + offset);
}

uint8_t read_data_byte(Memory* mem, uint64_t addr) {
    if (!mem->cache) return mem->data[addr];

    uint8_t* block_ptr = find_or_replace(mem, addr, true);
    
    mem->cache_stats.hit_rate = mem->cache_stats.hit_count/mem->cache_stats.access_count;
    return *(block_ptr + mem->masks.data_offset + (addr & mem->masks.offset) );
}

// uint16_t read_data_halfword(Memory* mem, uint16_t addr) {
//     if (!mem->cache) return *(uint16_t*) (mem->data + addr);

//     uint8_t* block_ptr = find_or_replace(mem, addr, true);

// }

void write_data_byte(Memory* mem, uint64_t addr, uint8_t data) {
    if (!mem->cache) mem->data[addr] = data;

    uint8_t* block_ptr = find_or_replace(mem, addr, mem->cache_config.write_allocate);

    switch (mem->cache_config.write_policy) {
        case WriteThrough:
            mem->data[addr] = data;
            mem->cache_stats.writebacks += 1;
            if (block_ptr) *(block_ptr + mem->masks.data_offset + (addr & mem->masks.offset)) = data;

        case WriteBack:
            if (block_ptr) {
                *(block_ptr) |= DIRTY;
                *(block_ptr + mem->masks.data_offset + (addr & mem->masks.offset)) = data;
            } else {
                mem->data[addr] = data;
                mem->cache_stats.writebacks += 1;
            }
            break;
    }

    mem->cache_stats.hit_rate = mem->cache_stats.hit_count/mem->cache_stats.access_count;
}

void invalidate_cache(Memory* memory) {
    if (!memory->cache_config.has_cache) return;

    for (int i=0; i<memory->cache_config.n_blocks; i++) {
        memory->cache[i*memory->masks.block_offset] &= ~VALID;
    }
}

void dump_cache(Memory* memory, FILE* f) {
    if (!memory->cache_config.has_cache) return;
    uint8_t* cache_ptr = memory->cache;

    for (int i=0; i<memory->cache_config.n_blocks; i++) {
        if (!(*(cache_ptr) & VALID)) return;

        fprintf(f, "Set: 0x%02lx, Tag: 0x%016lx, %s\n", i/memory->cache_config.associativity, *(uint64_t*) (cache_ptr+1), *cache_ptr&DIRTY?"Dirty":"Clean");
        cache_ptr += memory->masks.block_offset;
    };
}

void free_vmem(Memory* Memory) {
    if (Memory->cache) free(Memory->cache);
    free(Memory);
}

CacheConfig read_cache_config(FILE* fp) {
    CacheConfig config;
    unsigned long size;
    char r_policy[8], w_policy[8];
    config.has_cache = false;

    if (fscanf(fp, "Cache Size: %lu\nBlock Size: %lu\nAssociativity: %lu\nReplacement Policy: %8s\nWrite Back Policy: %8s", &size, &config.block_size, &config.associativity, r_policy, w_policy)!= 5) {
        show_error("Failed to parse config!");
        return config;
    }

    if (size & (size-1)) {
        show_error("Cache size is not a power of 2!");
        return config;
    }

    if (config.block_size & (config.block_size-1)) {
        show_error("Block size is not a power of 2!");
        return config;
    }

    if (size % (config.block_size*config.associativity) != 0) {
        show_error("Cache size does not match block size!");
        return config;
    }

    config.n_lines = size/(config.block_size*config.associativity);
    
    if (config.n_lines==0) {
        show_error("Cache size too small!");
        return config;
    }

    if (!strcmp("WB", w_policy)) {
        config.write_policy = WriteBack;
        config.write_allocate = true;
    } else if (!strcmp("WT", w_policy)) {
        config.write_policy = WriteThrough;
        config.write_allocate = false;
    } else {
        show_error("Invalid Write Back Policy!");
        return config;
    }
    
    if (!strcmp("LRU", r_policy)) config.replacement_policy = LRU; 
    else if (!strcmp("FIFO", r_policy)) config.replacement_policy = FIFO;
    else if (!strcmp("RANDOM", r_policy)) config.replacement_policy = RANDOM;
    else {
        show_error("Invalid Replacement Policy!");
        return config;
    }

    config.n_blocks = config.n_lines*config.associativity;
    config.has_cache = true;
    return config;
}