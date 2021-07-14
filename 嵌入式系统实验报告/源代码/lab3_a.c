/*
 *      Author: ljp
 */

#include "board.h"
#include "queue.h"
#include "fifo.h"

extern int led_cmd_exec(int argc, char *argv[]);

/*
 * poll structure
 */
typedef int (*pfn_poll_t)(void);
typedef void (*pfn_handler_t)(void);

typedef struct poller_t {
	pfn_poll_t poll;
	pfn_handler_t handler;

	TAILQ_ENTRY(poller_t)
	qentry;
} poller_t;

static TAILQ_HEAD(poller_queue_t, poller_t) pollers = TAILQ_HEAD_INITIALIZER(pollers);

//#define MAX_POLL_ENTRY 10
//static poller_t pollers[MAX_POLL_ENTRY];
//static int free_head = 0;

void poll_loop() {
	struct poller_t *poller;
	TAILQ_FOREACH(poller, &pollers, qentry)
	{
		if (poller->poll && poller->handler) {
			if ((*poller->poll)()) {
				(*poller->handler)();
			}
		}
	}
}

void poll_add(poller_t *poller, pfn_poll_t poll, pfn_handler_t handler) {
	poller->poll = poll;
	poller->handler = handler;
	TAILQ_INSERT_TAIL(&pollers, poller, qentry);
}

/*
 * uart recv & send
 */
static DEFINE_FIFO(fifo8_t, sendfifo, unsigned char, 64);

#define COMMAND_LEN_MAX 32
static char command[COMMAND_LEN_MAX];
static int cmd_idx = 0;

static int split_cmdline(char *cmd, unsigned int length, char *argv[]) {
	char *ptr;
	unsigned int position;
	unsigned int argc;

	ptr = cmd;
	position = 0;
	argc = 0;

	while (1) {
		/* strip blank and tab */
		while ((*ptr == ' ' || *ptr == '\t') && position < length) {
			*ptr = '\0';
			ptr++;
			position++;
		}
		if (position >= length)
			break;

		argv[argc] = ptr;
		argc++;
		while ((*ptr != ' ' && *ptr != '\t') && position < length) {
			ptr++;
			position++;
		}
		if (position >= length)
			break;
	}
	return argc;
}

void uart_recv_process(void) {
	char ch = board_getc();
	trace_printf("get start\n");
	FIFO_PUSH(&sendfifo,&ch);
	if(ch!='\r'&&cmd_idx<COMMAND_LEN_MAX){
				command[cmd_idx]=ch;
				cmd_idx++;
	}
	else{
		trace_printf("get end:%s\n",command);
		char *cmd[4];
		int sum = split_cmdline(command, cmd_idx, cmd);
		for(int i =0; i<sum; i++ ){
			trace_printf("%s\n", cmd[i]);
		}
		led_cmd_exec(sum, cmd);
		cmd_idx = 0;
		for(int i  = 0; i < COMMAND_LEN_MAX; i++){
			command[i]= '\0';
		}
	}
}

void uart_send_process(void) {
	while (board_putc_ready() && !FIFO_EMPTY(&sendfifo)) {
		unsigned char ch = 0;
		FIFO_POP_VAR(&sendfifo, ch);
		board_putc(ch);
	}
}

int uart_readble() {
	return board_getc_ready();
}

int uart_writeable() {
	return board_putc_ready();
}

int led_ready() {
	return 1;
}
extern void led_do(void);
static poller_t uart_read_poller, uart_write_poller, led_ready_poller;
void lab3_a_init() {
	//add to poll
	poll_add(&uart_read_poller, uart_readble, uart_recv_process);
	poll_add(&uart_write_poller, uart_writeable, uart_send_process);
	poll_add(&led_ready_poller, led_ready, led_do);
}

void lab3_a_do() {
	poll_loop();
}

/*
 * create send fifo but no recv fifo
 */
int stop_lab3_a;
void lab3_a_main() {
	lab3_a_init();
	//big loop
	while (!stop_lab3_a) {
		lab3_a_do();
	}
}

