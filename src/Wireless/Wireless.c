#include "Wireless.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "secrets.h"
#include <stdlib.h>

// UI update APIs
extern void UI_SetStates(bool heater_on, bool steam_on, bool shot_on);
extern void UI_UpdateTemp(float current_c, float set_c, bool steam_mode);
extern void UI_UpdatePressure(float bar);
extern void UI_SetHeaterSwitch(bool on);

static volatile bool s_wifi_got_ip = false;

static void on_got_ip(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)data;
        printf("Got IP: %d.%d.%d.%d\r\n", IP2STR(&event->ip_info.ip));
        s_wifi_got_ip = true;
    }
}

static void WIFI_Init(void *arg)
{
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    wifi_config_t sta_cfg = {0};
    strncpy((char *)sta_cfg.sta.ssid, WIFI_SSID, sizeof(sta_cfg.sta.ssid));
    strncpy((char *)sta_cfg.sta.password, WIFI_PASS, sizeof(sta_cfg.sta.password));
    sta_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &on_got_ip, NULL, NULL));

    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_connect());

    const TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(10000);
    while (!s_wifi_got_ip && xTaskGetTickCount() < deadline) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    if (!s_wifi_got_ip) {
        printf("WiFi connect timeout for SSID '%s'\r\n", WIFI_SSID);
    }

    MQTT_Start();

    vTaskDelete(NULL);
}

void Wireless_Init(void)
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Start WiFi task
    xTaskCreatePinnedToCore(
        WIFI_Init,
        "WIFI task",
        4096,
        NULL,
        3,
        NULL,
        0);
}

// -------------------- MQTT client (subscriber/publisher) --------------------
static esp_mqtt_client_handle_t s_mqtt = NULL;
static bool s_heater_on = false, s_steam_on = false, s_shot_on = false;
static float s_set_temp_c = 0.0f, s_cur_temp_c = 0.0f, s_pressure_bar = 0.0f;

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    static const char *k_topics[] = {
        "brew_setpoint",
        "steam_setpoint",
        "heater",
        "shot_volume",
        "set_temp",
        "current_temp",
        "pressure",
        "shot_state",
        "steam_state",
    };
    char topic_buf[128];
    char t_copy[128];
    char d_copy[256];
    switch (event->event_id) {
    case MQTT_EVENT_CONNECTED:
        for (size_t i = 0; i < (sizeof(k_topics) / sizeof(k_topics[0])); ++i) {
            int n = snprintf(topic_buf, sizeof(topic_buf), "gaggia_classic/%s/%s/state", GAGGIA_ID, k_topics[i]);
            if (n > 0 && n < (int)sizeof(topic_buf)) {
                esp_mqtt_client_subscribe(event->client, topic_buf, 1);
            }
        }
#ifdef MQTT_LWT_TOPIC
        esp_mqtt_client_publish(event->client, MQTT_LWT_TOPIC, "online", 0, 1, true);
#endif
        break;
    case MQTT_EVENT_DISCONNECTED:
        printf("MQTT disconnected\r\n");
        break;
    case MQTT_EVENT_DATA:
    {
        int tl = event->topic_len < (int)sizeof(t_copy) - 1 ? event->topic_len : (int)sizeof(t_copy) - 1;
        int dl = event->data_len < (int)sizeof(d_copy) - 1 ? event->data_len : (int)sizeof(d_copy) - 1;
        memcpy(t_copy, event->topic, tl); t_copy[tl] = '\0';
        memcpy(d_copy, event->data, dl); d_copy[dl] = '\0';
        char *last_slash = strrchr(t_copy, '/');
        if (last_slash && last_slash != t_copy) {
            *last_slash = '\0';
            char *key = strrchr(t_copy, '/');
            if (key) {
                key++;
                bool is_on = (strcasecmp(d_copy, "ON") == 0 || strcmp(d_copy, "1") == 0 || strcasecmp(d_copy, "true") == 0);
                if (strcmp(key, "heater") == 0) {
                    s_heater_on = is_on; UI_SetHeaterSwitch(is_on);
                } else if (strcmp(key, "steam_state") == 0) {
                    s_steam_on = is_on;
                } else if (strcmp(key, "shot_state") == 0) {
                    s_shot_on = is_on;
                } else if (strcmp(key, "set_temp") == 0 || strcmp(key, "brew_setpoint") == 0 || strcmp(key, "steam_setpoint") == 0) {
                    s_set_temp_c = strtof(d_copy, NULL);
                } else if (strcmp(key, "current_temp") == 0) {
                    s_cur_temp_c = strtof(d_copy, NULL);
                } else if (strcmp(key, "pressure") == 0) {
                    s_pressure_bar = strtof(d_copy, NULL);
                    UI_UpdatePressure(s_pressure_bar);
                }
                UI_SetStates(s_heater_on, s_steam_on, s_shot_on);
                UI_UpdateTemp(s_cur_temp_c, s_set_temp_c, s_steam_on);
            }
        }
        break;
    }
    default:
        break;
    }
}

void MQTT_Start(void)
{
#if defined(MQTT_URI)
    if (s_mqtt || !s_wifi_got_ip) return;
    esp_mqtt_client_config_t cfg = {
        .broker.address.uri = MQTT_URI,
        .session.last_will = {
#ifdef MQTT_LWT_TOPIC
            .topic = MQTT_LWT_TOPIC,
            .msg = "offline",
            .msg_len = 7,
            .qos = 1,
            .retain = true,
#endif
        },
        .credentials = {
#ifdef MQTT_USERNAME
            .username = MQTT_USERNAME,
#endif
#ifdef MQTT_PASSWORD
            .authentication.password = MQTT_PASSWORD,
#endif
#ifdef MQTT_CLIENT_ID
            .client_id = MQTT_CLIENT_ID,
#endif
        },
    };

    s_mqtt = esp_mqtt_client_init(&cfg);
    if (!s_mqtt) {
        printf("MQTT init failed\r\n");
        return;
    }
    ESP_ERROR_CHECK(esp_mqtt_client_register_event(s_mqtt, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL));
    ESP_ERROR_CHECK(esp_mqtt_client_start(s_mqtt));
#else
    (void)s_mqtt;
    if (!s_wifi_got_ip) return;
    printf("MQTT: MQTT_URI not defined in secrets.h; disabled\r\n");
#endif
}

esp_mqtt_client_handle_t MQTT_GetClient(void)
{
    return s_mqtt;
}

int MQTT_Publish(const char *topic, const char *payload, int qos, bool retain)
{
    if (!s_mqtt) return -1;
    return esp_mqtt_client_publish(s_mqtt, topic, payload, 0, qos, retain);
}

