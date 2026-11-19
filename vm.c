#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define BACKING_FILE "BACKING_STORE.bin"
#define OUTPUT_FILE "output.txt"

policy_t tlb_policy;
FILE *backing = NULL;

int num_addresses = 0;

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
    if (!backing) { perror("Error opening " BACKING_FILE); fclose(in); return 1; }

    FILE *out = fopen(OUTPUT_FILE, "w");
    if (!out) { perror("Error creating output file"); fclose(in); fclose(backing); return 1; }

    init_state();

    char line[128];
    int lineno = 0;
    while (fgets(line, sizeof line, in)) {
        lineno++;
        int vaddr;
        if (parse_address_line(line, &vaddr) != 0) {
            fprintf(stderr, "Error: invalid address at line %d of %s\n", lineno, argv[1]);
            /* TODO: close files and exit cleanly */
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