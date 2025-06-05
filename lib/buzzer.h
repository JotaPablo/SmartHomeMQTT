#ifndef BUZZER_H
#define BUZZER_H

#include <stdbool.h>
#include <stdint.h>
#include "pico/stdlib.h"



// Inicializa o buzzer no pino especificado (usando PWM)
void buzzer_init(uint pin);

// Liga o buzzer com a frequência especificada (em Hz) usando PWM
void buzzer_turn_on(uint pin, uint frequency);

// Desliga o buzzer
void buzzer_turn_off(uint pin);

// Inicia um beep não bloqueante com a frequência e duração (em ms) especificadas
void buzzer_start(uint frequency, uint duration_ms);

// Para o beep (desliga o buzzer)
void buzzer_stop(void);

// Função de atualização: deve ser chamada periodicamente no loop principal para
// verificar se a duração do beep expirou e desligar o buzzer
void buzzer_update(void);




#endif // BUZZER_H
