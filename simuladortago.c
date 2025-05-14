//Simulador em C para enviar duas variáveis para o TAGO IO

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

// Função para enviar os dados via POST usando libcurl
void send_json_data(const char *url, const char *json_data, const char *token) {
    CURL *curl;
    CURLcode res;

    // Inicializa o objeto CURL
    curl = curl_easy_init();
    if (curl) {
        // Define a URL para a qual a requisição será enviada
        curl_easy_setopt(curl, CURLOPT_URL, url);

        // Define os cabeçalhos da requisição
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        headers = curl_slist_append(headers, token); // Adiciona o token de autorização
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        // Define os dados JSON a serem enviados
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data);

        // Realiza a requisição HTTP POST
        res = curl_easy_perform(curl);

        // Verifica se houve erro na requisição
        if (res != CURLE_OK) {
            fprintf(stderr, "Erro ao enviar dados: %s\n", curl_easy_strerror(res));
        } else {
            printf("Dados enviados com sucesso!\n");
        }

        // Limpa os recursos utilizados
        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
    }
}

int main() {
    char temperatura[256], umidade[256];
    char json_data[1024];
    char url[256];
    char token[256];

    
    printf("Digite a temperatura: ");
    scanf("%s", temperatura);
    printf("Digite a umidade: ");
    scanf("%s", umidade);

    printf("Digite o token de autorização: ");
    scanf("%s", token);

    //URL
    strcpy(url, "https://api.tago.io/data");

    //JSON
    snprintf(json_data, sizeof(json_data),
             "[{\"variable\":\"temperatura\",\"value\":%s,\"unit\":\"°C\"},"
             "{\"variable\":\"umidade\",\"value\":%s,\"unit\":\"%%\"}]",
             temperatura, umidade);

    // Exibir JSON
    printf("Enviando JSON: %s\n", json_data);

    
    char authorization_header[512];
    snprintf(authorization_header, sizeof(authorization_header), "Authorization: %s", token);

    //POST
    send_json_data(url, json_data, authorization_header);

    return 0;
}
