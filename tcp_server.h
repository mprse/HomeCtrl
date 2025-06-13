#define TCP_TASK_PRIORITY (tskIDLE_PRIORITY + 2UL)
#define kConnectionThreadCount 3

typedef void (*connection_handle_t)(int);
typedef void (*disconnection_handle_t)(int);
typedef void (*msg_received_t)(int, char*);

void tcp_server_init(connection_handle_t conn_handle, disconnection_handle_t disc_handle, msg_received_t msg_receive);
void tcp_send_message(char *msg);
void tcp_server_task(__unused void *params);

