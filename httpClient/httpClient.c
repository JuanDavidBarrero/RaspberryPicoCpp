#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "lwip/pbuf.h"
#include "lwip/tcp.h"
#include "lwip/dns.h"

#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/ssl.h"
#include "mbedtls/x509_crt.h"
#include "mbedtls/error.h"
#include "mbedtls/debug.h"

// --- Configuración del Servidor y API Key ---
#define TCP_SERVER_HOST "reqres.in"
#define API_KEY "reqres-free-v1"
#define HTTPS_SERVER_PORT 443

// --- Peticiones HTTP ---
#define HTTP_GET_REQUEST "GET /api/users/2 HTTP/1.1\r\n" \
                         "Host: " TCP_SERVER_HOST "\r\n" \
                         "x-api-key: " API_KEY "\r\n" \
                         "User-Agent: pico-https-client/1.0\r\n" \
                         "Connection: close\r\n" \
                         "\r\n"

#define POST_BODY "{\"name\": \"morpheus\", \"job\": \"leader\"}"
#define HTTP_POST_REQUEST "POST /api/users HTTP/1.1\r\n" \
                          "Host: " TCP_SERVER_HOST "\r\n" \
                          "x-api-key: " API_KEY "\r\n" \
                          "User-Agent: pico-https-client/1.0\r\n" \
                          "Content-Type: application/json\r\n" \
                          "Content-Length: " "35" "\r\n" \
                          "Connection: close\r\n" \
                          "\r\n" \
                          POST_BODY

typedef struct HTTPS_CLIENT_T_ {
    struct tcp_pcb *tcp_pcb;
    ip_addr_t remote_addr;
    const char *request;
    int sent_len;
    bool complete;
    bool handshake_done;

    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
} HTTPS_CLIENT_T;

static err_t https_client_close(void *arg);
static err_t https_client_connected(void *arg, struct tcp_pcb *tpcb, err_t err);
static bool https_client_init(HTTPS_CLIENT_T *state);
static void dns_found(const char *hostname, const ip_addr_t *ipaddr, void *arg);
static err_t https_client_sent(void *arg, struct tcp_pcb *tpcb, u16_t len);
static err_t https_client_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);

// ========= INICIO CÓDIGO DE DEPURACIÓN =========
static void mbedtls_debug_callback(void *ctx, int level,
                                   const char *file, int line,
                                   const char *str) {
    ((void)ctx);
    printf("mbedtls_debug: [%d] %s:%d: %s", level, file, line, str);
}
// ========= FIN CÓDIGO DE DEPURACIÓN =========

static int https_client_send_request(HTTPS_CLIENT_T *state) {
    printf("Sending request...\n");
    int ret = 0;
    int remaining = strlen(state->request) - state->sent_len;
    while (remaining > 0 && (ret = mbedtls_ssl_write(&state->ssl, (const unsigned char*)state->request + state->sent_len, remaining)) > 0) {
        state->sent_len += ret;
        remaining -= ret;
    }
    if (ret < 0 && ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
        printf("mbedtls_ssl_write returned -0x%x\n", -ret);
        return ret;
    }
    printf("Sent %d bytes of request\n", state->sent_len);
    return 0;
}

static err_t https_client_close(void *arg) {
    HTTPS_CLIENT_T *state = (HTTPS_CLIENT_T *)arg;
    if (state->complete) return ERR_OK;
    state->complete = true;
    mbedtls_ssl_free(&state->ssl);
    mbedtls_ssl_config_free(&state->conf);
    mbedtls_ctr_drbg_free(&state->ctr_drbg);
    mbedtls_entropy_free(&state->entropy);
    if (state->tcp_pcb != NULL) {
        tcp_arg(state->tcp_pcb, NULL);
        tcp_poll(state->tcp_pcb, NULL, 0);
        tcp_sent(state->tcp_pcb, NULL);
        tcp_recv(state->tcp_pcb, NULL);
        tcp_err(state->tcp_pcb, NULL);
        err_t err = tcp_close(state->tcp_pcb);
        if (err != ERR_OK) {
            tcp_abort(state->tcp_pcb);
            return ERR_ABRT;
        }
    }
    return ERR_OK;
}

static err_t https_client_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    HTTPS_CLIENT_T *state = (HTTPS_CLIENT_T *)arg;
    if (!p) {
        printf("Connection closed by server (recv callback).\n");
        return https_client_close(arg);
    }
    tcp_recved(tpcb, p->tot_len);
    pbuf_free(p);

    if (!state->handshake_done) {
        int ret = mbedtls_ssl_handshake(&state->ssl);
        if (ret == 0) {
            state->handshake_done = true;
            printf("TLS Handshake successful.\n");
            https_client_send_request(state);
        } else if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
            printf("mbedtls_ssl_handshake failed in recv with -0x%x\n", -ret);
            return https_client_close(arg);
        }
        return ERR_OK;
    }
    unsigned char buffer[2048];
    int ret;
    printf("--- Response ---\n");
    while ((ret = mbedtls_ssl_read(&state->ssl, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[ret] = 0;
        printf("%s", buffer);
    }
    printf("\n---------------\n");
    if (ret != MBEDTLS_ERR_SSL_WANT_READ) {
       return https_client_close(arg);
    }
    return ERR_OK;
}

static err_t https_client_sent(void *arg, struct tcp_pcb *tpcb, u16_t len) {
    HTTPS_CLIENT_T *state = (HTTPS_CLIENT_T *)arg;
    if (!state->handshake_done) {
        int ret = mbedtls_ssl_handshake(&state->ssl);
        if (ret == 0) {
            state->handshake_done = true;
            printf("TLS Handshake successful (from sent).\n");
            https_client_send_request(state);
        } else if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
            printf("mbedtls_ssl_handshake failed in sent with -0x%x\n", -ret);
            https_client_close(arg);
        }
    } else if (state->sent_len < strlen(state->request)) {
        https_client_send_request(state);
    }
    return ERR_OK;
}

static err_t https_client_connected(void *arg, struct tcp_pcb *tpcb, err_t err) {
    HTTPS_CLIENT_T *state = (HTTPS_CLIENT_T *)arg;
    if (err != ERR_OK) {
        printf("connect failed %d\n", err);
        return https_client_close(arg);
    }
    printf("TCP connected, starting TLS handshake...\n");
    int ret = mbedtls_ssl_handshake(&state->ssl);
    if (ret != 0 && ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
        printf("mbedtls_ssl_handshake failed in connect with -0x%x\n", -ret);
        return https_client_close(arg);
    }
    return ERR_OK;
}

static int bio_send(void *ctx, const unsigned char *buf, size_t len) {
    HTTPS_CLIENT_T *state = (HTTPS_CLIENT_T *)ctx;
    err_t err = tcp_write(state->tcp_pcb, buf, len, TCP_WRITE_FLAG_COPY);
    if (err == ERR_MEM) return MBEDTLS_ERR_SSL_WANT_WRITE;
    if (err != ERR_OK) return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
    return len;
}

static int bio_recv(void *ctx, unsigned char *buf, size_t len) {
    return MBEDTLS_ERR_SSL_WANT_READ;
}

static bool https_client_init(HTTPS_CLIENT_T *state) {
    mbedtls_ssl_init(&state->ssl);
    mbedtls_ssl_config_init(&state->conf);
    mbedtls_ctr_drbg_init(&state->ctr_drbg);
    mbedtls_entropy_init(&state->entropy);

    if (mbedtls_ctr_drbg_seed(&state->ctr_drbg, mbedtls_entropy_func, &state->entropy, NULL, 0) != 0) return false;
    if (mbedtls_ssl_config_defaults(&state->conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT) != 0) return false;
    
    mbedtls_ssl_conf_authmode(&state->conf, MBEDTLS_SSL_VERIFY_OPTIONAL);
    mbedtls_ssl_conf_rng(&state->conf, mbedtls_ctr_drbg_random, &state->ctr_drbg);

    if (mbedtls_ssl_setup(&state->ssl, &state->conf) != 0) return false;
    if (mbedtls_ssl_set_hostname(&state->ssl, TCP_SERVER_HOST) != 0) return false;
    
    mbedtls_ssl_set_bio(&state->ssl, state, bio_send, bio_recv, NULL);
    
    // ========= INICIO CÓDIGO DE DEPURACIÓN =========
    // Activa la función de callback para todos los mensajes de depuración (nivel 4)
    mbedtls_ssl_conf_dbg(&state->conf, mbedtls_debug_callback, NULL);
    mbedtls_debug_set_threshold(4);
    // ========= FIN CÓDIGO DE DEPURACIÓN =========
    
    return true;
}

static void dns_found(const char *hostname, const ip_addr_t *ipaddr, void *arg) {
    HTTPS_CLIENT_T *state = (HTTPS_CLIENT_T *)arg;
    if (ipaddr) {
        printf("Resolved hostname %s to %s\n", hostname, ip4addr_ntoa(ipaddr));
        state->remote_addr = *ipaddr;
        state->tcp_pcb = tcp_new_ip_type(IP_GET_TYPE(&state->remote_addr));
        if (!state->tcp_pcb) {
            printf("failed to create pcb\n"); state->complete = true; return;
        }
        tcp_arg(state->tcp_pcb, state);
        tcp_sent(state->tcp_pcb, https_client_sent);
        tcp_recv(state->tcp_pcb, https_client_recv);
        
        if (!https_client_init(state)) {
            printf("https client init failed\n"); state->complete = true; return;
        }
        tcp_connect(state->tcp_pcb, &state->remote_addr, HTTPS_SERVER_PORT, https_client_connected);
    } else {
        printf("dns request failed\n"); state->complete = true;
    }
}

static bool run_https_request(const char *request) {
    HTTPS_CLIENT_T *state = calloc(1, sizeof(HTTPS_CLIENT_T));
    if (!state) return false;
    state->request = request;

    cyw43_arch_lwip_begin();
    err_t err = dns_gethostbyname(TCP_SERVER_HOST, &state->remote_addr, dns_found, state);
    cyw43_arch_lwip_end();

    if (err != ERR_OK && err != ERR_INPROGRESS) {
        printf("dns request failed %d\n", err);
        free(state);
        return false;
    }
    while (!state->complete) {
        cyw43_arch_poll();
        sleep_ms(1);
    }
    free(state);
    sleep_ms(100); 
    return true;
}

int main() {
    stdio_init_all();
    if (cyw43_arch_init()) {
        printf("failed to initialise\n");
        return 1;
    }
    cyw43_arch_enable_sta_mode();
    printf("Connecting to Wi-Fi...\n");
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000)) {
        printf("failed to connect.\n");
        return 1;
    } else {
        printf("Connected.\n");
    }

    printf("\n--- Performing GET request to https://%s ---\n", TCP_SERVER_HOST);
    run_https_request(HTTP_GET_REQUEST);

    printf("\n--- Performing POST request to https://%s ---\n", TCP_SERVER_HOST);
    run_https_request(HTTP_POST_REQUEST);

    cyw43_arch_deinit();
    return 0;
}