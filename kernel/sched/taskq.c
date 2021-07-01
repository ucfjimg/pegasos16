#include "task.h"
#include "printk.h"

/**
 * these routines all manipulate a task queue. All task queues are circular, and
 * new tasks are pushed just behore head.
 * 
 * none of these routines are task-safe on their own; the caller is responsible for
 * that if interruption is possible.
 */ 

/* Initialize a task queue
 */
void taskq_init(taskq_t *queue)
{
    queue->head = NULL;
}

/* Put the task on the given queue. It must not already be on another queue.
 */
void taskq_push(taskq_t *queue, task_t *task)
{
    register task_t *head = queue->head;

    task->owner = queue;

    if (head) {
        head->prev->next = task;
        task->prev = head->prev;
        head->prev = task;
        task->next = head;

        return;
    }

    queue->head = task;
    task->next = task;
    task->prev = task;
}

/* Remove the task (if any) from the front of a queue. Returns NULL if the queue
 * is empty.
 */
task_t *taskq_pop(taskq_t *queue)
{
    task_t *task;
    task_t *head = queue->head;

    if (head == NULL) {
        return NULL;
    }

    task = head;

    if (task->next == task) {
        queue->head = NULL;
    } else {
        head = queue->head = task->next;

        head->prev = task->prev;
        head->prev->next = head;
    }

    task->next = NULL;
    task->prev = NULL;
    task->owner = NULL;

    return task;
}

/* Dequeue the task from any queue it's already in.
 */
void task_dequeue(task_t *task)
{
    taskq_t *queue;

    if ((queue = task->owner) != NULL) {
        if (task->next == task) {
            /* last task in queue */
            queue->head = NULL;
        } else {
            if (queue->head == task) {
                queue->head = task->next;
            }
            task->prev->next = task->next;
            task->next->prev = task->prev;
        }
    }

    task->prev = NULL;
    task->next = NULL;
    task->owner = NULL;
}

