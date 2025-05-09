#include "connection_status_monitor.h"

void csm_init(ConnectionStatusMonitor* monitor) {
	monitor->connection_status = CONNECTED;
	pthread_mutex_init(&monitor->connection_status_mutex, NULL);
	pthread_cond_init(&monitor->connection_status_cond, NULL);
}

void csm_destroy(ConnectionStatusMonitor* monitor) {
	pthread_mutex_destroy(&monitor->connection_status_mutex);
	pthread_cond_destroy(&monitor->connection_status_cond);
}