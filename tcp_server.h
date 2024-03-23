#define TCP_TASK_PRIORITY (tskIDLE_PRIORITY + 2UL)
#define kConnectionThreadCount 3

#define AIRCR_Register (*((volatile uint32_t*)(PPB_BASE + 0x0ED0C)))

static inline void reboot()
{
    AIRCR_Register = 0x5FA0004;
}

typedef void (*connection_handle_t)(int);
typedef void (*disconnection_handle_t)(int);
typedef void (*msg_received_t)(int, char*);

void tcp_server_init(connection_handle_t conn_handle, disconnection_handle_t disc_handle, msg_received_t msg_receive);
void tcp_send_message(int con_id, char *msg);
void tcp_server_task(__unused void *params);

