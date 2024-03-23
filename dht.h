// description of the DHT sensor functionality
// dht.h

#ifndef DHT_H
#define DHT_H

void dht_init();
void print_dht_error(int i, const char* error_message);
void send_dht_data_to_client(int i);
void temp_task();


#endif