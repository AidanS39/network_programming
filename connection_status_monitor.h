#ifndef CONNECTION_STATUS_MONITOR_H
#define CONNECTION_STATUS_MONITOR_H

#include <pthread.h>

typedef enum _ConnectionStatus {
    CONNECTED,
    SENT_DISCONNECT_REQUEST,
    RECEIVED_DISCONNECT_CONFIRMATION
} ConnectionStatus;


// Monitor for connection status
typedef struct _ConnectionStatusMonitor {
    ConnectionStatus connection_status;
    pthread_mutex_t connection_status_mutex;
    pthread_cond_t connection_status_cond;
} ConnectionStatusMonitor;

void csm_init(ConnectionStatusMonitor* monitor);
void csm_destroy(ConnectionStatusMonitor* monitor);

#endif
