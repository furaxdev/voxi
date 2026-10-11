#include "task.h"
#include "mm/heap.h"
#include "scheduler.h"

void task_trampoline(void) {
    task_t *self = scheduler_current();
    self->entry(self->arg);
    self->state = TASK_DEAD;
    for (;;) yield(); /* never actually resumed again once dead */
}

task_t *task_create(void (*entry)(void *), void *arg, int id) {
    task_t *t = kmalloc(sizeof(task_t));
    if (!t) return NULL;

    t->kstack = kmalloc(TASK_KSTACK_SIZE);
    if (!t->kstack) {
        kfree(t);
        return NULL;
    }

    t->entry = entry;
    t->arg = arg;
    t->id = id;
    t->state = TASK_READY;
    t->next = NULL;

    /* Build an initial stack frame matching what switch_to() expects to
     * pop (r15,r14,r13,r12,rbx,rbp) followed by a return address. Since a
     * function entered via `ret` behaves exactly as if it was `call`ed,
     * landing on task_trampoline this way just works. */
    uint64_t *sp = (uint64_t *)(t->kstack + TASK_KSTACK_SIZE);
    *(--sp) = (uint64_t)task_trampoline; /* return address popped by ret */
    *(--sp) = 0; /* rbp */
    *(--sp) = 0; /* rbx */
    *(--sp) = 0; /* r12 */
    *(--sp) = 0; /* r13 */
    *(--sp) = 0; /* r14 */
    *(--sp) = 0; /* r15 */
    t->rsp = (uint64_t)sp;

    return t;
}

task_t *task_wrap_current(int id) {
    task_t *t = kmalloc(sizeof(task_t));
    if (!t) return NULL;
    t->kstack = NULL;
    t->entry = NULL;
    t->arg = NULL;
    t->id = id;
    t->state = TASK_RUNNING;
    t->next = NULL;
    t->rsp = 0; /* filled in by switch_to() on the first switch away */
    return t;
}
