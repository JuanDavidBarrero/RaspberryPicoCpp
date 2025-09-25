#ifndef PICO_MBEDTLS_CONFIG_H
#define PICO_MBEDTLS_CONFIG_H

// --- Configuración Esencial para Pico ---
#define MBEDTLS_NO_PLATFORM_ENTROPY
#define MBEDTLS_ENTROPY_HARDWARE_ALT
#define MBEDTLS_HAVE_TIME
#define MBEDTLS_PLATFORM_MS_TIME_ALT

// --- Optimización de Memoria ---
#define MBEDTLS_SSL_OUT_CONTENT_LEN    (4 * 1024)
#define MBEDTLS_AES_FEWER_TABLES

// --- Módulos Principales ---
#define MBEDTLS_PLATFORM_C
#define MBEDTLS_ERROR_C
#define MBEDTLS_BIGNUM_C
#define MBEDTLS_CIPHER_C
#define MBEDTLS_CTR_DRBG_C
#define MBEDTLS_ENTROPY_C
#define MBEDTLS_MD_C
#define MBEDTLS_PK_C
#define MBEDTLS_PK_PARSE_C
#define MBEDTLS_SSL_CLI_C
#define MBEDTLS_SSL_TLS_C
#define MBEDTLS_X509_CRT_PARSE_C

// --- Protocolo TLS y SNI (Crucial) ---
#define MBEDTLS_SSL_PROTO_TLS1_2
#define MBEDTLS_SSL_SERVER_NAME_INDICATION

// --- Algoritmos de Cifrado y Hash ---
#define MBEDTLS_AES_C
#define MBEDTLS_GCM_C
#define MBEDTLS_SHA256_C
#define MBEDTLS_SHA512_C

// --- Módulos de Criptografía de Curva Elíptica (ECC - ¡La Clave!) ---
#define MBEDTLS_ASN1_PARSE_C
#define MBEDTLS_ECP_C
#define MBEDTLS_ECDH_C
#define MBEDTLS_ECDSA_C
#define MBEDTLS_ECP_NIST_OPTIM

// --- Habilitar los Intercambios de Claves que necesitamos ---
#define MBEDTLS_KEY_EXCHANGE_ECDHE_RSA_ENABLED
#define MBEDTLS_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED

// --- Habilitar Curvas Específicas (Importante para compatibilidad) ---
#define MBEDTLS_ECP_DP_SECP256R1_ENABLED
#define MBEDTLS_ECP_DP_SECP384R1_ENABLED
#define MBEDTLS_ECP_DP_SECP521R1_ENABLED

#include "mbedtls/check_config.h"

#endif