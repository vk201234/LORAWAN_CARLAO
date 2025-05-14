#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h> // Para comunicação UDP
#include <unistd.h>    // Para a função close()
#include <cjson/cJSON.h>  // Para leitura de arquivos JSON

// Estrutura do cabeçalho do pacote Semtech UDP
typedef struct {
    uint8_t version;       // Versão do protocolo (2)
    uint8_t token[2];      // Token aleatório
    uint8_t identifier[2]; // Identificador do tipo de mensagem (PUSH_DATA = 0x0001)
    uint8_t gateway_id[8]; // ID único do gateway
} SemtechHeader;

// Estrutura para armazenar a configuração do gateway
typedef struct {
    char gateway_id[9]; // 8 caracteres + \0
    int gateway_port;
    char ttn_ip[16];
    int ttn_port;
} GatewayConfig;

// Função para ler a configuração do arquivo JSON
GatewayConfig read_config(const char *filename) {
    GatewayConfig config = {0};
    printf("Abrindo arquivo de configuração...\n");
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Erro ao abrir arquivo de configuração");
        exit(EXIT_FAILURE);
    }
    printf("Arquivo de configuração aberto com sucesso.\n");

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);
    char *buffer = malloc(length + 1);
    fread(buffer, 1, length, file);
    fclose(file);

    cJSON *json = cJSON_Parse(buffer);
    free(buffer);

    if (!json) {
        fprintf(stderr, "Erro ao analisar JSON\n");
        exit(EXIT_FAILURE);
    }

    cJSON *gateway_id = cJSON_GetObjectItemCaseSensitive(json, "gateway_id");
    cJSON *gateway_port = cJSON_GetObjectItemCaseSensitive(json, "gateway_port");
    cJSON *ttn_ip = cJSON_GetObjectItemCaseSensitive(json, "ttn_ip");
    cJSON *ttn_port = cJSON_GetObjectItemCaseSensitive(json, "ttn_port");

    if (cJSON_IsString(gateway_id) && cJSON_IsNumber(gateway_port) &&
        cJSON_IsString(ttn_ip) && cJSON_IsNumber(ttn_port)) {
        strncpy(config.gateway_id, gateway_id->valuestring, sizeof(config.gateway_id) - 1);
        config.gateway_port = gateway_port->valueint;
        strncpy(config.ttn_ip, ttn_ip->valuestring, sizeof(config.ttn_ip) - 1);
        config.ttn_port = ttn_port->valueint;
    } else {
        fprintf(stderr, "Arquivo de configuração inválido\n");
        exit(EXIT_FAILURE);
    }

    cJSON_Delete(json);
    return config;
}

// Função para gerar um token aleatório
void generate_random_token(uint8_t *token) {
    for (int i = 0; i < 2; i++) {
        token[i] = rand() % 256;
    }
}

// Função para enviar pacote ao TTN via UDP
void send_to_ttn(const char *ttn_ip, int ttn_port, const unsigned char *packet, int packet_size) {
    int sockfd;
    struct sockaddr_in servaddr;

    printf("Criando socket UDP...\n");
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Erro ao criar socket");
        exit(EXIT_FAILURE);
    }
    printf("Socket criado com sucesso.\n");

    // Configurar endereço do servidor (TTN)
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(ttn_port);
    servaddr.sin_addr.s_addr = inet_addr(ttn_ip);

    // Enviar pacote
    if (sendto(sockfd, packet, packet_size, 0, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("Erro ao enviar pacote UDP");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Pacote enviado para o TTN via UDP (%s:%d)\n", ttn_ip, ttn_port);

    // Fechar socket
    close(sockfd);
}

// Função principal
int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <arquivo_config.json>\n", argv[0]);
        return EXIT_FAILURE;
    }

    // Ler configuração do gateway
    GatewayConfig config = read_config(argv[1]);

    int sockfd;
    struct sockaddr_in servaddr, cliaddr;

    // Criar socket UDP
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Erro ao criar socket");
        exit(EXIT_FAILURE);
    }

    // Configurar endereço do servidor (gateway)
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(config.gateway_port);

    // Bind do socket
    if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("Erro ao fazer bind");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Emulador de gateway iniciado na porta %d com ID %s\n", config.gateway_port, config.gateway_id);

    while (1) {
        unsigned char buffer[1024];
        socklen_t len = sizeof(cliaddr);

        // Receber pacote UDP do dispositivo
        int n = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *)&cliaddr, &len);
        if (n < 0) {
            perror("Erro ao receber pacote UDP");
            continue;
        }

        printf("Pacote recebido de %s:%d\n", inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port));

        // Encapsular pacote no formato Semtech UDP
        unsigned char semtech_packet[1024];
        SemtechHeader header;

        header.version = 2;
        generate_random_token(header.token);
        header.identifier[0] = 0x00; // PUSH_DATA
        header.identifier[1] = 0x01;
        memcpy(header.gateway_id, config.gateway_id, 8);

        int packet_size = 0;

        // Copiar cabeçalho
        memcpy(semtech_packet, &header, sizeof(SemtechHeader));
        packet_size += sizeof(SemtechHeader);

        // Copiar payload recebido
        memcpy(semtech_packet + packet_size, buffer, n);
        packet_size += n;

        // Enviar pacote encapsulado para o TTN
        send_to_ttn(config.ttn_ip, config.ttn_port, semtech_packet, packet_size);
    }

    close(sockfd);
    return EXIT_SUCCESS;
}
