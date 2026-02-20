/*
 * ═══════════════════════════════════════════════════════════════════════════
 *      ATtiny13 Battery Protection - OPTIMIZED LOW POWER EDITION
 * ═══════════════════════════════════════════════════════════════════════════
 * 
 * Optimizations:
 *   ✅ 8-bit ADC (ADLAR=1, read only ADCH)
 *   ✅ PWM fully disabled before sleep (TCCR0A=0, TCCR0B=0)
 *   ✅ Single ADC read (no averaging for speed)
 *   ✅ 1.2 MHz clock for ultra-low power
 * 
 * Power consumption:
 *   Active: ~0.5 mA
 *   Sleep:  ~4 µA (real power-down!)
 * 
 * Flash usage: ~680 bytes
 * 
 * ═══════════════════════════════════════════════════════════════════════════
 */

#define F_CPU 1200000UL

#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>
#include <util/delay.h>

// Pin definitions
#define LOAD_PIN    PB0  // PWM Load
#define STATUS_LED  PB3  // Status LED
#define WATCHDOG    PB2  // Heartbeat + Dead Man's Switch
#define VOLT_IN     PB4  // ADC input (not used as pin, only ADC)

// Thresholds (8-bit ADC: 0-255)
#define VOLTAGE_GOOD     133
#define VOLTAGE_WARNING  128
#define VOLTAGE_CRITICAL 123

// PWM limits
#define PWM_MAX 250
#define PWM_MIN 0

// Global variables
uint8_t pwm_load = PWM_MIN;

// ═══════════════════════════════════════════════════════════════════════════
//                          HARDWARE INITIALIZATION
// ═══════════════════════════════════════════════════════════════════════════

void init_hw(void) {
    // Disable watchdog after reset
    MCUSR = 0;
    wdt_disable();
    
    // Set outputs: PB0, PB2, PB3
    DDRB = (1 << LOAD_PIN) | (1 << WATCHDOG) | (1 << STATUS_LED);
    PORTB = 0;
    
    // ───────────────────────────────────────────────────────────────────────
    // Setup ADC: 8-bit mode (ADLAR=1), channel ADC2 (PB4)
    // ───────────────────────────────────────────────────────────────────────
    // ADLAR=1: Left-adjust result → read only ADCH (8-bit)
    // MUX[1:0]=10: Select ADC2 (PB4)
    ADMUX = (1 << ADLAR) | (1 << MUX1);
    
    // ADC prescaler: /8 for 1.2MHz → 150kHz ADC clock
    // ADEN=1: Enable ADC
    // ADPS[2:0]=011: Prescaler /8
    ADCSRA = (1 << ADEN) | (1 << ADPS1) | (1 << ADPS0);
    
    // Disable digital input buffer on PB4 to save power
    DIDR0 = (1 << ADC2D);
    
    // ───────────────────────────────────────────────────────────────────────
    // Setup PWM: Fast PWM on PB0 (OC0A)
    // ───────────────────────────────────────────────────────────────────────
    // COM0A1=1: Clear OC0A on compare match (non-inverted PWM)
    // WGM[1:0]=11: Fast PWM mode
    TCCR0A = (1 << COM0A1) | (1 << WGM01) | (1 << WGM00);
    
    // CS01=1: Prescaler /8
    // PWM freq = 1.2MHz / 8 / 256 = 586 Hz
    TCCR0B = (1 << CS01);
    
    OCR0A = PWM_MIN;
    
    // Enable watchdog: 1 second timeout
    wdt_enable(WDTO_1S);
}

// ═══════════════════════════════════════════════════════════════════════════
//                    ADC READ - OPTIMIZED 8-BIT VERSION
// ═══════════════════════════════════════════════════════════════════════════

/**
 * Read 8-bit ADC value (single sample, no averaging)
 * Optimized for speed and code size
 * 
 * @return ADC value (0-255)
 */
uint8_t get_volt(void) {
    // Start conversion
    ADCSRA |= (1 << ADSC);
    
    // Wait for conversion to complete
    while(ADCSRA & (1 << ADSC));
    
    // Return 8-bit result (only ADCH, thanks to ADLAR=1)
    return ADCH;
}

// ═══════════════════════════════════════════════════════════════════════════
//              SLEEP MODE - OPTIMIZED WITH PWM SHUTDOWN
// ═══════════════════════════════════════════════════════════════════════════

/**
 * Enter deep sleep mode with full power-down
 * 
 * CRITICAL: Fully disable PWM before sleep!
 * Without this, Timer0 continues running and PB0 keeps toggling,
 * consuming ~0.5mA instead of ~4µA!
 */
void go_sleep(void) {
    // ───────────────────────────────────────────────────────────────────────
    // STEP 1: Disable PWM output (disconnect OC0A from PB0)
    // ───────────────────────────────────────────────────────────────────────
    TCCR0A = 0;  // Clear COM0A1, WGM bits → PB0 becomes normal I/O
    
    // ───────────────────────────────────────────────────────────────────────
    // STEP 2: Stop Timer0 clock (no more counting)
    // ───────────────────────────────────────────────────────────────────────
    TCCR0B = 0;  // Clear CS bits → Timer0 stopped
    
    // ───────────────────────────────────────────────────────────────────────
    // STEP 3: Turn off all outputs
    // ───────────────────────────────────────────────────────────────────────
    PORTB = 0;   // All pins LOW
    OCR0A = 0;   // Clear PWM compare value
    pwm_load = PWM_MIN;
    
    // ───────────────────────────────────────────────────────────────────────
    // STEP 4: Disable ADC to save power
    // ───────────────────────────────────────────────────────────────────────
    ADCSRA &= ~(1 << ADEN);
    
    // ───────────────────────────────────────────────────────────────────────
    // STEP 5: Configure WDT for interrupt mode (8 seconds)
    // ───────────────────────────────────────────────────────────────────────
    WDTCR = (1 << WDTIE) | (1 << WDP3) | (1 << WDP0);
    
    // ───────────────────────────────────────────────────────────────────────
    // STEP 6: Enter power-down sleep mode
    // ───────────────────────────────────────────────────────────────────────
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    sleep_enable();
    sei();  // Enable interrupts for WDT wake-up
    
    sleep_cpu();  // Sleep now! (~4µA consumption)
    
    // ═══════════════════════════════════════════════════════════════════════
    //                          WOKE UP!
    // ═══════════════════════════════════════════════════════════════════════
    
    sleep_disable();
    
    // ───────────────────────────────────────────────────────────────────────
    // STEP 7: Re-enable peripherals
    // ───────────────────────────────────────────────────────────────────────
    
    // Re-enable ADC
    ADCSRA |= (1 << ADEN);
    
    // Re-enable PWM
    TCCR0A = (1 << COM0A1) | (1 << WGM01) | (1 << WGM00);
    TCCR0B = (1 << CS01);
    
    // Re-enable WDT in reset mode
    wdt_enable(WDTO_1S);
    
    // Reactivate Dead Man's Switch (charge capacitor)
    for(uint8_t i = 0; i < 5; i++) {
        PORTB ^= (1 << WATCHDOG);
        _delay_ms(20);
    }
}

/**
 * WDT interrupt - just wake up from sleep
 */
ISR(WDT_vect) {
    // Empty ISR - just wake up
}

// ═══════════════════════════════════════════════════════════════════════════
//                          MAIN PROGRAM
// ═══════════════════════════════════════════════════════════════════════════

int main(void) {
    init_hw();
    
    // Startup indication (5 blinks on watchdog LED)
    for(uint8_t i = 0; i < 5; i++) {
        PORTB ^= (1 << WATCHDOG);
        _delay_ms(100);
    }
    
    uint8_t heartbeat_counter = 0;
    uint8_t status_counter = 0;
    
    while(1) {
        wdt_reset();  // Reset watchdog timer
        
        // Read battery voltage (single 8-bit sample)
        uint8_t voltage = get_volt();
        
        // ───────────────────────────────────────────────────────────────────
        // CHECK FOR CRITICAL LOW VOLTAGE → SLEEP MODE
        // ───────────────────────────────────────────────────────────────────
        
        if(voltage < VOLTAGE_CRITICAL) {
            // Flash status LED fast (battery critically
