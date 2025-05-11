#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <emscripten.h>

#define SV32 1
#define PAGE_SIZE 4096

typedef struct {
    uint64_t ppn;
    uint8_t flags; // V|R|W|X|U|G|A|D
} PTE;

typedef struct {
    PTE* entries;
    size_t num_entries;
} PageTable;

typedef struct {
    PageTable* root_pt;
    uint8_t mode;
} MemorySystem;

static MemorySystem g_mem;

// Physical memory simulation
static uint8_t* phys_memory = NULL;
static size_t phys_mem_size = 0;

EMSCRIPTEN_KEEPALIVE
const char* get_log_buffer() {
    return log_buffer;
}

EMSCRIPTEN_KEEPALIVE
void clear_log_buffer() {
    log_position = 0;
    log_buffer[0] = '\0';
}

EMSCRIPTEN_KEEPALIVE
void init_physical_memory(size_t size_mb) {
    phys_mem_size = size_mb * 1024 * 1024;
    phys_memory = (uint8_t*)calloc(phys_mem_size, 1);
}

EMSCRIPTEN_KEEPALIVE
void init_memory_system(uint8_t mode) {
    g_mem.mode = mode;
    
    // Allocate root page table in physical memory
    size_t pt_size = (mode == SV32) ? 1024 * sizeof(PTE) : 512 * sizeof(PTE);
    uint64_t root_ppn = 0x1000; // Arbitrary physical page number
    g_mem.root_pt = (PageTable*)(phys_memory + (root_ppn << 12));
    g_mem.root_pt->num_entries = (mode == SV32) ? 1024 : 512;
    g_mem.root_pt->entries = (PTE*)(phys_memory + (root_ppn << 12) + sizeof(PageTable));
}

EMSCRIPTEN_KEEPALIVE
void demo_setup() {
    init_physical_memory(16); // 16MB physical memory
    init_memory_system(SV32);

    // Create valid mapping: VA 0x40000000 → PA 0x10000000
    PTE entry = {
        .ppn = 0x10000, // 0x10000000 >> 12
        .flags = 0x01 | 0x02 // Valid + Readable
    };
    g_mem.root_pt->entries[0x100] = entry;
}

EMSCRIPTEN_KEEPALIVE
uint64_t sv32_translate(uint32_t va) {
    if (!phys_memory) return 0xFFFFFFFFFFFFFFFF;
    
    printf("Translating VA: 0x%08x\n", va);
    
    uint32_t vpn1 = (va >> 22) & 0x3FF;
    uint32_t vpn0 = (va >> 12) & 0x3FF;
    
    if (vpn1 >= g_mem.root_pt->num_entries) {
        printf("VPN1 out of range\n");
        return 0xFFFFFFFFFFFFFFFF;
    }

    PTE pte1 = g_mem.root_pt->entries[vpn1];
    printf("Level1 PTE: ppn=0x%05lx flags=0x%02x\n", pte1.ppn, pte1.flags);

    if (!(pte1.flags & 0x01)) {
        printf("Level1 PTE invalid\n");
        return 0xFFFFFFFFFFFFFFFF;
    }

    if (pte1.flags & 0x0E) { // Leaf PTE
        printf("Superpage mapping\n");
        return (pte1.ppn << 12) | (va & 0xFFF);
    }

    // Level0 table
    uint64_t level0_addr = (pte1.ppn << 12);
    if (level0_addr + sizeof(PTE) >= phys_mem_size) {
        printf("Invalid level0 table address\n");
        return 0xFFFFFFFFFFFFFFFF;
    }

    PTE* level0 = (PTE*)(phys_memory + level0_addr);
    if (vpn0 >= g_mem.root_pt->num_entries) {
        printf("VPN0 out of range\n");
        return 0xFFFFFFFFFFFFFFFF;
    }

    PTE pte0 = level0[vpn0];
    printf("Level0 PTE: ppn=0x%05lx flags=0x%02x\n", pte0.ppn, pte0.flags);

    if (!(pte0.flags & 0x01)) {
        printf("Level0 PTE invalid\n");
        return 0xFFFFFFFFFFFFFFFF;
    }

    return (pte0.ppn << 12) | (va & 0xFFF);
}
