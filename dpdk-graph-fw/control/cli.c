#include <stdio.h>
#include <string.h>
#include "config.h"
#include <stdint.h>

int firewall_enabled = 0;
int app_running = 1;

uint64_t rx_count = 0;
uint64_t parser_count = 0;
uint64_t firewall_count = 0;
uint64_t tx_count = 0;

void run_cli()
{
    char cmd[64];

    while (app_running) {

        printf("cli> ");
        fgets(cmd, sizeof(cmd), stdin);

        if (strncmp(cmd, "enable firewall", 15) == 0) {

            firewall_enabled = 1;
            printf("Firewall enabled\n");
        }

        else if (strncmp(cmd, "disable firewall", 16) == 0) {

            firewall_enabled = 0;
            printf("Firewall disabled\n");
        }

        else if (strncmp(cmd, "show stats", 10) == 0) {

            printf("\nNode statistics\n");
            printf("RX node:       %lu packets\n", rx_count);
            printf("Parser node:   %lu packets\n", parser_count);
            printf("Firewall node: %lu packets\n", firewall_count);
            printf("TX node:       %lu packets\n\n", tx_count);
        }

        else if (strncmp(cmd, "quit", 4) == 0) {

            printf("Stopping application\n");
            app_running = 0;
            break;
        }

        else {

            printf("Commands:\n");
            printf("enable firewall\n");
            printf("disable firewall\n");
            printf("show stats\n");
            printf("quit\n");
        }
    }
}