extern int trace_printf(const char *format, ...);
extern unsigned int HAL_GetTick(void);
extern void led_on(int n);
extern void led_off(int n);

typedef void (*Work_func_t)(void);
typedef struct {
	Work_func_t do_work;
	unsigned int delay;
	unsigned int repeat;
	unsigned int runme;
} Timer_task_t;

#define MAX_TASKS 8
Timer_task_t tasks[MAX_TASKS];

//update by TIM interrupt
void task_tick_update() {
	Timer_task_t *ptask;
	for (ptask = tasks; ptask < &tasks[MAX_TASKS]; ptask++) {
		if (ptask->do_work) {
			if (ptask->delay == 0) {
				ptask->runme++;
				if (ptask->repeat) {
					ptask->delay = (ptask->repeat-1);
				}
			} else {
				ptask->delay--;
			}
		}
	}
}

//add task to schedule
static Timer_task_t* task_add(Work_func_t workfunc, unsigned int delay,
		unsigned int period) {
	Timer_task_t *ptask = 0;
	for (ptask = tasks; ptask < &tasks[MAX_TASKS]; ptask++) {
		if (!ptask->do_work) {
			ptask->do_work = workfunc;
			ptask->delay = delay;
			ptask->repeat = period;
			ptask->runme = 0;
			return ptask;
		}
	}
	return ptask;
}

//add oneshot style task. it means 'repeat == 0'
static Timer_task_t* task_add_oneshot(Work_func_t workfunc, unsigned int delay) {
	return task_add(workfunc, delay, 0);
}

//delete a task
static void task_delete(Timer_task_t *ptask){
	if(!ptask)
		return;
	ptask->do_work = 0;
	ptask->delay = 0;
	ptask->repeat = 0;
	ptask->runme = 0;
}

//called by background main loop to poll who is timeouted to do work
static void task_schedule() {
	Timer_task_t *ptask = 0;
	for (ptask = tasks; ptask < &tasks[MAX_TASKS]; ptask++) {
		if (ptask->runme && ptask->do_work) {
			(*ptask->do_work)();
			ptask->runme--;
			if(ptask->repeat == 0){
				task_delete(ptask);
			}
		}
	}
	//go to low power mode?
	__wfi();
}


//led10/D10 flash
int flag=0;
static void led_flash(){
	if(flag){
		flag--;
		led_on(10);
	}
	else{
		flag++;
		led_off(10);
	}
}

static void tick_print(void) {
	trace_printf("current time: %d\r\n", HAL_GetTick());
}

static void alarm(){
	trace_printf("Alarm Fire @%d\r\n", HAL_GetTick());
//	task_add_oneshot(alarm, 1000);
}

void exp4_a_init(){
	//add task
	task_add(led_flash, 0, 500);
	task_add(tick_print, 0, 1000);
	task_add_oneshot(alarm, 1000);
}

void exp4_a_do(){
	task_schedule();
}

/*
 * create send fifo but no recv fifo
 */
int stop_exp4_a;
void exp4_a_main() {
	//init
	exp4_a_init();

	//background main loop
	while(!stop_exp4_a){
		exp4_a_do();
	}
}
