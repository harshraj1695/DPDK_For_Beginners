#pragma once

#define RING_NAME "FD_RING"
#define RING_SIZE 1024

#define PROC_A 1
#define PROC_B 2

enum msg_type {
    MSG_REQ,
    MSG_RESP,
    MSG_EXIT
};

struct msg {
    int src_id;
    int dst_id;
    enum msg_type type;
    int value;
};
