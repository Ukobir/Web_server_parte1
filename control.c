/**
 * AULA IoT - Embarcatech - Ricardo Prates - 004 - Webserver Raspberry Pi Pico w - wlan
 *
 * Material de suporte
 *
 * https://www.raspberrypi.com/documentation/pico-sdk/networking.html#group_pico_cyw43_arch_1ga33cca1c95fc0d7512e7fef4a59fd7475
 */

#include <stdio.h>  // Biblioteca padrão para entrada e saída
#include <string.h> // Biblioteca manipular strings
#include <stdlib.h> // funções para realizar várias operações, incluindo alocação de memória dinâmica (malloc)

#include "pico/stdlib.h"     // Biblioteca da Raspberry Pi Pico para funções padrão (GPIO, temporização, etc.)
#include "hardware/adc.h"    // Biblioteca da Raspberry Pi Pico para manipulação do conversor ADC
#include "pico/cyw43_arch.h" // Biblioteca para arquitetura Wi-Fi da Pico com CYW43

#include "lwip/pbuf.h"  // Lightweight IP stack - manipulação de buffers de pacotes de rede
#include "lwip/tcp.h"   // Lightweight IP stack - fornece funções e estruturas para trabalhar com o protocolo TCP
#include "lwip/netif.h" // Lightweight IP stack - fornece funções e estruturas para trabalhar com interfaces de rede (netif)

#include "lib/func.c"     //algumas funções uteis (USB)
#include "pico/bootrom.h" //biblioteca para o modo bootsel(utilizado no botão B)
#include "lib/ws2812.h"   //biblioteca para o controle PIO da Matriz de LED
#include "lib/ssd1306.h"  //biblioteca do ssd1306

// Credenciais WIFI - Tome cuidado se publicar no github!
#define WIFI_SSID "Coloque o nome da sua rede aqui"
#define WIFI_PASSWORD "Coloque o nome da sua senha aqui"

// Definição dos pinos dos LEDs

#define LED_PIN CYW43_WL_GPIO_LED_PIN // GPIO do CI CYW43
#define LED_BLUE_PIN 12               // GPIO12 - LED azul
#define LED_GREEN_PIN 11              // GPIO11 - LED verde
#define LED_RED_PIN 13                // GPIO13 - LED vermelho

#define botaoB 6

// Comando de instrução para o display
ssd1306_t ssd;

// Variáveis de controle do display
static bool q1 = 0;
static bool q2 = 0;
static bool q3 = 0;

// Inicializar os Pinos GPIO para acionamento dos LEDs da BitDogLab
void gpio_led_bitdog(void);

// Função de callback ao aceitar conexões TCP
static err_t tcp_server_accept(void *arg, struct tcp_pcb *newpcb, err_t err);

// Função de callback para processar requisições HTTP
static err_t tcp_server_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);

// Leitura da temperatura interna
float temp_read(void);

// Tratamento do request do usuário
void user_request(char **request);

// Interrução
void gpio_irq_handler(uint gpio, uint32_t events);

// Variáveis utilizada para o debouncing
static volatile uint32_t passado = 0;
static uint32_t tempo_atual;

// Função principal
int main()
{
    // Inicializa todos os tipos de bibliotecas stdio padrão presentes que estão ligados ao binário.
    stdio_init_all();

    // Inicializar os Pinos GPIO para acionamento dos LEDs da BitDogLab
    gpio_led_bitdog();

    initDisplay(&ssd); // Inicia o display LED

    gpio_set_irq_enabled_with_callback(botaoB, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

    waitUSB();

    // Inicializa a arquitetura do cyw43
    while (cyw43_arch_init())
    {
        printf("Falha ao inicializar Wi-Fi\n");
        sleep_ms(100);
        return -1;
    }

    npInit();
    npClear();
    npWrite();

    // GPIO do CI CYW43 em nível baixo
    cyw43_arch_gpio_put(LED_PIN, 0);

    // Ativa o Wi-Fi no modo Station, de modo a que possam ser feitas ligações a outros pontos de acesso Wi-Fi.
    cyw43_arch_enable_sta_mode();

    // Conectar à rede WiFI - fazer um loop até que esteja conectado
    printf("Conectando ao Wi-Fi...\n");
    while (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 20000))
    {
        printf("Falha ao conectar ao Wi-Fi\n");
        sleep_ms(100);
    }
    printf("Conectado ao Wi-Fi\n");

    // Caso seja a interface de rede padrão - imprimir o IP do dispositivo.
    if (netif_default)
    {
        printf("IP do dispositivo: %s\n", ipaddr_ntoa(&netif_default->ip_addr));
    }

    // Configura o servidor TCP - cria novos PCBs TCP. É o primeiro passo para estabelecer uma conexão TCP.
    struct tcp_pcb *server = tcp_new();
    if (!server)
    {
        printf("Falha ao criar servidor TCP\n");
        return -1;
    }

    // vincula um PCB (Protocol Control Block) TCP a um endereço IP e porta específicos.
    if (tcp_bind(server, IP_ADDR_ANY, 80) != ERR_OK)
    {
        printf("Falha ao associar servidor TCP à porta 80\n");
        return -1;
    }

    // Coloca um PCB (Protocol Control Block) TCP em modo de escuta, permitindo que ele aceite conexões de entrada.
    server = tcp_listen(server);

    // Define uma função de callback para aceitar conexões TCP de entrada. É um passo importante na configuração de servidores TCP.
    tcp_accept(server, tcp_server_accept);
    printf("Servidor ouvindo na porta 80\n");

    // Inicializa o conversor ADC
    adc_init();
    adc_set_temp_sensor_enabled(true);

    while (true)
    {
        /*
         * Efetuar o processamento exigido pelo cyw43_driver ou pela stack TCP/IP.
         * Este método deve ser chamado periodicamente a partir do ciclo principal
         * quando se utiliza um estilo de sondagem pico_cyw43_arch
         */
        cyw43_arch_poll(); // Necessário para manter o Wi-Fi ativo
        sleep_ms(100);     // Reduz o uso da CPU
    }

    // Desligar a arquitetura CYW43.
    cyw43_arch_deinit();
    return 0;
}

// -------------------------------------- Funções ---------------------------------

// Inicializar os Pinos GPIO para acionamento dos LEDs da BitDogLab
void gpio_led_bitdog(void)
{
    // Configuração dos LEDs como saída
    gpio_init(LED_BLUE_PIN);
    gpio_set_dir(LED_BLUE_PIN, GPIO_OUT);
    gpio_put(LED_BLUE_PIN, false);

    gpio_init(LED_GREEN_PIN);
    gpio_set_dir(LED_GREEN_PIN, GPIO_OUT);
    gpio_put(LED_GREEN_PIN, false);

    gpio_init(LED_RED_PIN);
    gpio_set_dir(LED_RED_PIN, GPIO_OUT);
    gpio_put(LED_RED_PIN, false);

    gpio_init(botaoB);
    gpio_set_dir(botaoB, GPIO_IN);
    gpio_pull_up(botaoB);
}

// Função de callback ao aceitar conexões TCP
static err_t tcp_server_accept(void *arg, struct tcp_pcb *newpcb, err_t err)
{
    tcp_recv(newpcb, tcp_server_recv);
    return ERR_OK;
}

// Tratamento do request do usuário - digite aqui
void user_request(char **request)
{

    if (strstr(*request, "GET /quarto_i_on") != NULL)
    {
        gpio_put(LED_BLUE_PIN, 1);
        npSetLED(13, 1, 0, 0);
        npWrite();
        q1 = 1;
        if (q2 && q3)
        {
            semafor(&ssd, 7);
        }
        else if (q2 && !q3)
        {
            semafor(&ssd, 3);
        }
        else if (!q2 && q3)
        {
            semafor(&ssd, 5);
        }
        else
        {
            semafor(&ssd, 1);
        }
    }
    else if (strstr(*request, "GET /quarto_i_off") != NULL)
    {
        gpio_put(LED_BLUE_PIN, 0);
        npSetLED(13, 0, 0, 0);
        npWrite();
        q1 = 0;
        if (q2 && q3)
        {
            semafor(&ssd, 6);
        }
        else if (q2 && !q3)
        {
            semafor(&ssd, 2);
        }
        else if (!q2 && q3)
        {
            semafor(&ssd, 4);
        }
        else
        {
            semafor(&ssd, 0);
        }
    }
    else if (strstr(*request, "GET /quarto_ii_on") != NULL)
    {
        gpio_put(LED_GREEN_PIN, 1);
        npSetLED(12, 1, 0, 0);
        npWrite();
        q2 = 1;
        if (q1 && q3)
        {
            semafor(&ssd, 7);
        }
        else if (q1 && !q3)
        {
            semafor(&ssd, 3);
        }
        else if (!q1 && q3)
        {
            semafor(&ssd, 6);
        }
        else
        {
            semafor(&ssd, 2);
        }
    }
    else if (strstr(*request, "GET /quarto_ii_off") != NULL)
    {
        gpio_put(LED_GREEN_PIN, 0);
        npSetLED(12, 0, 0, 0);
        npWrite();
        q2 = 0;
        if (q1 && q3)
        {
            semafor(&ssd, 5);
        }
        else if (q1 && !q3)
        {
            semafor(&ssd, 1);
        }
        else if (!q1 && q3)
        {
            semafor(&ssd, 4);
        }
        else
        {
            semafor(&ssd, 0);
        }
    }
    else if (strstr(*request, "GET /quarto_iii_on") != NULL)
    {
        gpio_put(LED_RED_PIN, 1);
        npSetLED(11, 1, 0, 0);
        npWrite();
        q3 = 1;
        if (q1 && q2)
        {
            semafor(&ssd, 7);
        }
        else if (q1 && !q2)
        {
            semafor(&ssd, 5);
        }
        else if (!q1 && q2)
        {
            semafor(&ssd, 6);
        }
        else
        {
            semafor(&ssd, 4);
        }
    }
    else if (strstr(*request, "GET /quarto_iii_off") != NULL)
    {
        gpio_put(LED_RED_PIN, 0);
        npSetLED(11, 0, 0, 0);
        npWrite();

        q3 = 0;
        if (q1 && q2)
        {
            semafor(&ssd, 3);
        }
        else if (q1 && !q2)
        {
            semafor(&ssd, 1);
        }
        else if (!q1 && q2)
        {
            semafor(&ssd, 2);
        }
        else
        {
            semafor(&ssd, 0);
        }
    }
};

// Leitura da temperatura interna
float temp_read(void)
{
    adc_select_input(4);
    uint16_t raw_value = adc_read();
    const float conversion_factor = 3.3f / (1 << 12);
    float temperature = 27.0f - ((raw_value * conversion_factor) - 0.706f) / 0.001721f;
    return temperature;
}

// Função de callback para processar requisições HTTP
static err_t tcp_server_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
    if (!p)
    {
        tcp_close(tpcb);
        tcp_recv(tpcb, NULL);
        return ERR_OK;
    }

    // Alocação do request na memória dinámica
    char *request = (char *)malloc(p->len + 1);
    memcpy(request, p->payload, p->len);
    request[p->len] = '\0';

    printf("Request: %s\n", request);

    // Tratamento de request - Controle dos LEDs
    user_request(&request);

    // Leitura da temperatura interna
    float temperature = temp_read();

    // Cria a resposta HTML
    char html[1300];

    // Instruções html do webserver
    snprintf(html, sizeof(html), // Formatar uma string e armazená-la em um buffer de caracteres
                 "HTTP/1.1 200 OK\r\n"
             "Content-Type: text/html\r\n"
             "\r\n"
             "<!DOCTYPE html>\n"
             "<html>\n"
             "<head>\n"
             "<title> Controle Residencial </title>\n"
             "<style>\n"
             "body { background-color: #6A5ACD; font-family: Arial, sans-serif; text-align: center; margin-top: 50px; }\n"
             "h1 { font-size: 64px; margin-bottom: 30px; }\n"
             "button { color: black;  width: 400px; height: 100px; text-align: center;  display: inline-block; font-size: 36px;  margin: 4px 2px;}\n"
             ".temperature { font-size: 48px; margin-top: 30px; color: #333; }\n"
             "</style>\n"
             "</head>\n"
             "<body>\n"
             "<h1>Sistema de Controle Residencial</h1>\n"
             "<hr>\n"
             "<form action=\"./quarto_i_on\"><button>Ligar Quarto I</button></form>\n"
             "<form action=\"./quarto_i_off\"><button>Desligar Quarto I</button></form>\n"
             "<hr>\n"
             "<form action=\"./quarto_ii_on\"><button>Ligar Quarto II</button></form>\n"
             "<form action=\"./quarto_ii_off\"><button>Desligar Quarto II</button></form>\n"
             "<hr>\n"
             "<form action=\"./quarto_iii_on\"><button>Ligar Quarto III</button></form>\n"
             "<form action=\"./quarto_iii_off\"><button>Desligar Quarto III</button></form>\n"
             "<hr>\n"
             "<p class=\"temperature\">Temperatura Interna: %.2f &deg;C</p>\n"
             "</body>\n"
             "</html>\n",
             temperature);

    // Escreve dados para envio (mas não os envia imediatamente).
    tcp_write(tpcb, html, strlen(html), TCP_WRITE_FLAG_COPY);

    // Envia a mensagem
    tcp_output(tpcb);

    // libera memória alocada dinamicamente
    free(request);

    // libera um buffer de pacote (pbuf) que foi alocado anteriormente
    pbuf_free(p);

    return ERR_OK;
}

// Interrupção
void gpio_irq_handler(uint gpio, uint32_t events)
{
    tempo_atual = to_ms_since_boot(get_absolute_time());

    if (tempo_atual - passado > 5e2)
    {
        passado = tempo_atual;
        if (gpio == botaoB)
        {
            reset_usb_boot(0, 0);
        }
    }
}
