#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "ws2818b.pio.h"

#define PIN_LED        7
#define QTD_LEDS       25
#define PIN_ENTRADA    18
#define DEBOUNCE_MS    10

static inline uint8_t inverter_bits8(uint8_t v) {
    v = (uint8_t)((v >> 1) & 0x55) | (uint8_t)((v & 0x55) << 1);
    v = (uint8_t)((v >> 2) & 0x33) | (uint8_t)((v & 0x33) << 2);
    v = (uint8_t)((v >> 4) & 0x0F) | (uint8_t)((v & 0x0F) << 4);
    return v;
}

static void escrever_cor_solida(PIO pio, uint maquina, uint8_t r, uint8_t g, uint8_t b) {
    uint8_t G = inverter_bits8(g), R = inverter_bits8(r), B = inverter_bits8(b);
    for (uint i = 0; i < QTD_LEDS; ++i) {
        pio_sm_put_blocking(pio, maquina, G);
        pio_sm_put_blocking(pio, maquina, R);
        pio_sm_put_blocking(pio, maquina, B);
    }
    sleep_us(100);
}

int main() {
    stdio_init_all();

    gpio_init(PIN_ENTRADA);
    gpio_set_dir(PIN_ENTRADA, false);
    gpio_pull_down(PIN_ENTRADA);

    PIO pio = pio0;
    uint deslocamento = pio_add_program(pio, &ws2818b_program);
    int indice_sm = pio_claim_unused_sm(pio, false);
    if (indice_sm < 0) {
        pio = pio1;
        deslocamento = pio_add_program(pio, &ws2818b_program);
        indice_sm = pio_claim_unused_sm(pio, true);
    }
    uint maquina = (uint)indice_sm;

    ws2818b_program_init(pio, maquina, deslocamento, PIN_LED, 800000.f);

    bool ultimo_ligado = false;
    escrever_cor_solida(pio, maquina, 0, 0, 0);

    while (true) {
        bool leitura1 = gpio_get(PIN_ENTRADA);
        sleep_ms(DEBOUNCE_MS);
        bool leitura2 = gpio_get(PIN_ENTRADA);
        bool ligado = leitura1 && leitura2;

        if (ligado != ultimo_ligado) {
            ultimo_ligado = ligado;
            if (ligado) {
                escrever_cor_solida(pio, maquina, 255, 255, 255);
            } else {
                escrever_cor_solida(pio, maquina, 0, 0, 0);
            }
        }
        sleep_ms(5);
    }
}
