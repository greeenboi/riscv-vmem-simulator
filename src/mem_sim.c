#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <emscripten.h>

#define SV32 1
#define SV39 2
#define SV48 3

typedef struct {
    uint64_t ppn;
    uint8_t flags;
} PTE;

typedef struct {
    PTE* entries;
    size_t num_entries;
} PageTable;

typedef struct {
    PageTable* root;
    uint8_t mode;
} MemorySystem;
static MemorySystem g_mem;
EMSCRIPTEN_KEEPALIVE
void init_memory_system(MemorySystem* mem, uint8_t mode) {
    mem->mode = mode;
    mem->root = (PageTable*)malloc(sizeof(PageTable));
    mem->root->num_entries = (mode == SV32) ? 1024 : 512;
    mem->root->entries = (PTE*)calloc(mem->root->num_entries, sizeof(PTE));
}

EMSCRIPTEN_KEEPALIVE
uint64_t sv32_translate(MemorySystem* mem, uint32_t va) {
    if (!g_mem.root || !g_mem.root->entries) {
        printf("Memory system not initialized!\n");
        return 0xFFFFFFFFFFFFFFFF;
    }

    printf("Starting translation for VA: 0x%08x\n", va);
    
    uint32_t vpn1 = (va >> 22) & 0x3FF;
    uint32_t vpn0 = (va >> 12) & 0x3FF;
    printf("VPN[1]: 0x%03x, VPN[0]: 0x%03x\n", vpn1, vpn0);

    if (vpn1 >= mem->root->num_entries) {
        printf("Page Fault: VPN1 out of range\n");
        return 0xFFFFFFFFFFFFFFFF;
    }

    PTE pte1 = mem->root->entries[vpn1];
    printf("Level 1 PTE - PPN: 0x%05lx, Flags: 0x%02x\n", pte1.ppn, pte1.flags);

    if (!(pte1.flags & 0x01)) {
        printf("Page Fault: Level 1 PTE invalid\n");
        return 0xFFFFFFFFFFFFFFFF;
    }

    if (pte1.flags & 0x0E) {  // Leaf PTE check
        printf("Superpage mapping found\n");
        if (vpn0 != 0) {
            printf("Page Fault: Misaligned superpage\n");
            return 0xFFFFFFFFFFFFFFFF;
        }
        return (pte1.ppn << 12) | (va & 0xFFF);
    }

    PageTable* level1 = (PageTable*)(pte1.ppn << 12);
    if (vpn0 >= level1->num_entries) {
        printf("Page Fault: VPN0 out of range\n");
        return 0xFFFFFFFFFFFFFFFF;
    }

    PTE pte0 = level1->entries[vpn0];
    printf("Level 0 PTE - PPN: 0x%05lx, Flags: 0x%02x\n", pte0.ppn, pte0.flags);

    if (!(pte0.flags & 0x01)) {
        printf("Page Fault: Level 0 PTE invalid\n");
        return 0xFFFFFFFFFFFFFFFF;
    }

    return (pte0.ppn << 12) | (va & 0xFFF);
}



EMSCRIPTEN_KEEPALIVE
void demo_setup() {
    init_memory_system(&g_mem, SV32);
    
    // Allocate physical memory for root page table
    uint64_t root_ppn = 0x1000;  // Arbitrary physical address
    g_mem.root = (PageTable*)(root_ppn << 12);
    g_mem.root->entries = (PTE*)calloc(1024, sizeof(PTE));
    
    // Create valid mapping: VA 0x40000000 → PA 0x10000000
    PTE entry;
    entry.ppn = 0x10000;  // 0x10000000 >> 12
    entry.flags = 0x01 | 0x02;  // Valid + Readable
    g_mem.root->entries[0x100] = entry;  // VPN1=0x100
}
