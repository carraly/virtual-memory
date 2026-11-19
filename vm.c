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

    if (end == line) return -1;          /* no digits at all */
    if (errno == ERANGE) return -1;      /* overflow */

    while (*end != '\0') {               /* only whitespace may follow */
        if (!isspace((unsigned char)*end)) return -1;
        end++;
    }

    if (value < 0 || value > 65535) return -1;   /* 16-bit address */

    *out = (int)value;
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
            return 1;
        }
        translate(vaddr, out);
        num_addresses++;
    }

    fclose(in);
    fclose(backing);
    fclose(out);
    
    return 0;
}