#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <ctype.h>

#define BACKING_FILE "BACKING_STORE.bin"
#define OUTPUT_FILE "output.txt"
#define PAGE_SIZE   256
#define NUM_PAGES   256
#define NUM_FRAMES  128
#define TLB_SIZE    16

typedef enum {
    POLICY_FIFO,
    POLICY_LRU
} policy_t;

typedef struct {
    int valid;
    int frame;
} page_entry_t;

typedef struct {
    int valid;
    int page;
    int frame;
    unsigned long last_used;
    unsigned long loaded_at;
} tlb_entry_t;

policy_t page_policy, tlb_policy;
FILE *backing = NULL;

int num_addresses = 0;

page_entry_t page_table[NUM_PAGES];
int frame_to_page[NUM_FRAMES];
unsigned long frame_last_used[NUM_FRAMES];
tlb_entry_t tlb[TLB_SIZE];

int next_free_frame = 0;
int fifo_frame_ptr = 0;
int fifo_tlb_ptr = 0;
unsigned long clock_ticks = 0;

signed char physical_mem[NUM_FRAMES][PAGE_SIZE];
int page_faults = 0;
int tlb_hits = 0;

int parse_policy(const char *s, policy_t *p) {
    if(strcmp(s, "fifo") == 0) {
        *p = POLICY_FIFO; return 0;
    }
    if(strcmp(s, "lru")  == 0){
        *p = POLICY_LRU;  return 0;
    }
    return -1;
}

void init_state(void) {
    for(int i = 0; i < NUM_PAGES; i++) {
        page_table[i].valid = 0;
        page_table[i].frame = -1;
    }
    for(int i = 0; i < NUM_FRAMES; i++) {
        frame_to_page[i] = -1;
        frame_last_used[i] = 0;
    }
    for(int i = 0; i < TLB_SIZE; i++) {
        tlb[i].valid = 0;
        tlb[i].page = -1;
        tlb[i].frame = -1;
        tlb[i].last_used = 0;
        tlb[i].loaded_at = 0;
    }
    next_free_frame = 0;
    fifo_frame_ptr = 0;
    fifo_tlb_ptr = 0;
    clock_ticks = 0;
}

int parse_address_line(const char *line, int *out) {
    char *end;

    errno = 0;
    long value = strtol(line, &end, 10);

    if (end == line) return -1;
    if (errno == ERANGE) return -1;

    while (*end != '\0') {
        if (!isspace((unsigned char)*end)) return -1;
        end++;
    }

    if (value < 0 || value > 65535) return -1;

    *out = (int)value;
    return 0;
} 

int tlb_lookup(int page) {
    (void)page;
    return -1;
}

int tlb_insert(int page, int frame) {
    (void)page; (void)frame;
    return -1;
}

void tlb_invalidate_page(int evicted_page) {
    for(int i = 0; i < TLB_SIZE; i++) {
        if(tlb[i].valid && tlb[i].page == evicted_page) {
            tlb[i].valid = 0;
            tlb[i].page = -1;
            tlb[i].frame = -1;
        }
    }
}

int get_frame_for_page(void) {
    if(next_free_frame < NUM_FRAMES) {
        return next_free_frame++;
    }

    int victim;

    if(page_policy == POLICY_FIFO) {
        victim = fifo_frame_ptr;
        fifo_frame_ptr = (fifo_frame_ptr + 1) % NUM_FRAMES;
    }else {
        victim = 0;
        for(int f = 1; f < NUM_FRAMES; f++) {
            if(frame_last_used[f] < frame_last_used[victim]) {
                victim = f;
            }
        }
    }

    int old_page = frame_to_page[victim];
    if(old_page >= 0) {
        page_table[old_page].valid = 0;
        page_table[old_page].frame = -1;
        tlb_invalidate_page(old_page);
    }
    frame_to_page[victim] = -1;

    return victim;
}

int handle_page_fault(int page) {
    int frame = get_frame_for_page();
    if(frame < 0) {
        return -1;
    }

    if(fseek(backing, (long)page * PAGE_SIZE, SEEK_SET) != 0) {
        return -1;
    }
    if(fread(physical_mem[frame], 1, PAGE_SIZE, backing) != PAGE_SIZE) {
        return -1;
    }

    page_table[page].valid = 1;
    page_table[page].frame = frame;
    frame_to_page[frame] = page;
    return frame;
}

int translate(int vaddr, FILE *out) {
    int page = (vaddr >> 8) & 0xFF;
    int offset = vaddr & 0xFF;
    int frame;
    unsigned long now = ++clock_ticks;

    int slot = tlb_lookup(page);

    if(slot >= 0) {
        tlb_hits++;
        frame = tlb[slot].frame;
        tlb[slot].last_used = now;
    }else {
        if(page_table[page].valid) {
            frame = page_table[page].frame;
        }else {
            page_faults++;
            frame = handle_page_fault(page);
            if(frame < 0) {
                fprintf(stderr, "Error: failed to load page %d\n", page);
                return -1;
            }
        }
        slot = tlb_insert(page, frame);
    }

    frame_last_used[frame] = now;

    int physical = frame * PAGE_SIZE + offset;
    int value = physical_mem[frame][offset];

    fprintf(out, "Virtual address: %d TLB: %d Physical address: %d Value: %d\n", vaddr, slot, physical, value);
    return 0;
}

int main(int argc, char *argv[]) {
    if(argc != 4) {
        fprintf(stderr, "Usage: %s <address_file> <fifo|lru> <fifo|lru>\n", argv[0]);
        return 1;
    }
    if(parse_policy(argv[2], &page_policy) != 0 || parse_policy(argv[3], &tlb_policy) != 0) {
        fprintf(stderr, "Error: policies must be 'fifo' or 'lru'.\n");
        return 1;
    }

    FILE *in = fopen(argv[1], "r");
    if (!in) {
        perror("Error opening address file");
        return 1;
    }

    backing = fopen(BACKING_FILE, "rb");
    if(!backing) { 
        perror("Error opening" BACKING_FILE);
        fclose(in); return 1;
    }

    FILE *out = fopen(OUTPUT_FILE, "w");
    
    if(!out) {
        perror("Error creating output file");
        fclose(in);
        fclose(backing);
        return 1; 
    }

    init_state();

    char line[128];
    int lineno = 0;
    while (fgets(line, sizeof line, in)) {
        lineno++;
        int vaddr;
        if (parse_address_line(line, &vaddr) != 0) {
            fprintf(stderr, "Error: invalid address at line %d of %s\n", lineno, argv[1]);
            fclose(in);
            fclose(backing);
            fclose(out);
            return 1;
        }
        if(translate(vaddr, out) != 0) {
            fclose(in);
            fclose(backing);
            fclose(out);
            return 1;
        }
        num_addresses++;
    }

    fclose(in);
    fclose(backing);
    fclose(out);
    
    return 0;
}