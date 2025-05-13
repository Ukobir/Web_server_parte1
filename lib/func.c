#include <stdio.h>
#include <stdlib.h>

void waitUSB()
{
    printf("Aguardando a comunicação USB...\n");

    // Espera até que a comunicação USB esteja conectada
    while (!stdio_usb_connected())
    {
        // O dispositivo USB ainda não foi conectado
        sleep_ms(100); // Espera 100ms antes de checar novamente
    }

    // Quando a comunicação USB for detectada, a execução continua
    printf("Comunicação USB detectada!\n");
}
