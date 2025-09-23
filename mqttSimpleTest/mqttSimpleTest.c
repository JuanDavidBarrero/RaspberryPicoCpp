#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "pico/unique_id.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "lwip/apps/mqtt.h"
#include "lwip/apps/mqtt_priv.h"
#include "lwip/dns.h"
#include "lwip/altcp_tls.h"

#ifndef MQTT_SERVER
#error "Debes definir MQTT_SERVER"
#endif

#define MQTT_RPI_PORT 1883
#define MQTT_RPI_TLS_PORT 8883
#define MQTT_USERNAME "hades"
#define MQTT_PASSWORD "lobo"
#define MQTT_DEVICE_NAME "pico"

#define MQTT_KEEP_ALIVE_S 60
#define MQTT_UNIQUE_TOPIC 0

#define MQTT_WILL_TOPIC "esp32/online"
#define MQTT_WILL_MSG "0"
#define MQTT_WILL_QOS 1

#ifdef MQTT_CERT_INC
#include MQTT_CERT_INC
#endif

#define MQTT_SUBSCRIBE_QOS 1
#define MQTT_PUBLISH_QOS 1
#define MQTT_PUBLISH_RETAIN 0

#define LED_PIN 8

#ifndef MQTT_TOPIC_LEN
#define MQTT_TOPIC_LEN 100
#endif

typedef struct
{
    mqtt_client_t *mqtt_client_inst;
    struct mqtt_connect_client_info_t mqtt_client_info;
    char data[MQTT_OUTPUT_RINGBUF_SIZE];
    char topic[MQTT_TOPIC_LEN];
    uint32_t len;
    ip_addr_t mqtt_server_address;
    bool connect_done;
    bool subscribed;
} MQTT_CLIENT_DATA_T;

static void pub_request_cb(__unused void *arg, err_t err)
{
    if (err != 0)
    {
        printf("Error en la solicitud de publicación: %d\n", err);
    }
}

static const char *full_topic(MQTT_CLIENT_DATA_T *state, const char *name)
{
#if MQTT_UNIQUE_TOPIC
    static char full_topic[MQTT_TOPIC_LEN];
    snprintf(full_topic, sizeof(full_topic), "/%s%s", state->mqtt_client_info.client_id, name);
    return full_topic;
#else
    return name;
#endif
}

static void mqtt_incoming_publish_cb(void *arg, const char *topic, u32_t tot_len)
{
    MQTT_CLIENT_DATA_T *state = (MQTT_CLIENT_DATA_T *)arg;
    strncpy(state->topic, topic, sizeof(state->topic));
}

static void mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags)
{
    MQTT_CLIENT_DATA_T *state = (MQTT_CLIENT_DATA_T *)arg;

    printf("Mensaje recibido en el tópico '%s': %.*s\n", state->topic, len, data);

    if (strcmp(state->topic, "esp32/set") == 0)
    {

        char payload[len + 1];
        memcpy(payload, data, len);
        payload[len] = '\0';

        if (strstr(payload, "\"on\""))
        {
            printf("Comando recibido: ON -> Encendiendo LED en GPIO %d\n", LED_PIN);
            gpio_put(LED_PIN, 1);
        }
        else if (strstr(payload, "\"off\""))
        {
            printf("Comando recibido: OFF -> Apagando LED en GPIO %d\n", LED_PIN);
            gpio_put(LED_PIN, 0);
        }
    }
}

void mqtt_subscribe_to_topic(MQTT_CLIENT_DATA_T *state, const char *topic);

static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status)
{
    MQTT_CLIENT_DATA_T *state = (MQTT_CLIENT_DATA_T *)arg;
    if (status == MQTT_CONNECT_ACCEPTED)
    {
        state->connect_done = true;
        printf("Conexión MQTT aceptada.\n");

        if (state->mqtt_client_info.will_topic)
        {
            mqtt_publish(state->mqtt_client_inst, state->mqtt_client_info.will_topic, "1", 1, MQTT_WILL_QOS, true, pub_request_cb, state);
        }

        mqtt_subscribe_to_topic(state, "esp32/set");
    }
    else
    {
        printf("Error en la conexión MQTT: %d\n", status);
        state->connect_done = false;
    }
}

err_t mqtt_connect(MQTT_CLIENT_DATA_T *state)
{
    state->mqtt_client_inst = mqtt_client_new();
    if (!state->mqtt_client_inst)
    {
        printf("Error al crear la instancia del cliente MQTT.\n");
        return ERR_MEM;
    }

    printf("Conectando al broker MQTT en %s (sin TLS)...\n", ipaddr_ntoa(&state->mqtt_server_address));

    cyw43_arch_lwip_begin();
    err_t err = mqtt_client_connect(state->mqtt_client_inst, &state->mqtt_server_address, MQTT_RPI_PORT, mqtt_connection_cb, state, &state->mqtt_client_info);
    cyw43_arch_lwip_end();

    if (err != ERR_OK)
    {
        printf("Error al conectar con el broker MQTT: %d\n", err);
    }

    mqtt_set_inpub_callback(state->mqtt_client_inst, mqtt_incoming_publish_cb, mqtt_incoming_data_cb, state);

    return err;
}

void mqtt_publish_message(MQTT_CLIENT_DATA_T *state, const char *topic, const char *message)
{
    if (!mqtt_client_is_connected(state->mqtt_client_inst))
    {
        printf("No se puede publicar, el cliente no está conectado.\n");
        return;
    }

    const char *full_topic_name = full_topic(state, topic);
    printf("Publicando en '%s': %s\n", full_topic_name, message);

    cyw43_arch_lwip_begin();
    mqtt_publish(
        state->mqtt_client_inst,
        full_topic_name,
        message,
        strlen(message),
        MQTT_PUBLISH_QOS,
        MQTT_PUBLISH_RETAIN,
        pub_request_cb,
        state);
    cyw43_arch_lwip_end();
}

void mqtt_subscribe_to_topic(MQTT_CLIENT_DATA_T *state, const char *topic)
{
    if (!mqtt_client_is_connected(state->mqtt_client_inst))
    {
        printf("No se puede suscribir, el cliente no está conectado.\n");
        return;
    }

    const char *full_topic_name = full_topic(state, topic);
    printf("Suscribiéndose al tópico '%s'\n", full_topic_name);

    cyw43_arch_lwip_begin();
    mqtt_sub_unsub(
        state->mqtt_client_inst,
        full_topic_name,
        MQTT_SUBSCRIBE_QOS,
        NULL,
        state,
        1);
    cyw43_arch_lwip_end();
}

static void dns_found(const char *hostname, const ip_addr_t *ipaddr, void *arg)
{
    MQTT_CLIENT_DATA_T *state = (MQTT_CLIENT_DATA_T *)arg;
    if (ipaddr)
    {
        state->mqtt_server_address = *ipaddr;
        mqtt_connect(state);
    }
    else
    {
        printf("Error en la solicitud DNS.\n");
    }
}

int main(void)
{
    stdio_init_all();
    printf("Iniciando cliente MQTT...\n");

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    static MQTT_CLIENT_DATA_T state;
    memset(&state, 0, sizeof(MQTT_CLIENT_DATA_T));

    if (cyw43_arch_init())
    {
        printf("Error al inicializar CYW43.\n");
        return 1;
    }

    char unique_id_buf[5];
    pico_get_unique_board_id_string(unique_id_buf, sizeof(unique_id_buf));
    for (int i = 0; i < sizeof(unique_id_buf) - 1; i++)
    {
        unique_id_buf[i] = tolower(unique_id_buf[i]);
    }
    char client_id_buf[sizeof(MQTT_DEVICE_NAME) + sizeof(unique_id_buf)];
    snprintf(client_id_buf, sizeof(client_id_buf), "%s%s", MQTT_DEVICE_NAME, unique_id_buf);
    printf("ID del dispositivo: %s\n", client_id_buf);

    state.mqtt_client_info.client_id = client_id_buf;
    state.mqtt_client_info.keep_alive = MQTT_KEEP_ALIVE_S;
    state.mqtt_client_info.client_user = MQTT_USERNAME;
    state.mqtt_client_info.client_pass = MQTT_PASSWORD;

    static char will_topic[MQTT_TOPIC_LEN];
    strncpy(will_topic, full_topic(&state, MQTT_WILL_TOPIC), sizeof(will_topic));
    state.mqtt_client_info.will_topic = will_topic;
    state.mqtt_client_info.will_msg = MQTT_WILL_MSG;
    state.mqtt_client_info.will_qos = MQTT_WILL_QOS;
    state.mqtt_client_info.will_retain = true;

    cyw43_arch_enable_sta_mode();
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000))
    {
        printf("Error al conectar a la red Wi-Fi.\n");
        return 1;
    }
    printf("Conectado a la red Wi-Fi.\n");

    cyw43_arch_lwip_begin();
    dns_gethostbyname(MQTT_SERVER, &state.mqtt_server_address, dns_found, &state);
    cyw43_arch_lwip_end();

    uint32_t last_publish_time = 0;

    while (1)
    {
        cyw43_arch_poll();
        cyw43_arch_wait_for_work_until(make_timeout_time_ms(1000));

        if (state.connect_done)
        {
            uint32_t now = to_ms_since_boot(get_absolute_time());
            if (now - last_publish_time > 10000)
            {
                last_publish_time = now;
                mqtt_publish_message(&state, "esp32/ping", "hola mundo");
            }
        }
    }

    printf("Saliendo del cliente MQTT.\n");
    return 0;
}