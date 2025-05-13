#include "ssd1306.h"
#include "anima.h"

void ssd1306_init(ssd1306_t *ssd, uint8_t width, uint8_t height, bool external_vcc, uint8_t address, i2c_inst_t *i2c)
{
  ssd->width = width;
  ssd->height = height;
  ssd->pages = height / 8U;
  ssd->address = address;
  ssd->i2c_port = i2c;
  ssd->bufsize = ssd->pages * ssd->width + 1;
  ssd->ram_buffer = calloc(ssd->bufsize, sizeof(uint8_t));
  ssd->ram_buffer[0] = 0x40;
  ssd->port_buffer[0] = 0x80;
}

void ssd1306_config(ssd1306_t *ssd)
{
  ssd1306_command(ssd, SET_DISP | 0x00);
  ssd1306_command(ssd, SET_MEM_ADDR);
  ssd1306_command(ssd, 0x01);
  ssd1306_command(ssd, SET_DISP_START_LINE | 0x00);
  ssd1306_command(ssd, SET_SEG_REMAP | 0x01);
  ssd1306_command(ssd, SET_MUX_RATIO);
  ssd1306_command(ssd, HEIGHT - 1);
  ssd1306_command(ssd, SET_COM_OUT_DIR | 0x08);
  ssd1306_command(ssd, SET_DISP_OFFSET);
  ssd1306_command(ssd, 0x00);
  ssd1306_command(ssd, SET_COM_PIN_CFG);
  ssd1306_command(ssd, 0x12);
  ssd1306_command(ssd, SET_DISP_CLK_DIV);
  ssd1306_command(ssd, 0x80);
  ssd1306_command(ssd, SET_PRECHARGE);
  ssd1306_command(ssd, 0xF1);
  ssd1306_command(ssd, SET_VCOM_DESEL);
  ssd1306_command(ssd, 0x30);
  ssd1306_command(ssd, SET_CONTRAST);
  ssd1306_command(ssd, 0xFF);
  ssd1306_command(ssd, SET_ENTIRE_ON);
  ssd1306_command(ssd, SET_NORM_INV);
  ssd1306_command(ssd, SET_CHARGE_PUMP);
  ssd1306_command(ssd, 0x14);
  ssd1306_command(ssd, SET_DISP | 0x01);
}

void ssd1306_command(ssd1306_t *ssd, uint8_t command)
{
  ssd->port_buffer[1] = command;
  i2c_write_blocking(
      ssd->i2c_port,
      ssd->address,
      ssd->port_buffer,
      2,
      false);
}

void ssd1306_send_data(ssd1306_t *ssd)
{
  ssd1306_command(ssd, SET_COL_ADDR);
  ssd1306_command(ssd, 0);
  ssd1306_command(ssd, ssd->width - 1);
  ssd1306_command(ssd, SET_PAGE_ADDR);
  ssd1306_command(ssd, 0);
  ssd1306_command(ssd, ssd->pages - 1);
  i2c_write_blocking(
      ssd->i2c_port,
      ssd->address,
      ssd->ram_buffer,
      ssd->bufsize,
      false);
}

void ssd1306_pixel(ssd1306_t *ssd, uint8_t x, uint8_t y, bool value)
{
  uint16_t index = (y >> 3) + (x << 3) + 1;
  uint8_t pixel = (y & 0b111);
  if (value)
    ssd->ram_buffer[index] |= (1 << pixel);
  else
    ssd->ram_buffer[index] &= ~(1 << pixel);
}


void initDisplay(ssd1306_t *ssd)
{
  // I2C Initialisation. Using it at 400Khz.
  i2c_init(I2C_PORT, 400 * 1000);
  gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);                   // Set the GPIO pin function to I2C
  gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);                   // Set the GPIO pin function to I2C
  gpio_pull_up(I2C_SDA);                                       // Pull up the data line
  gpio_pull_up(I2C_SCL);                                       // Pull up the clock line
  ssd1306_init(ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT); // Inicializa o display
  ssd1306_config(ssd);                                         // Configura o display
  ssd1306_send_data(ssd);                                      // Envia os dados para o display
}

void ssd1306_fill(ssd1306_t *ssd, bool value)
{
  // Itera por todas as posições do display
  for (uint8_t y = 0; y < ssd->height; ++y)
  {
    for (uint8_t x = 0; x < ssd->width; ++x)
    {
      ssd1306_pixel(ssd, x, y, value);
    }
  }
}

const bool cor = true;

// Funçao que desenha o semaforo no display ssd1306
void semafor(ssd1306_t *ssd, int x)
{
  ssd1306_fill(ssd, !cor); // Limpa o display
  // Desenho feito ao exportar o arquivo no Piskelapp automatizado
  // Semaforo x = {vermelho, amarelo, verde, desligado}
  for (int j = 0; j < 64; j++)
  {
    for (int i = 0; i < 128; i++)
    {
      int a = j * 128 + i;
      if (tomada[x][a] >= 0xff000000)
      {
        ssd1306_pixel(ssd, i, j, true);
      }
    }
  }
  ssd1306_send_data(ssd); // Atualiza o display
}