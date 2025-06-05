#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "lib/ssd1306.h"
#include "lib/neopixel.h"

#include "pico/bootrom.h"
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"   
#include "hardware/timer.h"
#include "configMQTT.h"



//====================================
//    CONFIGURAÇÃO DE WIFI E MQTT        
//===================================

#define WIFI_SSID "SEU_SSID"                  // Substitua pelo nome da sua rede Wi-Fi
#define WIFI_PASSWORD "SEU_PASSORD_WIFI"      // Substitua pela senha da sua rede Wi-Fi
#define MQTT_SERVER "SEU_HOST"                // Substitua pelo endereço do host - broket MQTT: Ex: 192.168.1.107
#define MQTT_USERNAME "SEU_USERNAME_MQTT"     // Substitua pelo nome da host MQTT - Username
#define MQTT_PASSWORD "SEU_PASSWORD_MQTT"     // Substitua pelo Password da host MQTT - credencial de acesso - caso exista

#ifndef MQTT_SERVER
#error Need to define MQTT_SERVER
#endif

static MQTT_CLIENT_DATA_T state;
static char client_id_buf[sizeof(MQTT_DEVICE_NAME) + 6];

//====================================
//      Definição de Periféricos        
//===================================

// Estrutura do display OLED
static ssd1306_t ssd; 

// Definição dos Pinos
#define BUTTON_A      5
#define BUTTON_B      6
#define BUTTON_JOYSTICK 22
#define BUZZER_PIN    21

// Tratador central de interrupções de botões
uint64_t last_time_button_a = 0;
uint64_t last_time_button_b = 0;
uint64_t last_time_button_joystick = 0;
static void gpio_button_handler(uint gpio, uint32_t events);

// Configura os periféricos que serão utilizados
void setup();

//====================================
//   Variaveis e Funções do Programa        
//===================================

#define LAMPADA_QUARTO 1
#define LAMPADA_SALA 2
#define LAMPADA_COZINHA 3

typedef struct {
    bool ligada;
    int r, g, b;
} Lampada;

Lampada lampadas[4]; // índice 1 a 3 (quarto, sala, cozinha)

// Inicializa lampadas como desligadas e cor amarela
void inicializa_lampadas();

// Atualiza as lampada especifica e publica no topico de acordo com o que foi recebido
void atualiza_lampadas(MQTT_CLIENT_DATA_T *state, Lampada *lampada, uint16_t lampada_id);

volatile uint16_t ar_condicionado_valor = 25; //Temperatura inicial
volatile bool ar_condicionado_ligado = false; // Inicialmente desligado

#define LIMITE_AR_INF 16
#define LIMITE_AR_SUP 30

volatile bool flag_atualiza_ar = false;

// Função para atualizar o display e topico do ar condicionado de acordo com os valores recebidos
void atualiza_ar_condicionado(MQTT_CLIENT_DATA_T *state, int valor, bool estado);

//====================================
//      Main, Setup e Handler       
//===================================

// Main
int main()
{
    setup();

    initialize_mqtt_client(&state, client_id_buf);
    inicializa_lampadas();
    atualiza_ar_condicionado(&state, ar_condicionado_valor, ar_condicionado_ligado);

    while (!state.connect_done || mqtt_client_is_connected(state.mqtt_client_inst)) {
        cyw43_arch_poll();
        cyw43_arch_wait_for_work_until(make_timeout_time_ms(10000));

        if(flag_atualiza_ar){
            flag_atualiza_ar = false;
            atualiza_ar_condicionado(&state, ar_condicionado_valor, ar_condicionado_ligado);
        }

    }
}

// Tratador central de interrupções de botões
static void gpio_button_handler(uint gpio, uint32_t events) {
    uint64_t current_time = to_ms_since_boot(get_absolute_time());

    // Verifica qual botão gerou a interrupção e aplica debounce (200ms)
    if(gpio == BUTTON_A && current_time - last_time_button_a >= 200){
        last_time_button_a = current_time;
        
        if(ar_condicionado_valor > 16 && ar_condicionado_ligado){
            ar_condicionado_valor--;
            flag_atualiza_ar = true;
        }

    }
    else if(gpio == BUTTON_B && current_time - last_time_button_b >= 200){
        
        last_time_button_b = current_time;
        
        if(ar_condicionado_valor < 30 & ar_condicionado_ligado){
            ar_condicionado_valor++;
            flag_atualiza_ar = true;
        }


    }

     else if(gpio == BUTTON_JOYSTICK && current_time - last_time_button_joystick >= 200){
        
        last_time_button_joystick = current_time;

        ar_condicionado_ligado = !ar_condicionado_ligado;
        flag_atualiza_ar = true;
        
        /*
        printf("\nHABILITANDO O MODO GRAVAÇÃO\n");

        ssd1306_fill(&ssd, false);
        ssd1306_draw_string(&ssd, "  HABILITANDO", 5, 25);
        ssd1306_draw_string(&ssd, " MODO GRAVACAO", 5, 38);
        ssd1306_send_data(&ssd);

        reset_usb_boot(0, 0);
        */
    }
}

// Configuração dos Periéficos
void setup(){
    stdio_init_all();

    // Matriz de Leds
    npInit(LED_PIN);

    // Display OLED
    display_init(&ssd);

    // Configuração dos Botões
    gpio_init(BUTTON_A);
    gpio_set_dir(BUTTON_A, GPIO_IN);
    gpio_pull_up(BUTTON_A);

    gpio_init(BUTTON_B);
    gpio_set_dir(BUTTON_B, GPIO_IN);
    gpio_pull_up(BUTTON_B);

    gpio_init(BUTTON_JOYSTICK);
    gpio_set_dir(BUTTON_JOYSTICK, GPIO_IN);
    gpio_pull_up(BUTTON_JOYSTICK);

    // Configura interrupções
    gpio_set_irq_enabled_with_callback(BUTTON_A, GPIO_IRQ_EDGE_FALL, true, &gpio_button_handler);
    gpio_set_irq_enabled_with_callback(BUTTON_B, GPIO_IRQ_EDGE_FALL, true, &gpio_button_handler);
    gpio_set_irq_enabled_with_callback(BUTTON_JOYSTICK, GPIO_IRQ_EDGE_FALL, true, &gpio_button_handler);
}

//====================================
//      Funções do Programa       
//===================================

// Inicializa lampadas como desligadas e cor amarela
void inicializa_lampadas() {
    for (int i = LAMPADA_QUARTO; i <= LAMPADA_COZINHA; i++) {
        lampadas[i].ligada = false;
        lampadas[i].r = 255;
        lampadas[i].g = 255;
        lampadas[i].b = 0;
        
    }
}

// Atualiza as lampada especifica e publica no topico de acordo com o que foi recebido
void atualiza_lampadas(MQTT_CLIENT_DATA_T *state, Lampada *lampada, uint16_t lampada_id) {
    int r = 0, g = 0, b = 0;

    // Se a lâmpada estiver ligada, usa a cor armazenada; se desligada, fica preta (0,0,0)
    if (lampada->ligada) {
        r = lampada->r;
        g = lampada->g;
        b = lampada->b;
    }

    // Controla o LED físico de acordo com o ID da lâmpada
    switch (lampada_id) {
        case LAMPADA_QUARTO:
            npSetLED(npGetIndex(4, 2), r, g, b);
            break;
        case LAMPADA_SALA:
            npSetLED(npGetIndex(0, 2), r, g, b);
            break;
        case LAMPADA_COZINHA:
            npSetLED(npGetIndex(2, 2), r, g, b);
            break;
        default:
            // Caso não seja nenhuma lâmpada conhecida, não faz nada
            break;
    }
    npWrite(); // Escreve na matriz

    // Define o nome do tópico MQTT para cada lâmpada, usado na publicação
    const char *nome;
    if (lampada_id == LAMPADA_QUARTO) {
        nome = "quarto";
    } else if (lampada_id == LAMPADA_SALA) {
        nome = "sala";
    } else if (lampada_id == LAMPADA_COZINHA) {
        nome = "cozinha";
    } else {
        nome = "desconhecido";
    }

    // Monta o tópico MQTT para o estado (ligado/desligado) da lâmpada e publica
    char topico_estado[64];
    snprintf(topico_estado, sizeof(topico_estado), "/lampada/%s/state", nome);
    mqtt_publish(state->mqtt_client_inst, topico_estado,
                 lampada->ligada ? "On" : "Off", lampada->ligada ? 2 : 3,
                 MQTT_PUBLISH_QOS, MQTT_PUBLISH_RETAIN, pub_request_cb, state);

    // Monta o tópico MQTT para a cor da lâmpada e publica a cor atual (mesmo se desligada)
    char topico_cor[64];
    snprintf(topico_cor, sizeof(topico_cor), "/lampada/%s/cor", nome);
    char rgb_str[16];
    snprintf(rgb_str, sizeof(rgb_str), "%d,%d,%d", lampada->r, lampada->g, lampada->b);
    mqtt_publish(state->mqtt_client_inst, topico_cor,
                 rgb_str, strlen(rgb_str),
                 MQTT_PUBLISH_QOS, MQTT_PUBLISH_RETAIN, pub_request_cb, state);
}

// Função para atualizar o display e topico do ar condicionado de acordo com os valores recebidos
void atualiza_ar_condicionado(MQTT_CLIENT_DATA_T *state, int valor, bool estado) {
    ssd1306_fill(&ssd, 0); // Limpa a tela

    if (estado == 0) {
        // Ar-condicionado desligado
        ssd1306_draw_string(&ssd, "Ar-Condicionado", 4, 18);
        ssd1306_draw_string(&ssd, "Desligado", 32, 36);
        mqtt_publish(state->mqtt_client_inst, full_topic(state, "/ar/state"), "Off", strlen("Off"), MQTT_PUBLISH_QOS, MQTT_PUBLISH_RETAIN, pub_request_cb, state);
    } else {
        // Ar-condicionado ligado, exibe temperatura
        char texto[20];
        snprintf(texto, sizeof(texto), "%d C", valor);

        int len = strlen(texto);
        int x = (128 - (len * 8)) / 2;

        ssd1306_draw_string(&ssd, "Temperatura do", 8, 10);
        ssd1306_draw_string(&ssd, "Ar-Condicionado", 4, 28);
        ssd1306_draw_string(&ssd, texto, x, 46);

        // Publida no topico de estado e de temperatura
        mqtt_publish(state->mqtt_client_inst, full_topic(state, "/ar/state"), "On", strlen("On"), MQTT_PUBLISH_QOS, MQTT_PUBLISH_RETAIN, pub_request_cb, state);
        mqtt_publish(state->mqtt_client_inst, full_topic(state, "/ar/temperatura"), texto, strlen(texto), MQTT_PUBLISH_QOS, MQTT_PUBLISH_RETAIN, pub_request_cb, state);
    }


    ssd1306_send_data(&ssd);
}

//====================================
//    Funções para Inscrição e
//       Publicação MQTT
//====================================

// Converte uma cor hexadecimal (ex: "#FFAABB") em valores RGB
void hexParaRgb(const char *hex, int *r, int *g, int *b) {
    if (hex[0] == '#') hex++; // pula o '#'

    // Pega os dois dígitos de cada cor
    char rStr[3] = { hex[0], hex[1], '\0' };
    char gStr[3] = { hex[2], hex[3], '\0' };
    char bStr[3] = { hex[4], hex[5], '\0' };

    // Converte de string hexadecimal para inteiro
    *r = strtol(rStr, NULL, 16);
    *g = strtol(gStr, NULL, 16);
    *b = strtol(bStr, NULL, 16);
}

// Verifica se uma string representa uma cor hexadecimal válida (ex: "#FFAABB")
// Retorna true se for válida, false caso contrário.
bool eh_hex_color_valido(const char *str) {
    if (*str == '#') str++;

    for (int i = 0; i < 6; i++) {
        if (str[i] == '\0') return false;
        if (!isxdigit((unsigned char)str[i])) return false;
    }

    if (str[6] != '\0') return false;

    return true;
}

// Verifica se a string está no formato de função RGB (ex: "rgb(255,255,0)")
// Retorna true se o formato for reconhecido, false caso contrário.
bool eh_rgb_func_valido(const char *str) {
    return strncmp(str, "rgb(", 4) == 0 && strchr(str, ')') != NULL;
}

// Extrai os valores RGB de uma string no formato "rgb(r, g, b)"
// Retorna true se os valores forem extraídos com sucesso e estiverem no intervalo [0,255]
bool extrai_rgb_func(const char *str, int *r, int *g, int *b) {
    // Espera o formato "rgb(r, g, b)"
    if (!eh_rgb_func_valido(str)) return false;

    // Avança o ponteiro para pular "rgb("
    str += 4;

    // Usa sscanf para extrair os valores
    int rr, gg, bb;
    if (sscanf(str, "%d , %d , %d", &rr, &gg, &bb) == 3) {
        // Validação do intervalo
        if (rr < 0 || rr > 255 || gg < 0 || gg > 255 || bb < 0 || bb > 255)
            return false;

        *r = rr;
        *g = gg;
        *b = bb;
        return true;
    }

    return false;
}

// Retorna o ID da lâmpada com base no tópico recebido via MQTT.
// Ex: "/lampada/sala" → LAMPADA_SALA
// Retorna 0 se o tópico não corresponder a nenhum cômodo conhecido.
uint16_t lampada_id_por_topico(const char *topico) {
    if (strstr(topico, "quarto") != NULL) return LAMPADA_QUARTO;
    if (strstr(topico, "sala") != NULL) return LAMPADA_SALA;
    if (strstr(topico, "cozinha") != NULL) return LAMPADA_COZINHA;
    return 0; // inválido ou não encontrado
}

// Interpreta uma mensagem recebida via MQTT e atualiza o estado ou a cor da lâmpada.
// Suporta formatos hexadecimais e RGB, além dos comandos "On" e "Off".
void trata_mensagem_lampada(Lampada *lamp, const char *msg) {
    int r, g, b;

    if (eh_hex_color_valido(msg) && lamp->ligada) {
        hexParaRgb(msg, &r, &g, &b);
        lamp->r = r;
        lamp->g = g;
        lamp->b = b;
    } else if (eh_rgb_func_valido(msg) && extrai_rgb_func(msg, &r, &g, &b) && lamp->ligada) {
        lamp->r = r;
        lamp->g = g;
        lamp->b = b;
    } else if (lwip_stricmp(msg, "On") == 0) {
        lamp->ligada = true;
    } else if (lwip_stricmp(msg, "Off") == 0) {
        lamp->ligada = false;
    } else {
        printf("Mensagem inválida para lâmpada: %s\n", msg);
    }
}

// Controle do LED CYW43
static void control_led(MQTT_CLIENT_DATA_T *state, bool on) {
    // Publish state on /state topic and on/off led board
    const char* message = on ? "On" : "Off";
    if (on)
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
    else
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);

    mqtt_publish(state->mqtt_client_inst, full_topic(state, "/led/state"), message, strlen(message), MQTT_PUBLISH_QOS, MQTT_PUBLISH_RETAIN, pub_request_cb, state);
}

// Tópicos de assinatura
static void sub_unsub_topics(MQTT_CLIENT_DATA_T* state, bool sub) {
    mqtt_request_cb_t cb = sub ? sub_request_cb : unsub_request_cb;
    mqtt_sub_unsub(state->mqtt_client_inst, full_topic(state, "/lampada/quarto"), MQTT_SUBSCRIBE_QOS, cb, state, sub);
    mqtt_sub_unsub(state->mqtt_client_inst, full_topic(state, "/lampada/sala"), MQTT_SUBSCRIBE_QOS, cb, state, sub);
    mqtt_sub_unsub(state->mqtt_client_inst, full_topic(state, "/lampada/cozinha"), MQTT_SUBSCRIBE_QOS, cb, state, sub);
    mqtt_sub_unsub(state->mqtt_client_inst, full_topic(state, "/ar"), MQTT_SUBSCRIBE_QOS, cb, state, sub);
    mqtt_sub_unsub(state->mqtt_client_inst, full_topic(state, "/led"), MQTT_SUBSCRIBE_QOS, cb, state, sub);
    mqtt_sub_unsub(state->mqtt_client_inst, full_topic(state, "/print"), MQTT_SUBSCRIBE_QOS, cb, state, sub);
    mqtt_sub_unsub(state->mqtt_client_inst, full_topic(state, "/ping"), MQTT_SUBSCRIBE_QOS, cb, state, sub);
    mqtt_sub_unsub(state->mqtt_client_inst, full_topic(state, "/exit"), MQTT_SUBSCRIBE_QOS, cb, state, sub);

}

// Dados de entrada MQTT
static void mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags) {
    MQTT_CLIENT_DATA_T* state = (MQTT_CLIENT_DATA_T*)arg;
#if MQTT_UNIQUE_TOPIC
    const char *basic_topic = state->topic + strlen(state->mqtt_client_info.client_id) + 1;
#else
    const char *basic_topic = state->topic;
#endif
    strncpy(state->data, (const char *)data, len);
    state->len = len;
    state->data[len] = '\0';

    DEBUG_printf("Topic: %s, Message: %s\n", state->topic, state->data);
    printf("Topic: %s, Message: %s\n", state->topic, state->data);


    if (strncmp(basic_topic, "/lampada/", 9) == 0) {
        uint16_t lamp_id = lampada_id_por_topico(basic_topic);

        if (lamp_id != 0) {
            trata_mensagem_lampada(&lampadas[lamp_id], state->data);
            atualiza_lampadas(state, &lampadas[lamp_id], lamp_id);
        } else {
            printf("Lâmpada desconhecida para tópico %s\n", basic_topic);
        }
    }
    else if(strcmp(basic_topic, "/ar") == 0){
        if (lwip_stricmp((const char *)state->data, "On") == 0) ar_condicionado_ligado = true; // Liga o ar
        else if (lwip_stricmp((const char *)state->data, "Off") == 0) ar_condicionado_ligado = false; // Desliga o ar
        // Se foi um valor númerico
        else{
            char *endptr; 
            int valor = strtol((const char *)state->data, &endptr, 10); // Converte o valor da string para inteiro se possivel

            // Verifica se a conversão foi bem-sucedida, se está no intervalo e se o Ar está ligado
            if (*endptr == '\0' && valor >= LIMITE_AR_INF && valor <= LIMITE_AR_SUP && ar_condicionado_ligado) {
                ar_condicionado_valor = valor;
            }
        }
        flag_atualiza_ar = true; // Pede para atualizar o display
    }
    else if (strcmp(basic_topic, "/led") == 0)
    {
        if (lwip_stricmp((const char *)state->data, "On") == 0 || strcmp((const char *)state->data, "1") == 0)
            control_led(state, true);
        else if (lwip_stricmp((const char *)state->data, "Off") == 0 || strcmp((const char *)state->data, "0") == 0)
            control_led(state, false);
    } else if (strcmp(basic_topic, "/print") == 0) {
        INFO_printf("%.*s\n", len, data);
    } else if (strcmp(basic_topic, "/ping") == 0) {
        char buf[11];
        snprintf(buf, sizeof(buf), "%u", to_ms_since_boot(get_absolute_time()) / 1000);
        mqtt_publish(state->mqtt_client_inst, full_topic(state, "/uptime"), buf, strlen(buf), MQTT_PUBLISH_QOS, MQTT_PUBLISH_RETAIN, pub_request_cb, state);
    } else if (strcmp(basic_topic, "/exit") == 0) {
        state->stop_client = true; // stop the client when ALL subscriptions are stopped
        sub_unsub_topics(state, false); // unsubscribe
    }


    
}


//====================================
//      CONFIGURAÇÃO DO MQTT       
//===================================

// Requisição para publicar
static void pub_request_cb(__unused void *arg, err_t err) {
    if (err != 0) {
        ERROR_printf("pub_request_cb failed %d", err);
    }
}

//Topico MQTT
static const char *full_topic(MQTT_CLIENT_DATA_T *state, const char *name) {
#if MQTT_UNIQUE_TOPIC
    static char full_topic[MQTT_TOPIC_LEN];
    snprintf(full_topic, sizeof(full_topic), "/%s%s", state->mqtt_client_info.client_id, name);
    return full_topic;
#else
    return name;
#endif
}

// Requisição de Assinatura - subscribe
static void sub_request_cb(void *arg, err_t err) {
    MQTT_CLIENT_DATA_T* state = (MQTT_CLIENT_DATA_T*)arg;
    if (err != 0) {
        panic("subscribe request failed %d", err);
    }
    state->subscribe_count++;
}

// Requisição para encerrar a assinatura
static void unsub_request_cb(void *arg, err_t err) {
    MQTT_CLIENT_DATA_T* state = (MQTT_CLIENT_DATA_T*)arg;
    if (err != 0) {
        panic("unsubscribe request failed %d", err);
    }
    state->subscribe_count--;
    assert(state->subscribe_count >= 0);

    // Stop if requested
    if (state->subscribe_count <= 0 && state->stop_client) {
        mqtt_disconnect(state->mqtt_client_inst);
    }
}

// Dados de entrada publicados
static void mqtt_incoming_publish_cb(void *arg, const char *topic, u32_t tot_len) {
    MQTT_CLIENT_DATA_T* state = (MQTT_CLIENT_DATA_T*)arg;
    strncpy(state->topic, topic, sizeof(state->topic));
}

// Conexão MQTT
static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status) {
    MQTT_CLIENT_DATA_T* state = (MQTT_CLIENT_DATA_T*)arg;
    if (status == MQTT_CONNECT_ACCEPTED) {
        state->connect_done = true;
        sub_unsub_topics(state, true); // subscribe;

        // indicate online
        if (state->mqtt_client_info.will_topic) {
            mqtt_publish(state->mqtt_client_inst, state->mqtt_client_info.will_topic, "1", 1, MQTT_WILL_QOS, true, pub_request_cb, state);
        }

        // Publish temperature every 10 sec if it's changed
    } else if (status == MQTT_CONNECT_DISCONNECTED) {
        if (!state->connect_done) {
            panic("Failed to connect to mqtt server");
        }
    }
    else {
        panic("Unexpected status");
    }
}

// Inicializar o cliente MQTT
static void start_client(MQTT_CLIENT_DATA_T *state) {
#if LWIP_ALTCP && LWIP_ALTCP_TLS
    const int port = MQTT_TLS_PORT;
    INFO_printf("Using TLS\n");
#else
    const int port = MQTT_PORT;
    INFO_printf("Warning: Not using TLS\n");
#endif

    state->mqtt_client_inst = mqtt_client_new();
    if (!state->mqtt_client_inst) {
        panic("MQTT client instance creation error");
    }
    INFO_printf("IP address of this device %s\n", ipaddr_ntoa(&(netif_list->ip_addr)));
    INFO_printf("Connecting to mqtt server at %s\n", ipaddr_ntoa(&state->mqtt_server_address));

    cyw43_arch_lwip_begin();
    if (mqtt_client_connect(state->mqtt_client_inst, &state->mqtt_server_address, port, mqtt_connection_cb, state, &state->mqtt_client_info) != ERR_OK) {
        panic("MQTT broker connection error");
    }
#if LWIP_ALTCP && LWIP_ALTCP_TLS
    // This is important for MBEDTLS_SSL_SERVER_NAME_INDICATION
    mbedtls_ssl_set_hostname(altcp_tls_context(state->mqtt_client_inst->conn), MQTT_SERVER);
#endif
    mqtt_set_inpub_callback(state->mqtt_client_inst, mqtt_incoming_publish_cb, mqtt_incoming_data_cb, state);
    cyw43_arch_lwip_end();
}

// Call back com o resultado do DNS
static void dns_found(const char *hostname, const ip_addr_t *ipaddr, void *arg) {
    MQTT_CLIENT_DATA_T *state = (MQTT_CLIENT_DATA_T*)arg;
    if (ipaddr) {
        state->mqtt_server_address = *ipaddr;
        start_client(state);
    } else {
        panic("dns request failed");
    }
}

// Função para inicializar o cliente em uma única chamada
static void initialize_mqtt_client(MQTT_CLIENT_DATA_T *state, char *client_id_buf) {
    // 1. Inicializa hardware WiFi
    if (cyw43_arch_init()) {
        panic("Falha ao inicializar CYW43");
    }

    char unique_id[5];
    pico_get_unique_board_id_string(unique_id, sizeof(unique_id));
    for(int i = 0; i < sizeof(unique_id) - 1; i++) {
        unique_id[i] = tolower(unique_id[i]);
    }

    snprintf(client_id_buf, sizeof(client_id_buf), "%s%s", MQTT_DEVICE_NAME, unique_id);
    INFO_printf("Nome do dispositivo: %s\n", client_id_buf);
    printf("Nome do dispositivo: %s\n", client_id_buf);

    state->mqtt_client_info.client_id = client_id_buf;
    state->mqtt_client_info.keep_alive = MQTT_KEEP_ALIVE_S;

    #if defined(MQTT_USERNAME) && defined(MQTT_PASSWORD)
        state->mqtt_client_info.client_user = MQTT_USERNAME;
        state->mqtt_client_info.client_pass = MQTT_PASSWORD;
    #else
        state->mqtt_client_info.client_user = NULL;
        state->mqtt_client_info.client_pass = NULL;
    #endif  

        static char will_topic[MQTT_TOPIC_LEN];
        strncpy(will_topic, full_topic(state, MQTT_WILL_TOPIC), sizeof(will_topic));
        state->mqtt_client_info.will_topic = will_topic;
        state->mqtt_client_info.will_msg = MQTT_WILL_MSG;
        state->mqtt_client_info.will_qos = MQTT_WILL_QOS;
        state->mqtt_client_info.will_retain = true;

    #if LWIP_ALTCP && LWIP_ALTCP_TLS
    #ifdef MQTT_CERT_INC
    static const uint8_t ca_cert[] = TLS_ROOT_CERT;
    static const uint8_t client_key[] = TLS_CLIENT_KEY;
    static const uint8_t client_cert[] = TLS_CLIENT_CERT;
    state->mqtt_client_info.tls_config = altcp_tls_create_config_client_2wayauth(
        ca_cert, sizeof(ca_cert),
        client_key, sizeof(client_key),
        NULL, 0,
        client_cert, sizeof(client_cert)
    );
    #if ALTCP_MBEDTLS_AUTHMODE != MBEDTLS_SSL_VERIFY_REQUIRED
    WARN_printf("Aviso: TLS sem verificação é inseguro\n");
    #endif
    #else
    state->mqtt_client_info.tls_config = altcp_tls_create_config_client(NULL, 0);
    WARN_printf("Aviso: TLS sem certificado é inseguro\n");
    #endif
    #endif

    // 7. Conecta ao WiFi
    cyw43_arch_enable_sta_mode();
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, 
                                          CYW43_AUTH_WPA2_AES_PSK, 30000)) {
        panic("Falha na conexão WiFi");
    }
    INFO_printf("\nConectado ao WiFi\n");

    // 8. Resolução DNS do servidor MQTT
    cyw43_arch_lwip_begin();
    int err = dns_gethostbyname(MQTT_SERVER, &state->mqtt_server_address, dns_found, state);
    cyw43_arch_lwip_end();

    // 9. Inicia cliente se DNS resolvido
    if (err == ERR_OK) {
        start_client(state);
    } else if (err != ERR_INPROGRESS) {
        panic("Falha na requisição DNS");
    }
}