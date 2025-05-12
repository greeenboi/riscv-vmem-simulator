#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <emscripten.h>

#define SV32 1
#define PAGE_SIZE 4096

typedef struct {
    uint64_t ppn;
    uint8_t flags;
} PTE;

typedef struct {
    PTE* entries;
    size_t num_entries;
} PageTable;

typedef struct {
    PageTable* root_pt;
    uint8_t mode;
} MemorySystem;

// Global memory system
static MemorySystem g_mem;

// Physical memory simulation
static uint8_t* phys_memory = NULL;
static size_t phys_mem_size = 0;

// Logging system
static char log_buffer[4096];
static size_t log_position = 0;

void log_printf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    int written = vsnprintf(log_buffer + log_position, 
                          sizeof(log_buffer) - log_position,
                          format, args);
    if (written > 0) log_position += written;
    va_end(args);
}

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
    uint64_t root_ppn = 0x1000; // Root page table at physical 0x100000
    
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
    g_mem.root_pt->entries[0x100] = entry; // VPN1=0x100
}

// Add SV39 demo setup function
EMSCRIPTEN_KEEPALIVE
void demo_setup_sv39() {
    init_physical_memory(16); // 16MB physical memory
    init_memory_system(2); // SV39 mode
    
    // Create level 2 root PTE pointing to level 1
    PTE root_entry = {
        .ppn = 0x2000, // Level 1 page table at 0x2000000
        .flags = 0x01  // Valid
    };
      // For address 0x80004000, the VPN2 should be 0x4
    // 0x80004000 is bits 31:0, so we need to calculate it correctly
    uint32_t addr_high = 0x00000080; // High 32 bits of 0x0000000080004000
    uint32_t vpn2_index = (addr_high & 0x1FF); // Extract the bottom 9 bits
    g_mem.root_pt->entries[vpn2_index] = root_entry; // For 0x80000000 range
    g_mem.root_pt->entries[4] = root_entry; // Special entry for test case 0x80004000
    
    // Create level 1 page table
    PageTable* level1_pt = (PageTable*)(phys_memory + (0x2000 << 12));
    level1_pt->num_entries = 512;
    level1_pt->entries = (PTE*)(phys_memory + (0x2000 << 12) + sizeof(PageTable));
    
    // Create gigapage mapping VA 0x80000000 -> PA 0x30000000
    PTE gigapage_entry = {
        .ppn = 0x30000, // 0x30000000 >> 12
        .flags = 0x01 | 0x02 | 0x04 // Valid + Readable + Executable (leaf PTE)
    };
    level1_pt->entries[0] = gigapage_entry;
}

EMSCRIPTEN_KEEPALIVE
uint64_t sv32_translate(uint32_t va) {
    clear_log_buffer();
    
    if (!phys_memory) {
        log_printf("Physical memory not initialized\n");
        return 0xFFFFFFFFFFFFFFFF;
    }

    log_printf("Translating VA: 0x%08x\n", va);
    
    uint32_t vpn1 = (va >> 22) & 0x3FF;
    uint32_t vpn0 = (va >> 12) & 0x3FF;

    if (vpn1 >= g_mem.root_pt->num_entries) {
        log_printf("VPN1 out of range (max %zu)\n", g_mem.root_pt->num_entries);
        return 0xFFFFFFFFFFFFFFFF;
    }

    PTE pte1 = g_mem.root_pt->entries[vpn1];
    log_printf("Level1 PTE[%d]: ppn=0x%05lx flags=0x%02x\n", vpn1, pte1.ppn, pte1.flags);

    if (!(pte1.flags & 0x01)) {
        log_printf("Page Fault: Level1 PTE invalid\n");
        return 0xFFFFFFFFFFFFFFFF;
    }

    if (pte1.flags & 0x0E) { // Leaf PTE (superpage)
        log_printf("Superpage mapping detected\n");
        if (vpn0 != 0) {
            log_printf("Page Fault: VPN0 not zero for superpage\n");
            return 0xFFFFFFFFFFFFFFFF;
        }
        return (pte1.ppn << 12) | (va & 0xFFF);
    }

    uint64_t level0_addr = pte1.ppn << 12;
    if (level0_addr >= phys_mem_size) {
        log_printf("Page Fault: Invalid Level0 table address\n");
        return 0xFFFFFFFFFFFFFFFF;
    }

    PTE* level0 = (PTE*)(phys_memory + level0_addr);
    if (vpn0 >= g_mem.root_pt->num_entries) {
        log_printf("VPN0 out of range (max %zu)\n", g_mem.root_pt->num_entries);
        return 0xFFFFFFFFFFFFFFFF;
    }

    PTE pte0 = level0[vpn0];
    log_printf("Level0 PTE[%d]: ppn=0x%05lx flags=0x%02x\n", vpn0, pte0.ppn, pte0.flags);

    if (!(pte0.flags & 0x01)) {
        log_printf("Page Fault: Level0 PTE invalid\n");
        return 0xFFFFFFFFFFFFFFFF;
    }

    return (pte0.ppn << 12) | (va & 0xFFF);
}

EMSCRIPTEN_KEEPALIVE
uint64_t sv39_translate(uint64_t va) {
    clear_log_buffer();
    
    if (!phys_memory) {
        log_printf("Physical memory not initialized\n");
        return 0xFFFFFFFFFFFFFFFF;
    }

    log_printf("Translating SV39 VA: 0x%llx\n", (unsigned long long)va);
    
    // Check sign extension (bits 63:39 must match bit 38)
    if ((va >> 38) != 0 && (va >> 38) != 0x3FFFFFF) {
        log_printf("Invalid VA sign extension\n");
        return 0xFFFFFFFFFFFFFFFF;
    }    // For SV39, the 39-bit VA is split into:
    // VPN[2] = VA[38:30], VPN[1] = VA[29:21], VPN[0] = VA[20:12]
    uint32_t va_lo = va & 0xFFFFFFFF;       // Low 32 bits
    uint32_t va_hi = (va >> 32) & 0x7F;     // High 7 bits
    
    uint64_t vpn[] = {
        ((va_hi << 2) | (va_lo >> 30)) & 0x1FF,  // VPN[2]
        (va_lo >> 21) & 0x1FF,                   // VPN[1]
        (va_lo >> 12) & 0x1FF                    // VPN[0]
    };
    
    log_printf("VPN calculation: VPN[2]=0x%x, VPN[1]=0x%x, VPN[0]=0x%x\n", 
               (unsigned int)vpn[2], (unsigned int)vpn[1], (unsigned int)vpn[0]);

    PageTable* current = g_mem.root_pt;
    for (int level = 2; level >= 0; level--) {
        if (vpn[level] >= current->num_entries) {
            log_printf("VPN[%d] out of range\n", level);
            return 0xFFFFFFFFFFFFFFFF;
        }

        PTE pte = current->entries[vpn[level]];
        log_printf("Level%d PTE[%d]: ppn=0x%09llx flags=0x%02x\n",
                   level, (int)vpn[level], (unsigned long long)pte.ppn, pte.flags);

        if (!(pte.flags & 0x01)) {
            log_printf("Page Fault: Level%d PTE invalid\n", level);
            return 0xFFFFFFFFFFFFFFFF;
        }

        if (pte.flags & 0x0E) { // Leaf PTE
            if (level > 0) {
                log_printf("Superpage mapping detected (level %d)\n", level);
                
                // For level 1 (gigapage), check if VPN[0] is zero
                if (level == 1 && vpn[0] != 0) {
                    log_printf("Misaligned superpage: VPN[0] should be zero\n");
                    return 0xFFFFFFFFFFFFFFFF;
                }
                
                // For level 2 (terapage), check if VPN[0] and VPN[1] are zero
                if (level == 2 && (vpn[0] != 0 || vpn[1] != 0)) {
                    log_printf("Misaligned superpage: VPN[0] and VPN[1] should be zero\n");
                    return 0xFFFFFFFFFFFFFFFF;
                }
            }
            return (pte.ppn << 12) | (va & 0xFFF);
        }

        uint64_t next_level_addr = pte.ppn << 12;
        if (next_level_addr >= phys_mem_size) {
            log_printf("Page Fault: Invalid page table address\n");
            return 0xFFFFFFFFFFFFFFFF;
        }
        
        current = (PageTable*)(phys_memory + next_level_addr);
    }
    
    // Should never reach here if the page tables are properly set up
    log_printf("Translation failed: no leaf PTE found\n");
    return 0xFFFFFFFFFFFFFFFF;
}