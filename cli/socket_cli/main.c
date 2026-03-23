#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <rte_common.h>
#include <rte_eal.h>
#include <rte_launch.h>
#include <rte_lcore.h>
#include <rte_cycles.h>

#include <cmdline.h>
#include <cmdline_parse.h>
#include <cmdline_parse_string.h>
#include <cmdline_parse_num.h>
#include <cmdline_rdline.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>

struct cmd_hello_result {
    char *hello;
    uint16_t number;
};

cmdline_parse_token_string_t cmd_hello =
    TOKEN_STRING_INITIALIZER(struct cmd_hello_result, hello, "hello");

cmdline_parse_token_num_t cmd_number =
    TOKEN_NUM_INITIALIZER(struct cmd_hello_result, number, RTE_UINT16);

static void
cmd_hello_parsed(void *parsed_result,
                 struct cmdline *cl,
                 void *data)
{
    struct cmd_hello_result *res = parsed_result;
    cmdline_printf(cl, "Hello! You entered number: %u\n", res->number);
}

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
    cmdline_printf(cl, "Bye!\n");
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

cmdline_parse_inst_t cmd_hello_inst = {
    .f = cmd_hello_parsed,
    .data = NULL,
    .help_str = "hello <number>",
    .tokens = {
        (void *)&cmd_hello,
        (void *)&cmd_number,
        NULL,
    },
};

cmdline_parse_ctx_t main_ctx[] = {
    (cmdline_parse_inst_t *)&cmd_hello_inst,
    (cmdline_parse_inst_t *)&cmd_exit,
    NULL,
};

static int cli_thread(void *arg)
{
    int client_fd = (intptr_t)arg;

    struct cmdline *cl;

    cl = cmdline_new(main_ctx, "dpdk> ", client_fd, client_fd);
    if (cl == NULL) {
        printf("Failed to create cmdline\n");
        close(client_fd);
        return -1;
    }

    cmdline_interact(cl);

    cmdline_free(cl);
    close(client_fd);

    return 0;
}

int main(int argc, char **argv)
{
    int ret;

    ret = rte_eal_init(argc, argv);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "EAL init failed\n");

    int server_fd;
    struct sockaddr_in addr;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
        rte_exit(EXIT_FAILURE, "socket failed\n");

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(9090);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        rte_exit(EXIT_FAILURE, "bind failed\n");

    if (listen(server_fd, 5) < 0)
        rte_exit(EXIT_FAILURE, "listen failed\n");

    printf("Socket CLI running on port 9090...\n");

   while (1) {
    int client_fd = accept(server_fd, NULL, NULL);

    if (client_fd < 0) {
        perror("accept");
        continue;
    }

    printf("Client connected!\n");

    // run CLI in SAME thread (simpler & stable)
    struct cmdline *cl;

    cl = cmdline_new(main_ctx, "dpdk> ", client_fd, client_fd);
    if (cl == NULL) {
        printf("cmdline creation failed\n");
        close(client_fd);
        continue;
    }

    cmdline_interact(cl);

    cmdline_free(cl);
    close(client_fd);

    printf("Client disconnected\n");
}
    return 0;
}