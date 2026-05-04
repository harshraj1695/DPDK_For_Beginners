#include "source.h"

#include <stdio.h>

#define MAX_LINE 64

// reads private_ip public_ip pairs line by line from txt file
// skips blank lines and lines starting with #
void nat_load_from_file(struct rte_hash *nat, const char *filepath)
{
    FILE *f = fopen(filepath, "r");
    if (f == NULL) {
        printf("Error: could not open NAT config file: %s\n", filepath);
        return;
    }

    char line[MAX_LINE];
    char private_ip[32];
    char public_ip[32];
    int count = 0;

    while (fgets(line, sizeof(line), f) != NULL) {
        // skip blank lines and comments
        if (line[0] == '\n' || line[0] == '#')
            continue;

        if (sscanf(line, "%31s %31s", private_ip, public_ip) != 2) {
            printf("Skipping invalid line: %s", line);
            continue;
        }

        nat_add_mapping(nat, private_ip, public_ip);
        count++;
    }

    fclose(f);
    printf("Loaded %d NAT mappings from %s\n", count, filepath);
}