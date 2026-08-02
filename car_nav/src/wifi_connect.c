#include "wifi_connect.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

static const char *TAG = "wifi";
static EventGroupHandle_t evt = NULL;
static char ip[16] = "0.0.0.0";
static int retry = 0;

#define CONNECTED BIT0
#define FAILED    BIT1

static void handler(void *arg, esp_event_base_t base, int32_t id, void *data) {
    if (base == WIFI_EVENT) {
        if (id == WIFI_EVENT_STA_START) esp_wifi_connect();
        else if (id == WIFI_EVENT_STA_DISCONNECTED) {
            if (retry++ < 10) { esp_wifi_connect(); ESP_LOGW(TAG, "retry %d", retry); }
            else xEventGroupSetBits(evt, FAILED);
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *e = data;
        snprintf(ip, sizeof(ip), IPSTR, IP2STR(&e->ip_info.ip));
        ESP_LOGI(TAG, "IP: %s", ip);
        retry = 0;
        xEventGroupSetBits(evt, CONNECTED);
    }
}

esp_err_t wifi_connect(const char *ssid, const char *pass, int timeout_ms) {
    nvs_flash_init();
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    evt = xEventGroupCreate();
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &handler, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &handler, NULL, NULL);
    wifi_config_t wc = {0};
    strncpy((char*)wc.sta.ssid, ssid, 32);
    strncpy((char*)wc.sta.password, pass, 64);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wc);
    esp_wifi_start();
    EventBits_t bits = xEventGroupWaitBits(evt, CONNECTED | FAILED, pdFALSE, pdFALSE,
        timeout_ms > 0 ? pdMS_TO_TICKS(timeout_ms) : portMAX_DELAY);
    return (bits & CONNECTED) ? ESP_OK : ESP_FAIL;
}

void wifi_disconnect(void) {
    esp_wifi_stop(); esp_wifi_deinit();
    if (evt) vEventGroupDelete(evt);
}

const char *wifi_get_ip(void) { return ip; }
int wifi_is_connected(void) { return ip[0] != '0'; }
