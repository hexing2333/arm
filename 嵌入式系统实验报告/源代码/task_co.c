/*
 *      Author: ljp
 */


#include <stdlib.h>
#include <string.h>

#include "board.h"
#include "task.h"


static TAILQ_HEAD(ready_queue_t, task_t) ready_queue;
static TAILQ_HEAD(wait_queue_t, task_t) wait_queue;

task_t *current_task;
static task_t *next_task;

void (*cpu_context_swich_call)(void);

void set_cpu_context_swich_call (void (*cscall)(void)) {
	cpu_context_swich_call = cscall;
}

stack_t* task_switch_sp(stack_t *sp) {
	if(current_task) {
		current_task->context = (void*) sp;
	}
	current_task = next_task;
	return (void*)next_task->context;
}

static task_t* task_change_state_to(task_t* t, state_t newstate) {
	switch (t->state) {
		case TASK_DEAD:
			if(newstate == TASK_READY) {
				//add task to ready list
				TAILQ_INSERT_TAIL(&ready_queue, t, stateq_node);
			}
			break;
		case TASK_READY:
				//#error NOT implemented!
			break;
		case TASK_RUNNING:
			if(newstate == TASK_READY){
				TAILQ_INSERT_TAIL(&ready_queue, t, stateq_node);
			} else if(newstate == TASK_BLOCKED) {
				TAILQ_INSERT_TAIL(&wait_queue, t, stateq_node);
			} else if(newstate == TASK_DEAD) {
			}
			break;
		case TASK_BLOCKED:
			if(newstate == TASK_READY){
				TAILQ_REMOVE(&wait_queue, t, stateq_node);
				TAILQ_INSERT_TAIL(&ready_queue, t, stateq_node);
			} else if(newstate == TASK_DEAD) {
				TAILQ_REMOVE(&wait_queue, t, stateq_node);
			}
			break;
		default:
			break;
	}
	t->state = newstate;
	return t;
}

task_t *get_current_task(void) {
	return current_task;
}

//parent function for monitor
static void task_guard(task_t* th) {
	if (!th) {
		return;
	}
	if (th->entry) {
		(*th->entry)(th->arg);
	}
	task_delete(th);
}

static task_t *pick_next(void){
	return TAILQ_FIRST(&ready_queue);
}

task_t *pick_next_rr(void) {
	task_t* t = TAILQ_FIRST(&ready_queue);
	TAILQ_REMOVE(&ready_queue, t, stateq_node);
	TAILQ_INSERT_TAIL(&ready_queue, t, stateq_node);
	return t;
}

void task_switch_to(task_t *to) {
	if(!to || (current_task == to)) {
		return;
	}
	//change state
	next_task = to;
	if(current_task && current_task->state == TASK_RUNNING) {
		task_change_state_to(current_task, TASK_READY);
	}
	task_change_state_to(next_task, TASK_RUNNING);
	//do switch
	(*cpu_context_swich_call)();
}


void task_schedule(void) {
	task_switch_to(pick_next());
}

void task_wait(void *event) {
	task_change_state_to(current_task, TASK_BLOCKED);
	current_task->wait_event = event;
	task_schedule();
}

void task_wakeup(void *event) {
	task_t *t;
	TAILQ_FOREACH(t, &wait_queue, stateq_node) {
		if(t->wait_event == event) {
			task_change_state_to(t, TASK_READY);
			task_schedule();
			return;
		}
	}
}

void task_yield(void) {
	task_schedule();
}

void task_delete(task_t *t) {
	task_change_state_to(t, TASK_DEAD);
	task_schedule();
}

task_t * task_create(task_t *t, task_func_t func, void *arg, const char *name) {
	t->entry = func;
	t->arg = arg;
	t->name = name;

	memset(t->stack, 'E', sizeof(t->stack));
	//create an init stack frame for function to become task
	stack_t *stack_top = (stack_t*) ((char*) t->stack + sizeof(t->stack));
	t->context = (void*)cpu_task_stack_prepair(stack_top, (task_func_t) task_guard, t);

	t->state = TASK_DEAD;

	task_change_state_to(t, TASK_READY);
	return t;
}

void tasklib_init(){
	TAILQ_INIT(&ready_queue);
	TAILQ_INIT(&wait_queue);
	current_task = next_task =NULL;
}

//////////////////////////////////////////////////////
static task_t taskidle;

static void idle_task(void){
	while(1) {
		task_yield();
//		HAL_CPU_Sleep();
//		trace_printf("#");
//		HAL_Delay(100);
	}
}




void task_co_init(void) {
	tasklib_init();
	set_cpu_context_swich_call(&syscall_svc);
	task_create(&taskidle, (task_func_t)idle_task, NULL, "idle_task");
}

