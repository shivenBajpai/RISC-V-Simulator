#include "memory.h"
#include "stdlib.h"
#include "string.h"
#include "math.h"
// #include "../frontend/frontend.h"
#include "../supervisor/supervisor.h"
#include "../globals.h" 

#define SEC_TO_NS(sec) ((sec)*1000000000)

uint64_t nanos()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    uint64_t ns = SEC_TO_NS((uint64_t)ts.tv_sec) + (uint64_t)ts.tv_nsec;
    return ns;
}

static bool last_acc_hit = false;

Memory* new_vmem(CacheConfig cache_config) {
    Memory* mem = malloc(sizeof(Memory));
    if (!mem) return NULL;

    mem->cache_config = cache_config;
    if (cache_config.has_cache) {
        mem->masks.block_offset = sizeof(uint8_t) + sizeof(uint64_t) + sizeof(uint8_t) * cache_config.block_size + (cache_config.replacement_policy == RANDOM?0:sizeof(time_t));
        mem->cache = calloc(cache_config.n_blocks, mem->masks.block_offset);
        // mem->debug_info.info_table = calloc(cache_config.n_blocks, sizeof(uint8_t));
        if (!mem->cache) return NULL;

        mem->masks.offset = (cache_config.block_size - 1);
        mem->masks.index = (cache_config.n_lines - 1) * cache_config.block_size;
        mem->masks.tag = ~0 & ~mem->masks.index & ~mem->masks.offset;
        mem->masks.data_offset = sizeof(uint8_t) + sizeof(uint64_t);
        mem->masks.timestamp_offset = mem->masks.block_offset - sizeof(uint64_t);

        mem->cache_config.trace_file = fopen(cache_config.trace_file_name, "w");
    } else {
        mem->cache = NULL;
    }

    mem->cache_stats.access_count = 0;
    mem->cache_stats.hit_count = 0;
    mem->cache_stats.miss_count = 0;
    mem->cache_stats.hit_rate = 0;
    mem->cache_stats.writebacks = 0;

    return mem;
}

void reset_cache(Memory* memory) {
    if (memory->cache) memset(memory->cache, 0,memory->cache_config.n_blocks*memory->masks.block_offset);
    memory->cache_stats.access_count =0;
    memory->cache_stats.hit_count = 0;
    memory->cache_stats.miss_count = 0;
    memory->cache_stats.hit_rate = 0;
    memory->cache_stats.writebacks = 0;
}

uint8_t* find_or_replace_data_line(Memory* mem, uint64_t addr, bool allocate, bool read, bool override_dirty) {
    int victim = -1;
    
    mem->cache_stats.access_count += 1;

    uint64_t offset = addr & mem->masks.offset;
    uint64_t index = (addr & mem->masks.index) / mem->cache_config.block_size;
    uint64_t tag = addr & mem->masks.tag;
    
    uint8_t* line_ptr = mem->cache + (mem->masks.block_offset * index * mem->cache_config.associativity);

    for (int i=0; i<mem->cache_config.associativity; i++) {
        if (!(line_ptr[i*mem->masks.block_offset] & VALID)) {
            if (victim == -1) victim = i; 
            continue;
        }
        if (*(uint64_t*)(line_ptr + i*mem->masks.block_offset +1) != tag) continue;

        // Hit!
        mem->cache_stats.hit_count += 1;
        if (mem->cache_config.replacement_policy == LRU) *(line_ptr + i*mem->masks.block_offset + mem->masks.timestamp_offset) = time(NULL);
        fprintf(mem->cache_config.trace_file, "%c: Address: 0x%lx, Set: 0x%lx, Hit, Tag: 0x%lx, %s\n", read?'R':'W', addr, index, tag/mem->cache_config.block_size/mem->cache_config.n_lines, (line_ptr[i*mem->masks.block_offset]&DIRTY)==DIRTY||override_dirty?"Dirty":"Clean");
        return (line_ptr + i*mem->masks.block_offset + mem->masks.data_offset);
    }

    mem->cache_stats.miss_count += 1;
    if (!allocate) {
        fprintf(mem->cache_config.trace_file, "%c: Address: 0x%lx, Set: 0x%lx, Miss, Tag: 0x%lx, %s\n", read?'R':'W', addr, index, tag/mem->cache_config.block_size/mem->cache_config.n_lines, "Clean");
        return NULL;
    }

    if (victim == -1) {
        switch (mem->cache_config.replacement_policy) {
            case RANDOM:
                victim = rand() & (mem->cache_config.associativity -1);
                break;
            
            case FIFO:
            case LRU:
                uint64_t earliest_time = *(line_ptr+mem->masks.timestamp_offset);
                victim = 0;

                for (int i=1; i<mem->cache_config.associativity; i++) {
                    if (line_ptr[i*mem->masks.block_offset + mem->masks.timestamp_offset] < earliest_time) {
                        earliest_time = line_ptr[i*mem->masks.block_offset + mem->masks.timestamp_offset];
                        victim = i;
                    }
                }
                break;                
        }
    }

    line_ptr += victim*mem->masks.block_offset;

    if ((*line_ptr & VALID) && (*line_ptr & DIRTY) && mem->cache_config.write_policy == WriteBack) {
        uint64_t ret_addr = *(uint64_t*) (line_ptr+1) | (addr & mem->masks.index);
        memcpy(mem->data+ret_addr, line_ptr + mem->masks.data_offset, mem->cache_config.block_size);
        memcpy(line_ptr + mem->masks.data_offset, mem->data+(addr&~mem->masks.offset), mem->cache_config.block_size);
        mem->cache_stats.writebacks += 1;
    }

    *line_ptr = VALID;
    *(uint64_t*)(line_ptr+1) = tag;
    memcpy(line_ptr+mem->masks.data_offset, mem->data+(addr&~mem->masks.offset), mem->cache_config.block_size);
    if (mem->cache_config.replacement_policy != RANDOM) *(line_ptr + mem->masks.timestamp_offset) = time(NULL);

    fprintf(mem->cache_config.trace_file, "%c: Address: 0x%lx, Set: 0x%lx, Miss, Tag: 0x%lx, %s\n", read?'R':'W', addr, index, tag/mem->cache_config.block_size/mem->cache_config.n_lines, (*line_ptr&DIRTY)==DIRTY||override_dirty?"Dirty":"Clean");
    return (line_ptr + mem->masks.data_offset);
}

uint8_t read_data_byte(Memory* mem, uint64_t addr) {
    if (!mem->cache) return mem->data[addr];
    

    uint8_t* block_ptr = find_or_replace_data_line(mem, addr, true, true, false);
    
    mem->cache_stats.hit_rate = (double) mem->cache_stats.hit_count/mem->cache_stats.access_count;
    return *(block_ptr + (addr & mem->masks.offset) );
}

uint16_t read_data_halfword(Memory* mem, uint64_t addr) {
    if (!mem->cache) return *(uint16_t*) (mem->data + addr);
    

    uint8_t* block_ptr = find_or_replace_data_line(mem, addr, true, true, false);
    uint64_t block_addr = addr & ~mem->masks.offset;
    uint64_t curr_addr = block_addr;
    uint16_t result = *(block_ptr+ (addr & mem->masks.offset));


    for (int i=1; i<2; i++) {
        curr_addr = (addr+i) & ~mem->masks.offset;
        if (block_addr != curr_addr) {
            block_ptr = find_or_replace_data_line(mem, addr+i, true, true, false);
            block_addr = curr_addr;
        };

        result += ((uint16_t) *(block_ptr+((addr+i) & mem->masks.offset))) << (8*i);
    }

    mem->cache_stats.hit_rate = (double) mem->cache_stats.hit_count/mem->cache_stats.access_count;
    return result;
}

uint32_t read_data_word(Memory* mem, uint64_t addr) {
    if (!mem->cache) return *(uint32_t*) (mem->data + addr);
    
    
    uint8_t* block_ptr = find_or_replace_data_line(mem, addr, true, true, false);
    uint64_t block_addr = addr & ~mem->masks.offset;
    uint64_t curr_addr = block_addr;
    uint32_t result = *(block_ptr + (addr & mem->masks.offset));

    for (int i=1; i<4; i++) {
        curr_addr = (addr+i) & ~mem->masks.offset;
        if (block_addr != curr_addr) {
            block_ptr = find_or_replace_data_line(mem, addr+i, true, true, false);
            block_addr = curr_addr;
        };

        result += ((uint32_t) *(block_ptr+((addr+i) & mem->masks.offset))) << (8*i);
    }

    mem->cache_stats.hit_rate = (double) mem->cache_stats.hit_count/mem->cache_stats.access_count;
    return result;
}

uint64_t read_data_doubleword(Memory* mem, uint64_t addr) {
    if (!mem->cache) return *(uint64_t*) (mem->data + addr);
    
    
    uint8_t* block_ptr = find_or_replace_data_line(mem, addr, true, true, false);
    uint64_t block_addr = addr & ~mem->masks.offset;
    uint64_t curr_addr = block_addr;
    uint64_t result = *(block_ptr + (addr & mem->masks.offset));

    for (int i=1; i<8; i++) {
        curr_addr = (addr+i) & ~mem->masks.offset;
        if (block_addr != curr_addr) {
            block_ptr = find_or_replace_data_line(mem, addr+i, true, true, false);
            block_addr = curr_addr;
        };

        result += (uint64_t) *(block_ptr+((addr+i) & mem->masks.offset)) << (8*i);
    }

    mem->cache_stats.hit_rate = (double) mem->cache_stats.hit_count/mem->cache_stats.access_count;
    return result;
}

void write_data_byte(Memory* mem, uint64_t addr, uint8_t data) {
    if (!mem->cache) {mem->data[addr] = data; return;}
    

    uint8_t* block_ptr = find_or_replace_data_line(mem, addr, mem->cache_config.write_allocate, false, true);

    // switch (mem->cache_config.write_policy) {
    //     case WriteThrough:
    //         mem->data[addr] = data;
    //         mem->cache_stats.writebacks += 1;
    //         if (block_ptr) *(block_ptr + mem->masks.data_offset + (addr & mem->masks.offset)) = data;

    //     case WriteBack:
    //         if (block_ptr) {
    //             *(block_ptr) |= DIRTY;
    //             *(block_ptr + mem->masks.data_offset + (addr & mem->masks.offset)) = data;
    //         } else {
    //             mem->data[addr] = data;
    //             mem->cache_stats.writebacks += 1;
    //         }
    //         break;
    // }

    if (!block_ptr || mem->cache_config.write_policy == WriteThrough) {
        mem->data[addr] = data;
        mem->cache_stats.writebacks += 1;
    }

    if (mem->cache_config.write_policy == WriteBack) *(block_ptr-mem->masks.data_offset) |= DIRTY;

    if (block_ptr) *(block_ptr + (addr & mem->masks.offset)) = data;

    mem->cache_stats.hit_rate = (double) mem->cache_stats.hit_count/mem->cache_stats.access_count;
}

void write_data_halfword(Memory* mem, uint64_t addr, uint16_t data) {
    if (!mem->cache) {*(uint16_t*) (mem->data+addr) = data; return;}
    

    uint8_t* block_ptr = NULL;
    uint64_t block_addr = mem->masks.offset;
    uint64_t curr_addr;

    if (mem->cache_config.write_policy == WriteThrough) mem->cache_stats.writebacks += 1;

    for (int i=0; i<2; i++) {
        curr_addr = (addr+i) & ~mem->masks.offset;
        if (block_addr != curr_addr) {
            block_ptr = find_or_replace_data_line(mem, addr+i, mem->cache_config.write_allocate, false, true);
            block_addr = curr_addr;
            if (mem->cache_config.write_policy == WriteBack) *(block_ptr-mem->masks.data_offset) |= DIRTY;
            if (!block_ptr && mem->cache_config.write_policy != WriteThrough) mem->cache_stats.writebacks += 1;
        };

        if (!block_ptr || mem->cache_config.write_policy == WriteThrough) mem->data[addr+i] = (uint8_t) (data >> (8*i));

        if (block_ptr) *(block_ptr + ((addr+i) & mem->masks.offset)) = (uint8_t) (data >> (8*i));
    }

    mem->cache_stats.hit_rate = (double) mem->cache_stats.hit_count/mem->cache_stats.access_count;
}

void write_data_word(Memory* mem, uint64_t addr, uint32_t data) {
    if (!mem->cache) {*(uint32_t*) (mem->data+addr) = data; return;}
    

    uint8_t* block_ptr = NULL;
    uint64_t block_addr = mem->masks.offset;
    uint64_t curr_addr;

    if (mem->cache_config.write_policy == WriteThrough) mem->cache_stats.writebacks += 1;

    for (int i=0; i<4; i++) {
        curr_addr = (addr+i) & ~mem->masks.offset;
        if (block_addr != curr_addr) {
            block_ptr = find_or_replace_data_line(mem, addr+i, mem->cache_config.write_allocate, false, true);
            block_addr = curr_addr;
            if (mem->cache_config.write_policy == WriteBack) *(block_ptr-mem->masks.data_offset) |= DIRTY;
            if (!block_ptr && mem->cache_config.write_policy != WriteThrough) mem->cache_stats.writebacks += 1;
        };

        if (!block_ptr || mem->cache_config.write_policy == WriteThrough) mem->data[addr+i] = (uint8_t) (data >> (8*i));

        if (block_ptr) *(block_ptr + ((addr+i) & mem->masks.offset)) = (uint8_t) (data >> (8*i));
    }

    mem->cache_stats.hit_rate = (double) mem->cache_stats.hit_count/mem->cache_stats.access_count;
}

void write_data_doubleword(Memory* mem, uint64_t addr, uint64_t data) {
    if (!mem->cache) {*(uint64_t*) (mem->data+addr) = data; return;}
    

    uint8_t* block_ptr = NULL;
    uint64_t block_addr = mem->masks.offset;
    uint64_t curr_addr;

    if (mem->cache_config.write_policy == WriteThrough) mem->cache_stats.writebacks += 1;

    for (int i=0; i<8; i++) {
        curr_addr = (addr+i) & ~mem->masks.offset;
        if (block_addr != curr_addr) {
            block_ptr = find_or_replace_data_line(mem, addr+i, mem->cache_config.write_allocate, false, true);
            block_addr = curr_addr;
            if (mem->cache_config.write_policy == WriteBack) *(block_ptr-mem->masks.data_offset) |= DIRTY;
            if (!block_ptr && mem->cache_config.write_policy != WriteThrough) mem->cache_stats.writebacks += 1;
        };

        if (!block_ptr || mem->cache_config.write_policy == WriteThrough) mem->data[addr+i] = (uint8_t) (data >> (8*i));

        if (block_ptr) *(block_ptr + ((addr+i) & mem->masks.offset)) = (uint8_t) (data >> (8*i));
    }

    mem->cache_stats.hit_rate = (double) mem->cache_stats.hit_count/mem->cache_stats.access_count;
}

void invalidate_cache(Memory* memory) {
    if (!memory->cache_config.has_cache) return;

    for (int i=0; i<memory->cache_config.n_blocks; i++) {
        if (memory->cache_config.write_policy == WriteBack && (memory->cache[i*memory->masks.block_offset] & VALID)) {
            
            memory->cache_stats.writebacks += 1;

            memcpy(memory->data+ *(uint64_t*) (memory->cache+(i*memory->masks.block_offset)+1) +((i/memory->cache_config.associativity)*memory->cache_config.block_size),
             memory->cache+(i*memory->masks.block_offset)+memory->masks.data_offset,
             memory->cache_config.block_size);
        }

        memory->cache[i*memory->masks.block_offset] &= ~VALID;
    }
}

void dump_cache(Memory* memory, FILE* f) {
    if (!memory->cache_config.has_cache) return;
    uint8_t* cache_ptr = memory->cache;

    for (int i=0; i<memory->cache_config.n_blocks; i++) {
        if ((*cache_ptr & VALID)) 
            fprintf(f, "Set: 0x%02lx, Tag: 0x%lx, %s\n", i/memory->cache_config.associativity, (*(uint64_t*) (cache_ptr+1))/memory->cache_config.block_size/memory->cache_config.n_lines, *cache_ptr&DIRTY?"Dirty":"Clean");;
        
        cache_ptr += memory->masks.block_offset;
    };
}

void free_vmem(Memory* Memory) {
    if (Memory->cache) {
        free(Memory->cache);
        fclose(Memory->cache_config.trace_file);
    }
    free(Memory);
}

CacheConfig read_cache_config(FILE* fp) {
    CacheConfig config;
    unsigned long size;
    char r_policy[8], w_policy[8];
    config.has_cache = false;

    if (fscanf(fp, "%lu\n%lu\n%lu\n%8s\n%8s", &size, &config.block_size, &config.associativity, r_policy, w_policy)!= 5) {
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
    
    if (config.n_lines==0 || size <= 8) {
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
    // config.tag_shift = log2(config.n_lines*config.block_size);
    snprintf(config.trace_file_name, 300, "%s.output", active_file);
    return config;
}

// void debug_update(Memory* memory) {
//     for (size_t i=memory->debug_info.last_update; i<memory->debug_info.last_update+memory->debug_info.last_update_size; i++) {
//         switch(memory->debug_info.info_table[i]) {
//             case HIT:
//             case MISS:
//                 memory->debug_info.info_table[i] = NORMAL;
//                 break;

//             case DIRTY_HIT:
//             case DIRTY_MISS:
//                 memory->debug_info.info_table[i] = DIRTY_NORMAL;
//                 break;
//         } 
//     }
// }