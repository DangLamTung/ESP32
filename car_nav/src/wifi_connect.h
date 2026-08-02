#pragma once
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t wifi_connect(const char *ssid, const char *pass, int timeout_ms);
void wifi_disconnect(void);
const char *wifi_get_ip(void);
int wifi_is_connected(void);

#ifdef __cplusplus
}
#endif
