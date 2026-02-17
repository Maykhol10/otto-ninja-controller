/**
 * MELODÍAS ESP32 - Implementación
 */

#include "melodias.h"

// Estructura para melodías personalizadas (definida en el .ino principal)
extern struct CustomMelody {
    char name[21];
    int noteCount;
    uint16_t notes[200]; // formato: freq, dur, freq, dur...
    bool loaded;
} customMelodies[5];

void Melodias::sing(int songNumber) {
    // Melodías personalizadas (16-20)
    if (songNumber >= 16 && songNumber <= 20) {
        int slot = songNumber - 16;
        if (customMelodies[slot].loaded && customMelodies[slot].noteCount > 0) {
            for (int i = 0; i < customMelodies[slot].noteCount * 2; i += 2) {
                unsigned int freq = customMelodies[slot].notes[i];
                unsigned int dur = customMelodies[slot].notes[i + 1];
                playTone(freq, dur);
            }
        }
        return;
    }

    switch(songNumber) {
        case 0: // S_CONNECTION
            playTone(659, 50);   // E5
            playTone(1319, 55);  // E6
            playTone(1760, 60);  // A6
            playTone(1319, 50);  // E6
            playTone(659, 50);   // E5
            playTone(0, 30);
            playTone(1319, 55);  // E6
            break;

        case 1: // S_DISCONNECTION
            playTone(659, 50);   // E5
            playTone(1760, 55);  // A6
            playTone(1319, 50);  // E6
            playTone(1760, 55);  // A6
            playTone(659, 50);   // E5
            playTone(0, 30);
            break;

        case 2: // S_BUTTON_PUSHED
            playTone(1319, 50);  // E6
            playTone(1568, 50);  // G6
            playTone(2637, 50);  // E7
            playTone(0, 30);
            break;

        case 3: // S_BATTLE
            playTone(659, 100);  // E5
            playTone(659, 100);  // E5
            playTone(659, 100);  // E5
            playTone(523, 100);  // C5
            playTone(659, 150);  // E5
            playTone(784, 150);  // G5
            playTone(523, 150);  // C5
            playTone(0, 30);
            break;

        case 4: // S_FURIA
            playTone(1175, 80);  // D6
            playTone(1175, 80);  // D6
            playTone(1175, 80);  // D6
            playTone(880, 100);  // A5
            playTone(831, 100);  // GS5
            playTone(784, 100);  // G5
            playTone(698, 100);  // F5
            playTone(587, 150);  // D5
            playTone(0, 30);
            break;

        case 5: // S_NINJA
            playTone(1319, 50);  // E6
            playTone(0, 50);
            playTone(1319, 50);  // E6
            playTone(0, 50);
            playTone(1319, 50);  // E6
            playTone(0, 50);
            playTone(1047, 100); // C6
            playTone(1319, 100); // E6
            playTone(1568, 100); // G6
            playTone(2093, 150); // C7
            playTone(0, 30);
            break;

        case 6: // S_SURPRISE
            playTone(800, 80);
            playTone(2150, 80);
            playTone(1050, 80);
            playTone(0, 30);
            break;

        case 7: // S_OHOOH
            playTone(880, 250);
            playTone(1760, 250);
            playTone(880, 250);
            playTone(0, 30);
            break;

        case 8: // S_OHOOH2
            playTone(1050, 150);
            playTone(1250, 150);
            playTone(1050, 150);
            playTone(0, 30);
            break;

        case 9: // S_CUDDLY
            playTone(700, 250);
            playTone(900, 250);
            playTone(1100, 300);
            playTone(0, 30);
            break;

        case 10: // S_SLEEPING
            playTone(100, 500);
            playTone(200, 500);
            playTone(300, 500);
            playTone(0, 30);
            break;

        case 11: // S_HAPPY
            playTone(659, 50);   // E5
            playTone(784, 50);   // G5
            playTone(880, 50);   // A5
            playTone(988, 50);   // B5
            playTone(1319, 50);  // E6
            playTone(1568, 50);  // G6
            playTone(1760, 50);  // A6
            playTone(0, 30);
            break;

        case 12: // S_SUPER_HAPPY
            playTone(1047, 60);  // C6
            playTone(1319, 60);  // E6
            playTone(1568, 60);  // G6
            playTone(2093, 60);  // C7
            playTone(2637, 60);  // E7
            playTone(3136, 60);  // G7
            playTone(0, 30);
            break;

        case 13: // S_HAPPY_SHORT
            playTone(659, 80);   // E5
            playTone(784, 80);   // G5
            playTone(880, 80);   // A5
            playTone(0, 30);
            break;

        case 14: // S_SAD
            playTone(880, 200);
            playTone(669, 200);
            playTone(587, 200);
            playTone(0, 30);
            break;

        case 15: // S_CONFUSED
            playTone(1000, 60);
            playTone(1044, 60);
            playTone(1000, 60);
            playTone(1044, 60);
            playTone(1000, 60);
            playTone(0, 30);
            break;

        default:
            // Melodía no encontrada
            break;
    }
}
