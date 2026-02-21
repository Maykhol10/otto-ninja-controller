/**
 * MELODÍAS WEB - Simulación de buzzer usando Web Audio API
 * Replica las melodías de melodias.h para preview en navegador
 */

class MelodiasWeb {
    constructor() {
        // Crear contexto de audio
        this.audioContext = null;
        this.currentGain = null;
        this.isPlaying = false;
        this.volume = 30; // Volumen por defecto (0-100)
        this.muted = false; // Estado de silencio

        // Inicializar contexto de audio (lazy loading)
        this.initAudioContext();
    }

    initAudioContext() {
        if (!this.audioContext) {
            try {
                this.audioContext = new (window.AudioContext || window.webkitAudioContext)();
            } catch (e) {
                console.error('Web Audio API no soportada', e);
            }
        }
    }

    /**
     * Reproducir un tono
     * @param {number} frequency - Frecuencia en Hz (0 = silencio)
     * @param {number} duration - Duración en ms
     */
    async playTone(frequency, duration) {
        if (!this.audioContext) {
            await new Promise(resolve => setTimeout(resolve, duration));
            return;
        }

        // Resume audio context si está suspendido (política de navegadores)
        if (this.audioContext.state === 'suspended') {
            await this.audioContext.resume();
        }

        if (frequency > 0) {
            const oscillator = this.audioContext.createOscillator();
            const gainNode = this.audioContext.createGain();

            oscillator.type = 'square'; // Simular buzzer con onda cuadrada
            oscillator.frequency.value = frequency;

            // Calcular ganancia basada en volumen y estado de mute
            // Usar window.state si está disponible (desde app.js), si no usar valores locales
            const volume = (window.state && window.state.volume !== undefined) ? window.state.volume : this.volume;
            const muted = (window.state && window.state.muted !== undefined) ? window.state.muted : this.muted;
            const targetGain = muted ? 0 : (volume / 100) * 0.5;

            gainNode.gain.value = targetGain;

            oscillator.connect(gainNode);
            gainNode.connect(this.audioContext.destination);

            this.currentGain = gainNode;

            oscillator.start();
            oscillator.stop(this.audioContext.currentTime + duration / 1000);

            await new Promise(resolve => setTimeout(resolve, duration));
        } else {
            // Silencio
            await new Promise(resolve => setTimeout(resolve, duration));
        }
    }

    /**
     * Detener todos los sonidos
     */
    stop() {
        if (this.currentGain) {
            this.currentGain.gain.value = 0;
        }
        this.isPlaying = false;
    }

    /**
     * Reproducir melodía por número
     * @param {number} songNumber - Número de melodía (0-15)
     */
    async sing(songNumber) {
        if (this.isPlaying) {
            console.log('Ya hay una melodía reproduciéndose');
            return;
        }

        this.isPlaying = true;

        try {
            // Melodías personalizadas (16-20): leer de state.customMelodies
            if (songNumber >= 16 && songNumber <= 20) {
                const melody = window.state?.customMelodies?.[songNumber];
                if (melody && melody.notes) {
                    for (const [freq, dur] of melody.notes) {
                        await this.playTone(freq, dur);
                    }
                }
                return;
            }

            switch(songNumber) {
                case 0: // S_CONNECTION
                    await this.playTone(659, 50);   // E5
                    await this.playTone(1319, 55);  // E6
                    await this.playTone(1760, 60);  // A6
                    await this.playTone(1319, 50);  // E6
                    await this.playTone(659, 50);   // E5
                    await this.playTone(0, 30);
                    await this.playTone(1319, 55);  // E6
                    break;

                case 1: // S_DISCONNECTION
                    await this.playTone(659, 50);   // E5
                    await this.playTone(1760, 55);  // A6
                    await this.playTone(1319, 50);  // E6
                    await this.playTone(1760, 55);  // A6
                    await this.playTone(659, 50);   // E5
                    await this.playTone(0, 30);
                    break;

                case 2: // S_BUTTON_PUSHED
                    await this.playTone(1319, 50);  // E6
                    await this.playTone(1568, 50);  // G6
                    await this.playTone(2637, 50);  // E7
                    await this.playTone(0, 30);
                    break;

                case 3: // S_BATTLE
                    await this.playTone(659, 100);  // E5
                    await this.playTone(659, 100);  // E5
                    await this.playTone(659, 100);  // E5
                    await this.playTone(523, 100);  // C5
                    await this.playTone(659, 150);  // E5
                    await this.playTone(784, 150);  // G5
                    await this.playTone(523, 150);  // C5
                    await this.playTone(0, 30);
                    break;

                case 4: // S_FURIA
                    await this.playTone(1175, 80);  // D6
                    await this.playTone(1175, 80);  // D6
                    await this.playTone(1175, 80);  // D6
                    await this.playTone(880, 100);  // A5
                    await this.playTone(831, 100);  // GS5
                    await this.playTone(784, 100);  // G5
                    await this.playTone(698, 100);  // F5
                    await this.playTone(587, 150);  // D5
                    await this.playTone(0, 30);
                    break;

                case 5: // S_NINJA
                    await this.playTone(1319, 50);  // E6
                    await this.playTone(0, 50);
                    await this.playTone(1319, 50);  // E6
                    await this.playTone(0, 50);
                    await this.playTone(1319, 50);  // E6
                    await this.playTone(0, 50);
                    await this.playTone(1047, 100); // C6
                    await this.playTone(1319, 100); // E6
                    await this.playTone(1568, 100); // G6
                    await this.playTone(2093, 150); // C7
                    await this.playTone(0, 30);
                    break;

                case 6: // S_SURPRISE
                    await this.playTone(800, 80);
                    await this.playTone(2150, 80);
                    await this.playTone(1050, 80);
                    await this.playTone(0, 30);
                    break;

                case 7: // S_OHOOH
                    await this.playTone(880, 250);
                    await this.playTone(1760, 250);
                    await this.playTone(880, 250);
                    await this.playTone(0, 30);
                    break;

                case 8: // S_OHOOH2
                    await this.playTone(1050, 150);
                    await this.playTone(1250, 150);
                    await this.playTone(1050, 150);
                    await this.playTone(0, 30);
                    break;

                case 9: // S_CUDDLY
                    await this.playTone(700, 250);
                    await this.playTone(900, 250);
                    await this.playTone(1100, 300);
                    await this.playTone(0, 30);
                    break;

                case 10: // S_SLEEPING
                    await this.playTone(100, 500);
                    await this.playTone(200, 500);
                    await this.playTone(300, 500);
                    await this.playTone(0, 30);
                    break;

                case 11: // S_HAPPY
                    await this.playTone(659, 50);   // E5
                    await this.playTone(784, 50);   // G5
                    await this.playTone(880, 50);   // A5
                    await this.playTone(988, 50);   // B5
                    await this.playTone(1319, 50);  // E6
                    await this.playTone(1568, 50);  // G6
                    await this.playTone(1760, 50);  // A6
                    await this.playTone(0, 30);
                    break;

                case 12: // S_SUPER_HAPPY
                    await this.playTone(1047, 60);  // C6
                    await this.playTone(1319, 60);  // E6
                    await this.playTone(1568, 60);  // G6
                    await this.playTone(2093, 60);  // C7
                    await this.playTone(2637, 60);  // E7
                    await this.playTone(3136, 60);  // G7
                    await this.playTone(0, 30);
                    break;

                case 13: // S_HAPPY_SHORT
                    await this.playTone(659, 80);   // E5
                    await this.playTone(784, 80);   // G5
                    await this.playTone(880, 80);   // A5
                    await this.playTone(0, 30);
                    break;

                case 14: // S_SAD
                    await this.playTone(880, 200);
                    await this.playTone(669, 200);
                    await this.playTone(587, 200);
                    await this.playTone(0, 30);
                    break;

                case 15: // S_CONFUSED
                    await this.playTone(1000, 60);
                    await this.playTone(1044, 60);
                    await this.playTone(1000, 60);
                    await this.playTone(1044, 60);
                    await this.playTone(1000, 60);
                    await this.playTone(0, 30);
                    break;

                default:
                    console.log('Melodía no encontrada:', songNumber);
            }
        } finally {
            this.isPlaying = false;
        }
    }

    /**
     * Beep simple
     */
    async beep() {
        await this.playTone(523, 100); // C5
        await new Promise(resolve => setTimeout(resolve, 50));
    }

    /**
     * Establecer volumen
     * @param {number} value - Volumen (0-100)
     */
    setVolume(value) {
        this.volume = Math.max(0, Math.min(100, value));
    }

    /**
     * Silenciar/Activar
     */
    toggleMute() {
        this.muted = !this.muted;
    }
}

// Crear instancia global
window.melodiasWeb = new MelodiasWeb();
