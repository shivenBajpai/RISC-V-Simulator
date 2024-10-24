#include "memory.h"
#include "stdlib.h"
#include "string.h"

Memory* new_vmem(CacheConfig cache_config) {
    Memory* mem = malloc(sizeof(Memory));
    if (!mem) return NULL;

    if (cache_config.has_cache) {
        mem->cache = malloc(sizeof(uint8_t)*mem->cache_config.block_size*mem->cache_config.n_lines);
        if (!mem->cache) return NULL;

        mem->masks.offset = (cache_config.block_size - 1);
        mem->masks.index = (cache_config.n_lines - 1) * cache_config.block_size;
        mem->masks.tag = -1 & !mem->masks.index & mem->masks.offset;
        mem->masks.block_offset = sizeof(uint8_t) + sizeof(uint64_t) + sizeof(uint8_t) * cache_config.block_size + cache_config.replacement_policy == RANDOM?0:sizeof(time_t);
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
    int replacement_subindex = -1;
    
    mem->cache_stats.access_count += 1;

    uint64_t offset = addr & mem->masks.offset;
    uint64_t index = addr & mem->masks.index;
    uint64_t tag = addr & mem->masks.offset;
    
    uint8_t* line_ptr = mem->cache + mem->masks.block_offset * index * mem->cache_config.associativity;

    for (int i=0; i<mem->cache_config.associativity; i++) {
        if (!(line_ptr[i*mem->masks.block_offset] & VALID)) {
            replacement_subindex = i; 
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

    if (replacement_subindex == -1) {
        switch (mem->cache_config.replacement_policy) {
            case RANDOM:
                replacement_subindex = rand() | (mem->cache_config.associativity -1);
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

    line_ptr += replacement_subindex*mem->masks.block_offset;

    if ((*line_ptr & VALID) && (*line_ptr & DIRTY) && mem->cache_config.write_policy == WriteBack) {
        uint64_t ret_addr = *(uint64_t*) (line_ptr+1) | index;
        memcpy(mem->data[ret_addr], line_ptr + mem->masks.data_offset, mem->cache_config.block_size);
        memcpy(line_ptr + mem->masks.data_offset, mem->data[addr&~mem->masks.offset], mem->cache_config.block_size);
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

void free_vmem(Memory* Memory) {
    if (Memory->cache) free(Memory->cache);
    free(Memory);
}