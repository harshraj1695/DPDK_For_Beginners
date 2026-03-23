#include <rte_launch.h>
#include <rte_lcore.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include <rte_common.h>
#include <rte_eal.h>

#include <cmdline.h>
#include <cmdline_parse.h>
#include <cmdline_parse_string.h>
#include <cmdline_parse_num.h>
#include <cmdline_rdline.h>
#include <cmdline_socket.h>  


//global data to pass to cli
int a=42;
//RESULT STRUCT

struct cmd_show_port_result {
    char *show;
    char *port;
    uint16_t port_id;
};

// TOKENS

cmdline_parse_token_string_t cmd_show =
    TOKEN_STRING_INITIALIZER(struct cmd_show_port_result, show, "show");

cmdline_parse_token_string_t cmd_port =
    TOKEN_STRING_INITIALIZER(struct cmd_show_port_result, port, "port");

cmdline_parse_token_num_t cmd_port_id =
    TOKEN_NUM_INITIALIZER(struct cmd_show_port_result, port_id, RTE_UINT16);

// HANDLER FUNCTION

static void
cmd_show_port_parsed(void *parsed_result,
                     struct cmdline *cl,
                     void *data)
{
    struct cmd_show_port_result *res = parsed_result;
    printf("The lcore id is %d\n", rte_lcore_id());
    printf("You entered: show port %u and data is %d\n", res->port_id, (*(int *)data)++);
}

// COMMAND INSTANCE

cmdline_parse_inst_t cmd_show_port = {
    .f = cmd_show_port_parsed,
    .data = &a,
    .help_str = "show port <port_id>",
    .tokens = {
        (void *)&cmd_show,
        (void *)&cmd_port,
        (void *)&cmd_port_id,
        NULL,
    },
};

//EXIT COMMAND

struct cmd_exit_result {
    char *exit;
};

cmdline_parse_token_string_t cmd_exit_token =
    TOKEN_STRING_INITIALIZER(struct cmd_exit_result, exit, "exit");

static void
cmd_exit_parsed(void *parsed_result,
                struct cmdline *cl,
                void *data)
{
    printf("Exiting...\n");
    cmdline_quit(cl);
}

cmdline_parse_inst_t cmd_exit = {
    .f = cmd_exit_parsed,
    .data = NULL,
    .help_str = "exit application",
    .tokens = {
        (void *)&cmd_exit_token,
        NULL,
    },
};


// main command context
cmdline_parse_ctx_t main_ctx[] = {
    (cmdline_parse_inst_t *)&cmd_show_port,
    (cmdline_parse_inst_t *)&cmd_exit,
    NULL,
};

int launching(void *arg) {
    struct cmdline *cl = (struct cmdline *)arg;
    cmdline_interact(cl);
    return 0;
}

int main(int argc, char **argv)
{
    int ret;

    ret = rte_eal_init(argc, argv);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "Error with EAL init\n");

    // Create CLI
    struct cmdline *cl;
    cl = cmdline_stdin_new(main_ctx, "dpdk-cli> ");

    if (cl == NULL)
        rte_exit(EXIT_FAILURE, "Cannot create cmdline\n");

    printf("The lcore id is %d\n", rte_lcore_id());

    // start cli on another lcore
    rte_eal_remote_launch(launching, cl, 1);

    rte_eal_mp_wait_lcore();
    // Start CLI loop
    // cmdline_interact(cl);

    // Cleanup
    cmdline_stdin_exit(cl);

    return 0;
}