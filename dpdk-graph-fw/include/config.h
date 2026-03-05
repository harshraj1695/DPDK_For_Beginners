#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

extern int firewall_enabled;
extern int app_running;

/* packet counters */
extern uint64_t rx_count;
extern uint64_t parser_count;
extern uint64_t firewall_count;
extern uint64_t tx_count;

#endif