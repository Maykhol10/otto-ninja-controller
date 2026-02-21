/**
 * MELODÍAS ESP32 - Reproducción de melodías en buzzer pasivo
 * Usa tone()/noTone() simple sin control de volumen por PWM
 */

#ifndef MELODIAS_H
#define MELODIAS_H

#include <Arduino.h>

class Melodias {
public:
    Melodias(uint8_t buzzerPin) : buzzerPin(buzzerPin) {
        pinMode(buzzerPin, OUTPUT);
    }

    /**
     * Reproducir un tono
     * @param frequency - Frecuencia en Hz (0 = silencio)
     * @param duration - Duración en ms
     */
    void playTone(unsigned int frequency, unsigned int duration) {
        if (frequency > 0) {
            tone(buzzerPin, frequency);
        }
        delay(duration);
        noTone(buzzerPin);
    }

    /**
     * Reproducir melodía por número
     * @param songNumber - Número de melodía (0-15: predefinidas, 16-20: personalizadas)
     */
    void sing(int songNumber);

    /**
     * Beep simple
     */
    void beep() {
        playTone(523, 100); // C5
        delay(50);
    }

private:
    uint8_t buzzerPin;
};

#endif // MELODIAS_H
