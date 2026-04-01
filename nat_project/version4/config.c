#include "source.h"

#include <stdio.h>

#define MAX_LINE 64

// reads start_ip and range from txt file and returns a nat_pool
// skips blank lines and lines starting with #
// first valid line is used as the pool config
struct nat_pool *nat_load_from_file(const char *filepath, uint64_t hz)
{
    FILE *f = fopen(filepath, "r");
    if (f == NULL) {
        printf("Error: could not open NAT config file: %s\n", filepath);
        return NULL;
    }

    char     line[MAX_LINE];
    char     start_ip[32];
    uint32_t range = 0;

    while (fgets(line, sizeof(line), f) != NULL) {
        // skip blank lines and comments
        if (line[0] == '\n' || line[0] == '#')
            continue;

        if (sscanf(line, "%31s %u", start_ip, &range) != 2) {
            printf("Skipping invalid line: %s", line);
            continue;
        }

        // first valid line is the pool config
        break;
    }

    fclose(f);

    if (range == 0) {
        printf("Error: no valid pool config found in %s\n", filepath);
        return NULL;
    }

    printf("Loaded pool config: start=%s range=%u\n", start_ip, range);
    return nat_pool_create(start_ip, range, hz);
}