#pragma once

#include <stdbool.h>

void dashboard_init(void);
/* Publishes the latest DHT reading for dashboard requests. */
void dashboard_publish_env(float temp, float humidity, bool valid);
