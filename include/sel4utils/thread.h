/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

/**
 *
 * Provides basic thread configuration/starting/cleanup functions.
 *
 * Any other operations (start, stop, resume) should use the seL4 API directly on
 * sel4utils_thread_t->tcb.cptr.
 *
 */
#ifndef _SEL4UTILS_THREAD_H
#define _SEL4UTILS_THREAD_H

#include <autoconf.h>

#ifdef CONFIG_LIB_SEL4_VSPACE

#include <sel4/sel4.h>

#include <vka/vka.h>

#include <vspace/vspace.h>

#include <utils/time.h>

#define SEL4UTILS_TIMESLICE (CONFIG_TIMER_TICK_MS * CONFIG_TIME_SLICE * US_IN_MS)

typedef struct sel4utils_thread {
    vka_object_t tcb;
    void *stack_top;
    seL4_CPtr ipc_buffer;
    seL4_Word ipc_buffer_addr;
    int own_sc;
    vka_object_t sched_context;
} sel4utils_thread_t;

typedef struct sel4utils_thread_config {
    /* fault_endpoint endpoint to set as the threads fault endpoint. Can be seL4_CapNull. */
    seL4_CPtr fault_endpoint;
    /* endpoint to set as the threads temporal fault endpoint. Can be seL4_CapNull. */
    seL4_CPtr temporal_fault_endpoint;
    /* max prio this thread can set itself or any other thread to */
    uint8_t max_priority;
    /* seL4 priority for the thread to be scheduled with. */
    uint8_t priority;
    /* max criticality that this thread can set other threads to (including itself) */
    uint32_t max_criticality;
    /* criticality of this thread */
    uint32_t criticality;
    /* root of the cspace to start the thread in */
    seL4_CNode cspace;
    /* data for cspace access */
    seL4_CapData_t cspace_root_data;
    /* create scheduling context or not? */
    int create_sc;
    /* params for created sc */
    seL4_SchedParams_t sched_params;
    /* sched control to populate sc with */
    seL4_CPtr sched_control;
    /* otherwise provide a sched control cap (can be seL4_CapNull) */
    seL4_CPtr sched_context;
} sel4utils_thread_config_t;

/**
 * Configure a thread, allocating any resources required.
 *
 * @param vka initialised vka to allocate objects with
 * @param parent vspace structure of the thread calling this function, used for temporary mappings
 * @param alloc initialised vspace structure to allocate virtual memory with
 * @param priority seL4 priority for the thread to be scheduled with.
 * @param maxPriority the maximum priority this thread will be allowed to promote itself
 *                    to or create other threads at.
 * @param sched_control time capability to allow a scheduling context to be configured for this thread
 * (can be NULL is sched_context is intended to be null).
 * @param cspace the root of the cspace to start the thread in
 * @param cspace_root_data data for cspace access
 * @param res an uninitialised sel4utils_thread_t data structure that will be initialised
 *            after this operation.
 *
 * @return 0 on success, -1 on failure. Use CONFIG_DEBUG to see error messages.
 */
int sel4utils_configure_thread(vka_t *vka, vspace_t *parent, vspace_t *alloc, seL4_CPtr fault_endpoint,
                               uint8_t priority, seL4_CNode cspace, seL4_CapData_t cspace_root_data, seL4_CPtr sched_control,
                               sel4utils_thread_t *res);


/**
 * Configure a passive thread (with no sched context), allocating any resources required.
 *
 * @param vka initialised vka to allocate objects with
 * @param parent vspace structure of the thread calling this function, used for temporary mappings
 * @param alloc initialised vspace structure to allocate virtual memory with
 * @param priority seL4 priority for the thread to be scheduled with.
 * @param maxPriority the maximum priority this thread will be allowed to promote itself
 *                    to or create other threads at.
 * @param cspace the root of the cspace to start the thread in
 * @param cspace_root_data data for cspace access
 * @param res an uninitialised sel4utils_thread_t data structure that will be initialised
 *            after this operation.
 *
 * @return 0 on success, -1 on failure. Use CONFIG_DEBUG to see error messages.
 */
int sel4utils_configure_passive_thread(vka_t *vka, vspace_t *parent, vspace_t *alloc, seL4_CPtr fault_endpoint,
                                       uint8_t priority, seL4_CNode cspace, seL4_CapData_t cspace_root_data, sel4utils_thread_t *res);

/**
 * As per sel4utils_configure_thread, but using a config struct.
 */
int sel4utils_configure_thread_config(vka_t *vka, vspace_t *parent, vspace_t *alloc,
                                      sel4utils_thread_config_t config, sel4utils_thread_t *res);

/**
 * Start a thread, allocating any resources required.
 * The third argument to the thread (in r2 for arm, on stack for ia32) will be the
 * address of the ipc buffer.
 *
 * @param thread      thread data structure that has been initialised with sel4utils_configure_thread
 * @param entry_point the address that the thread will start at
 *
 *                    NOTE: In order for the on-stack argument passing to work for ia32,
 *                    entry points must be functions.
 *
 *                    ie. jumping to this start symbol will work:
 *
 *                    void _start(int argc, char **argv) {
 *                        int ret = main(argc, argv);
 *                        exit(ret);
 *                    }
 *
 *
 *                    However, jumping to a start symbol like this:
 *
 *                    _start:
 *                         call main
 *
 *                    will NOT work, as call pushes an extra value (the return value)
 *                    onto the stack. If you really require an assembler stub, it should
 *                    decrement the stack value to account for this.
 *
 *                    ie.
 *
 *                    _start:
 *                         popl %eax
 *                         call main
 *
 *                    This does not apply for arm, as arguments are passed in registers.
 *
 *
 * @param arg0        a pointer to the arguments for this thread. User decides the protocol.
 * @param arg1        another pointer. User decides the protocol. Note that there are two args here
 *                    to easily support C standard: int main(int argc, char **argv).
 * @param resume      1 to start the thread immediately, 0 otherwise.
 *
 * @return 0 on success, -1 on failure.
 */
int sel4utils_start_thread(sel4utils_thread_t *thread, void *entry_point, void *arg0, void *arg1,
                           int resume);

/**
 * Release any resources used by this thread. The thread data structure will not be usable
 * until sel4utils_thread_configure is called again.
 *
 * @param vka the vka interface that this thread was initialised with
 * @param alloc the allocation interface that this thread was initialised with
 * @param thread the thread structure that was returned when the thread started
 */
void sel4utils_clean_up_thread(vka_t *vka, vspace_t *alloc, sel4utils_thread_t *thread);

/**
 * Start a fault handling thread that will print the name of the thread that faulted
 * as well as debugging information.
 *
 * The fault handler will run on the scheduling context of the faulting thread.
 *
 * @param fault_endpoint the fault_endpoint to wait on
 * @param vka allocator
 * @param vspace vspace (this library must be mapped into that vspace).
 * @param prio the priority to run the thread at (recommend highest possible)
 * @param cspace the cspace that the fault_endpoint is in
 * @param data the cspace_data for that cspace (with correct guard)
 * @param name the name of the thread to print if it faults
 * @param thread the thread data structure to populate
 *
 * @return 0 on success.
 */
int sel4utils_start_fault_handler(seL4_CPtr fault_endpoint, vka_t *vka, vspace_t *vspace,
                                  uint8_t prio, seL4_CPtr cspace, seL4_CapData_t data, char *name,
                                  seL4_CPtr sched_control, sel4utils_thread_t *res);


/**
 * Pretty print a fault messge.
 *
 * @param tag the message info tag delivered by the fault.
 * @param name thread name
 */
void sel4utils_print_fault_message(seL4_MessageInfo_t tag, char *name);

static inline seL4_TCB
sel4utils_get_tcb(sel4utils_thread_t *thread)
{
    return thread->tcb.cptr;
}

static inline int
sel4utils_suspend_thread(sel4utils_thread_t *thread)
{
    return seL4_TCB_Suspend(thread->tcb.cptr);
}

#endif /* CONFIG_LIB_SEL4_VSPACE */
#endif /* _SEL4UTILS_THREAD_H */
