#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h> // Para comunicação UDP
#include "mbedtls/aes.h" // Para criptografia AES
#include <cjson/cJSON.h>       // Para leitura de arquivos JSON
#include <unistd.h> // Para a função close()

// Estrutura para armazenar os parâmetros do dispositivo
typedef struct {
    char devEUI[17];  // DevEUI (16 caracteres + \0)
    char appEUI[17];  // AppEUI (16 caracteres + \0)
    char appKey[33];  // AppKey (32 caracteres + \0)
    char mode[4];     // Modo: "OTAA" ou "ABP"
    char gateway_ip[16]; // Endereço IP do gateway
    int gateway_port; // Porta do gateway
} DeviceConfig;

// Função para ler a configuração do arquivo JSON
DeviceConfig *read_config(const char *filename, int *num_devices) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Erro ao abrir arquivo de configuração");
        exit(EXIT_FAILURE);
    }

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);
    char *buffer = malloc(length + 1);
    fread(buffer, 1, length, file);
    fclose(file);

    cJSON *json = cJSON_Parse(buffer);
    free(buffer);

    if (!json || !cJSON_IsArray(json)) {
        fprintf(stderr, "Arquivo de configuração inválido ou não é um array\n");
        exit(EXIT_FAILURE);
    }

    int size = cJSON_GetArraySize(json);
    DeviceConfig *devices = malloc(size * sizeof(DeviceConfig));

    for (int i = 0; i < size; i++) {
        cJSON *device = cJSON_GetArrayItem(json, i);
        cJSON *devEUI = cJSON_GetObjectItemCaseSensitive(device, "devEUI");
        cJSON *appEUI = cJSON_GetObjectItemCaseSensitive(device, "appEUI");
        cJSON *appKey = cJSON_GetObjectItemCaseSensitive(device, "appKey");
        cJSON *mode = cJSON_GetObjectItemCaseSensitive(device, "mode");
        cJSON *gateway_ip = cJSON_GetObjectItemCaseSensitive(device, "gateway_ip");
        cJSON *gateway_port = cJSON_GetObjectItemCaseSensitive(device, "gateway_port");

        if (cJSON_IsString(devEUI) && cJSON_IsString(appEUI) && cJSON_IsString(appKey) &&
            cJSON_IsString(mode) && cJSON_IsString(gateway_ip) && cJSON_IsNumber(gateway_port)) {
            strncpy(devices[i].devEUI, devEUI->valuestring, sizeof(devices[i].devEUI) - 1);
            strncpy(devices[i].appEUI, appEUI->valuestring, sizeof(devices[i].appEUI) - 1);
            strncpy(devices[i].appKey, appKey->valuestring, sizeof(devices[i].appKey) - 1);
            strncpy(devices[i].mode, mode->valuestring, sizeof(devices[i].mode) - 1);
            strncpy(devices[i].gateway_ip, gateway_ip->valuestring, sizeof(devices[i].gateway_ip) - 1);
            devices[i].gateway_port = gateway_port->valueint;
        } else {
            fprintf(stderr, "Dispositivo %d inválido no arquivo de configuração\n", i);
            exit(EXIT_FAILURE);
        }
    }

    cJSON_Delete(json);
    *num_devices = size;
    return devices;
}

// Função para criptografar o payload usando AES
void encrypt_payload(const char *key, const char *plaintext, unsigned char *output) {
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_enc(&aes, (const unsigned char *)key, strlen(key) * 8);

    unsigned char iv[16] = {0}; // Vetor de inicialização (IV)
    mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, strlen(plaintext), iv, (const unsigned char *)plaintext, output);

    mbedtls_aes_free(&aes);
}

// Função para enviar pacote via UDP
void send_udp_packet(const char *gateway_ip, int gateway_port, const unsigned char *payload, int payload_size) {
    int sockfd;
    struct sockaddr_in servaddr;

    // Criar socket UDP
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Erro ao criar socket");
        exit(EXIT_FAILURE);
    }

    // Configurar endereço do servidor (gateway)
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(gateway_port);
    servaddr.sin_addr.s_addr = inet_addr(gateway_ip);

    // Enviar pacote
    if (sendto(sockfd, payload, payload_size, 0, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("Erro ao enviar pacote UDP");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Pacote enviado para o gateway (%s:%d)\n", gateway_ip, gateway_port);

    // Fechar socket
    close(sockfd);
}

// Função principal
int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <arquivo_config.json>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int num_devices;
    DeviceConfig *devices = read_config(argv[1], &num_devices);

    // Simular envio de payload para cada dispositivo
    for (int i = 0; i < num_devices; i++) {
        printf("\nSimulando dispositivo %d:\n", i + 1);
        printf("DevEUI: %s\n", devices[i].devEUI);
        printf("AppEUI: %s\n", devices[i].appEUI);
        printf("AppKey: %s\n", devices[i].appKey);
        printf("Modo: %s\n", devices[i].mode);

        // Simular um payload simples
        const char *payload = "HelloLoRaWAN";
        unsigned char encrypted_payload[128] = {0};

        // Criptografar o payload
        encrypt_payload(devices[i].appKey, payload, encrypted_payload);

        // Exibir payload criptografado
        printf("\nPayload criptografado:\n");
        for (int j = 0; j < strlen(payload); j++) {
            printf("%02x ", encrypted_payload[j]);
        }
        printf("\n");

        // Enviar pacote criptografado para o gateway
        send_udp_packet(devices[i].gateway_ip, devices[i].gateway_port, encrypted_payload, strlen(payload));
    }

    free(devices);
    return EXIT_SUCCESS;
}
