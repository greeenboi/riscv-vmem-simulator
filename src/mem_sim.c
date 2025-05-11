#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define SV32 1
#define SV39 2
#define SV48 3

typedef struct {
    uint64_t ppn;
    uint8_t flags;  // V|R|W|X|U|G|A|D
} PTE;

typedef struct {
    PTE *entries;
    size_t num_entries;
} PageTable;

typedef struct {
    PageTable *root;
    uint8_t mode;
} MemorySystem;

// Initialize memory system
void init_memory_system(MemorySystem *mem, uint8_t mode) {
    mem->mode = mode;
    mem->root = malloc(sizeof(PageTable));
    mem->root->num_entries = (mode == SV32) ? 1024 : 512;
    mem->root->entries = calloc(mem->root->num_entries, sizeof(PTE));
}

// Basic SV32 translation example
uint64_t sv32_translate(MemorySystem *mem, uint32_t va) {
    uint32_t vpn1 = (va >> 22) & 0x3FF;
    uint32_t vpn0 = (va >> 12) & 0x3FF;
    
    PTE pte1 = mem->root->entries[vpn1];
    if (!(pte1.flags & 0x01)) return -1;  // Check valid bit
    
    if (pte1.flags & 0x0E) {  // Leaf PTE (R/W/X)
        return (pte1.ppn << 12) | (va & 0xFFF);
    }
    
    PageTable *level1 = (PageTable*)(pte1.ppn << 12);
    PTE pte0 = level1->entries[vpn0];
    if (!(pte0.flags & 0x01)) return -1;
    
    return (pte0.ppn << 12) | (va & 0xFFF);
}

void demo_setup(MemorySystem *mem) {
    // SV32 example: Map VA 0x40000000 -> PA 0x10000000
    mem->root->entries[0x100] = (PTE){
        .ppn = 0x10000,  // Physical page number
        .flags = 0x01 | 0x02  // Valid + Readable

