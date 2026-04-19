idf_component_register(
    SRCS "main.cpp" "assets/logoteclab.c"
    INCLUDE_DIRS "."
    EMBED_FILES "boot_sound.wav"
    REQUIRES nvs_flash mbedtls esp_http_client esp_port esp_wifi esp_netif
)