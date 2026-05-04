#include <stdio.h>
#include <unistd.h>

#include <rte_eal.h>
#include <rte_lcore.h>
#include <rte_launch.h>
#include <rte_malloc.h>
#include <rte_rcu_qsbr.h>

/*  shared state  */
struct data {
    int value;
};

static struct data           *gdata;   /* RCU-protected pointer */
static struct rte_rcu_qsbr   *qsbr;
static volatile int           stop;

/* reader  */
static int reader(void *arg)
{
    (void)arg;
    unsigned id = rte_lcore_id();

    rte_rcu_qsbr_thread_register(qsbr, id);
    rte_rcu_qsbr_thread_online(qsbr, id);

    while (!stop) {
        rte_rcu_qsbr_lock(qsbr, id);

        struct data *d = __atomic_load_n(&gdata, __ATOMIC_ACQUIRE);
        printf("[reader lcore %u] value = %d\n", id, d->value);

        rte_rcu_qsbr_unlock(qsbr, id);

        rte_rcu_qsbr_quiescent(qsbr, id);   /* report quiescent state */

        sleep(1);
    }

    rte_rcu_qsbr_thread_offline(qsbr, id);
    rte_rcu_qsbr_thread_unregister(qsbr, id);
    return 0;
}

/* ── writer ── */
static void writer(void)
{
    for (int i = 0; i < 5; i++) {
        sleep(2);

        /* 1. allocate new version */
        struct data *new = rte_malloc(NULL, sizeof(*new), 0);
        new->value = i + 1;

        /* 2. swap in */
        struct data *old = __atomic_exchange_n(&gdata, new, __ATOMIC_ACQ_REL);

        printf("\n[writer] updated value → %d\n\n", new->value);

        /* 3. wait for all readers to finish using 'old' */
        rte_rcu_qsbr_synchronize(qsbr, RTE_QSBR_THRID_INVALID);

        /* 4. now safe to free */
        rte_free(old);
        printf("[writer] freed old value\n\n");
    }

    stop = 1;
}

int main(int argc, char **argv)
{
    rte_eal_init(argc, argv);

    /* allocate and init QSBR */
    size_t sz = rte_rcu_qsbr_get_memsize(RTE_MAX_LCORE);
    qsbr = rte_zmalloc(NULL, sz, 0);
    rte_rcu_qsbr_init(qsbr, RTE_MAX_LCORE);

    /* initial data */
    gdata = rte_malloc(NULL, sizeof(*gdata), 0);
    gdata->value = 0;

    /* launch reader on first worker lcore */
    unsigned worker = rte_get_next_lcore(-1, 1, 0);
    rte_eal_remote_launch(reader, NULL, worker);

    /* writer runs on main lcore */
    writer();

    rte_eal_mp_wait_lcore();

    /* cleanup */
    rte_free(gdata);
    rte_free(qsbr);
    rte_eal_cleanup();
    return 0;
}

// ### How it works in 4 steps
// Writer                              Reader
// ──────                              ──────
// 1. alloc new struct
// 2. atomic swap (old ↔ new)   ──► reader now sees new value
// 3. rcu_synchronize()         ──► blocks until reader passes quiescent
// 4. rte_free(old)             ──► safe, no reader holds old anymore