#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"

#include "lwip/apps/lwiperf.h"
#include "lwip/ip4_addr.h"
#include "lwip/netif.h"

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

#include <lwip/sockets.h>
#include "tcp_server.h"


static int conn_sock = 0;
static xSemaphoreHandle s_ConnectionSemaphore;

static connection_handle_t on_connection = NULL;
static disconnection_handle_t on_disconnection = NULL;
static msg_received_t on_msg_received = NULL;

static void send_message(int socket, char *msg)
{
    int len = strlen(msg);
    int done = 0;
    while (done < len)
    {
        int done_now = send(socket, msg + done, len - done, 0);
        if (done_now <= 0)
            return;

        done += done_now;
    }
}

static int handle_single_command(int conn_sock)
{
    char buffer[128];
    int done = 0;
    send_message(conn_sock, "Enter command: ");
    while (done < sizeof(buffer))
    {
        int done_now = recv(conn_sock, buffer + done, sizeof(buffer) - done, 0);
        if (done_now <= 0)
            return -1;

        done += done_now;
        char *end = strnstr(buffer, "\r", done);
        if (!end)
            continue;

        *end = 0;

        on_msg_received(conn_sock, buffer);

        break;
    }

    return 0;
}

static void do_handle_connection(void *arg)
{
    conn_sock = (int)arg;
    on_connection(conn_sock);
    while (!handle_single_command(conn_sock))
    {
    }

    on_disconnection(conn_sock);

    closesocket(conn_sock);
    xSemaphoreGive(s_ConnectionSemaphore);
    vTaskDelete(NULL);
}

static void handle_connection(int conn_sock)
{
    TaskHandle_t task;
    xSemaphoreTake(s_ConnectionSemaphore, portMAX_DELAY);
    xTaskCreate(do_handle_connection, "Connection Thread", configMINIMAL_STACK_SIZE, (void *)conn_sock, TCP_TASK_PRIORITY, &task);
}

static void run_server()
{
    int server_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    struct sockaddr_in listen_addr =
        {
            .sin_len = sizeof(struct sockaddr_in),
            .sin_family = AF_INET,
            .sin_port = htons(1234),
            .sin_addr = 0,
        };

    if (server_sock < 0)
    {
        printf("Unable to create socket: error %d", errno);
        return;
    }

    if (bind(server_sock, (struct sockaddr *)&listen_addr, sizeof(listen_addr)) < 0)
    {
        printf("Unable to bind socket: error %d\n", errno);
        return;
    }

    if (listen(server_sock, kConnectionThreadCount * 2) < 0)
    {
        printf("Unable to listen on socket: error %d\n", errno);
        return;
    }

    printf("Starting server at %s on port %u\n", ip4addr_ntoa(netif_ip4_addr(netif_list)), ntohs(listen_addr.sin_port));

    while (true)
    {
        struct sockaddr_storage remote_addr;
        socklen_t len = sizeof(remote_addr);
        int conn_sock = accept(server_sock, (struct sockaddr *)&remote_addr, &len);
        if (conn_sock >= 0)
            handle_connection(conn_sock);
    }
}

void tcp_server_init(connection_handle_t conn_handle, disconnection_handle_t disc_handle, msg_received_t msg_receive)
{
    on_connection = conn_handle;
    on_disconnection = disc_handle;
    on_msg_received = msg_receive;

    s_ConnectionSemaphore = xSemaphoreCreateCounting(kConnectionThreadCount, kConnectionThreadCount);

}

void tcp_send_message(int con_id, char *msg)
{
    if (con_id != -1) {
        send_message(con_id, msg);
    }
}

void tcp_server_task(__unused void *params)
{
    if (cyw43_arch_init())
    {
        printf("failed to initialise\n");
        return;
    }

    cyw43_arch_enable_sta_mode();
    printf("Connecting to WiFi...\n");
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 60000))
    {
        printf("failed to connect - rebooting.\n");
        reboot();
    }
    else
    {
        printf("Connected.\n");
    }

    run_server();
    cyw43_arch_deinit();
    vTaskDelete(NULL);
}
