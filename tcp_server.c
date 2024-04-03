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
    while (done < sizeof(buffer))
    {
        int done_now = recv(conn_sock, buffer + done, sizeof(buffer) - done, 0);
        if (done_now <= 0)
            return -1;

        done += done_now;
        char *end = strnstr(buffer, "\n", done);
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


#include "lwip/pbuf.h"
#include "lwip/udp.h"

#define UDP_PORT 4444
#define BEACON_MSG_LEN_MAX 127
#define BEACON_TARGET "255.255.255.255"
#define BEACON_INTERVAL_MS 5000

void run_udp_beacon() {
    struct udp_pcb* pcb = udp_new();

    ip_addr_t addr;
    ipaddr_aton(BEACON_TARGET, &addr);

    int counter = 0;
    while (true) {
        struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, BEACON_MSG_LEN_MAX+1, PBUF_RAM);
        char *req = (char *)p->payload;
        memset(req, 0, BEACON_MSG_LEN_MAX+1);
        snprintf(req, BEACON_MSG_LEN_MAX, "%d\n", counter);
        err_t er = udp_sendto(pcb, p, &addr, UDP_PORT);
        pbuf_free(p);
        if (er != ERR_OK) {
            printf("Failed to send UDP packet! error=%d", er);
        } else {
            printf("Sent packet %d\n", counter);
            counter++;
        }

        // Note in practice for this simple UDP transmitter,
        // the end result for both background and poll is the same

#if PICO_CYW43_ARCH_POLL
        // if you are using pico_cyw43_arch_poll, then you must poll periodically from your
        // main loop (not from a timer) to check for Wi-Fi driver or lwIP work that needs to be done.
        cyw43_arch_poll();
        sleep_ms(BEACON_INTERVAL_MS);
#else
        // if you are not using pico_cyw43_arch_poll, then WiFI driver and lwIP work
        // is done via interrupt in the background. This sleep is just an example of some (blocking)
        // work you might be doing.
        sleep_ms(BEACON_INTERVAL_MS);
#endif
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
    printf("Connecting to WiFi... ");
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 300000))
    {
        printf("Failed to connect - retry. \n");
        if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 300000))
        {
            printf("Failed to connect - rebooting.\n");
            reboot();
        }
    }
    printf("Connected.\n");


    run_server();
    cyw43_arch_deinit();
    vTaskDelete(NULL);
}
