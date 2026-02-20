/*
 * ═══════════════════════════════════════════════════════════════════════════
 *      ATtiny13 Battery Protection - WITH BOD DETECTION
 * ═══════════════════════════════════════════════════════════════════════════
 * 
 * Features:
 *   ✅ Brown-out Detection (BOD) check on startup
 *   ✅ Reset source detection (Power-on, WDT, BOD, External)
 *   ✅ Visual indication of reset cause
 *   ✅ Safe startup after BOD event
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
#define VOLT_IN     PB4  // ADC input

// Thresholds (8-bit ADC: 0-255)
#define VOLTAGE_GOOD     133
#define VOLTAGE_WARNING  128
#define VOLTAGE_CRITICAL 123

// PWM limits
#define PWM_MAX 250
#define PWM_MIN 0

// Global variables
uint8_t pwm_load = PWM_MIN;
uint8_t reset_source = 0;  // Store reset source for diagnostics

// ═══════════════════════════════════════════════════════════════════════════
//                    RESET SOURCE DETECTION & BOD CHECK
// ═══════════════════════════════════════════════════════════════════════════

/**
 * Check reset source and handle BOD events
 * 
 * MCUSR (MCU Status Register) bits:
 *   WDRF  (bit 3): Watchdog Reset Flag
 *   BORF  (bit 2): Brown-out Reset Flag ← BOD!
 *   EXTRF (bit 1): External Reset Flag
 *   PORF  (bit 0): Power-on Reset Flag
 * 
 * This function should be called FIRST in main(), before any other init!
 */
void check_reset_source(void) {
    // Save reset source (MCUSR will be cleared)
    reset_source = MCUSR;
    
    // Clear all reset flags (required before disabling WDT)
    MCUSR = 0;
    
    // Disable watchdog (in case it caused reset)
    wdt_disable();
    
    // ───────────────────────────────────────────────────────────────────────
    // Analyze reset source
    // ───────────────────────────────────────────────────────────────────────
    
    if(reset_source & (1 << BORF)) {
        // ═══════════════════════════════════════════════════════════════════
        //              BROWN-OUT RESET DETECTED! ⚠️
        // ═══════════════════════════════════════════════════════════════════
        // VCC dropped below BOD threshold (e.g., 4.3V)
        // This means power supply is unstable or battery is very low!
        
        // Visual indication: Fast blink on both LEDs
        DDRB = (1 << STATUS_LED) | (1 << WATCHDOG);
        
        for(uint8_t i = 0; i < 20; i++) {
            PORTB ^= (1 << STATUS_LED) | (1 << WATCHDOG);
            _delay_ms(50);  // 50ms on/off = very fast blink
        }
        
        // Small delay to let user see the indication
        _delay_ms(500);
        
        // Strategy after BOD:
        // Option 1: Go to sleep immediately (safest)
        // Option 2: Try to measure voltage and decide
        // Option 3: Reduce load and continue carefully
        
        // We'll use Option 2: measure and decide
    }
    else if(reset_source & (1 << WDRF)) {
        // ═══════════════════════════════════════════════════════════════════
        //              WATCHDOG RESET DETECTED
        // ═══════════════════════════════════════════════════════════════════
        // Software hung and WDT reset the MCU
        // This is normal after sleep wake-up or after crash recovery
        
        // Visual indication: 3 quick blinks on watchdog LED
        DDRB = (1 << WATCHDOG);
        
        for(uint8_t i = 0; i < 3; i++) {
            PORTB |= (1 << WATCHDOG);
            _delay_ms(100);
            PORTB &= ~(1 << WATCHDOG);
            _delay_ms(100);
        }
    }
    else if(reset_source & (1 << EXTRF)) {
        // ═══════════════════════════════════════════════════════════════════
        //              EXTERNAL RESET DETECTED
        // ═══════════════════════════════════════════════════════════════════
        // Reset pin was pulled low (manual reset button)
        
        // Visual indication: 2 slow blinks on status LED
        DDRB = (1 << STATUS_LED);
        
        for(uint8_t i = 0; i < 2; i++) {
            PORTB |= (1 << STATUS_LED);
            _delay_ms(200);
            PORTB &= ~(1 << STATUS_LED);
            _delay_ms(200);
        }
    }
    else if(reset_source & (1 << PORF)) {
        // ═══════════════════════════════════════════════════════════════════
        //              POWER-ON RESET DETECTED
        // ═══════════════════════════════════════════════════════════════════
        // First power-up or VCC was completely removed
        
        // Visual indication: 1 long blink on both LEDs
        DDRB = (1 << STATUS_LED) | (1 << WATCHDOG);
        
        PORTB |= (1 << STATUS_LED) | (1 << WATCHDOG);
        _delay_ms(500);
        PORTB &= ~((1 << STATUS_LED) | (1 << WATCHDOG));
        _delay_ms(200);
    }
    
    // Clear outputs before continuing
    PORTB = 0;
}

// ═══════════════════════════════════════════════════════════════════════════
//                          HARDWARE INITIALIZATION
// ═══════════════════════════════════════════════════════════════════════════

void init_hw(void) {
    // Set outputs: PB0, PB2, PB3
    DDRB = (1 << LOAD_PIN) | (1 << WATCHDOG) | (1 << STATUS_LED);
    PORTB = 0;
    
    // Setup ADC: 8-bit mode, channel ADC2 (PB4)
    ADMUX = (1 << ADLAR) | (1 << MUX1);
    ADCSRA = (1 << ADEN) | (1 << ADPS1) | (1 << ADPS0);
    DIDR0 = (1 << ADC2D);
    
    // Setup PWM: Fast PWM on PB0
    TCCR0A = (1 << COM0A1) | (1 << WGM01) | (1 << WGM00);
    TCCR0B = (1 << CS01);
    OCR0A = PWM_MIN;
    
    // Enable watchdog: 1 second timeout
    wdt_enable(WDTO_1S);
}

// ═══════════════════════════════════════════════════════════════════════════
//                          ADC READ - 8-BIT OPTIMIZED
// ═══════════════════════════════════════════════════════════════════════════

uint8_t get_volt(void) {
    ADCSRA |= (1 << ADSC);
    while(ADCSRA & (1 << ADSC));
    return ADCH;
}

// ═══════════════════════════════════════════════════════════════════════════
//                    SLEEP MODE WITH FULL PWM SHUTDOWN
// ═══════════════════════════════════════════════════════════════════════════

void go_sleep(void) {
    // Fully disable PWM
    TCCR0A = 0;
    TCCR0B = 0;
    
    // Turn off all outputs
    PORTB = 0;
    OCR0A = 0;
    pwm_load = PWM_MIN;
    
    // Disable ADC
    ADCSRA &= ~(1 << ADEN);
    
    // Configure WDT for interrupt mode (8 seconds)
    WDTCR = (1 << WDTIE) | (1 << WDP3) | (1 << WDP0);
    
    // Enter power-down sleep
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    sleep_enable();
    sei();
    sleep_cpu();
    
    // ─── Woke up! ───
    
    sleep_disable();
    
    // Re-enable ADC
    ADCSRA |= (1 << ADEN);
    
    // Re-enable PWM
    TCCR0A = (1 << COM0A1) | (1 << WGM01) | (1 << WGM00);
    TCCR0B = (1 << CS01);
    
    // Re-enable WDT in reset mode
    wdt_enable(WDTO_1S);
    
    // Reactivate Dead Man's Switch
    for(uint8_t i = 0; i < 5; i++) {
        PORTB ^= (1 << WATCHDOG);
        _delay_ms(20);
    }
}

ISR(WDT_vect) {
    // Empty - just wake up
}

// ═══════════════════════════════════════════════════════════════════════════
//                          MAIN PROGRAM
// ═══════════════════════════════════════════════════════════════════════════

int main(void) {
    // ───────────────────────────────────────────────────────────────────────
    // STEP 1: Check reset source FIRST (before any other initialization!)
    // ───────────────────────────────────────────────────────────────────────
    check_reset_source();
    
    // ───────────────────────────────────────────────────────────────────────
    // STEP 2: If BOD occurred, check voltage immediately
    // ───────────────────────────────────────────────────────────────────────
    if(reset_source & (1 << BORF)) {
        // BOD detected! Initialize minimal hardware to check voltage
        DDRB = (1 << STATUS_LED) | (1 << WATCHDOG);
        ADMUX = (1 << ADLAR) | (1 << MUX1);
        ADCSRA = (1 << ADEN) | (1 << ADPS1) | (1 << ADPS0);
        DIDR0 = (1 << ADC2D);
        
        _delay_ms(10);  // Let ADC stabilize
        
        uint8_t voltage = get_volt();
        
        if(voltage < VOLTAGE_CRITICAL) {
            // Voltage still critically low after BOD!
            // Flash warning and go to sleep immediately
            
            for(uint8_t i = 0; i < 10; i++) {
                PORTB ^= (1 << STATUS_LED) | (1 << WATCHDOG);
                _delay_ms(100);
            }
            
            // Disable ADC
            ADCSRA &= ~(1 << ADEN);
            
            // Configure WDT for sleep
            WDTCR = (1 << WDTIE) | (1 << WDP3) | (1 << WDP0);
            
            // Sleep indefinitely (will wake every 8 sec and check)
            while(1) {
                set_sleep_mode(SLEEP_MODE_PWR_DOWN);
                sleep_enable();
                sei();
                sleep_cpu();
                sleep_disable();
                
                // Check voltage after wake
                ADCSRA |= (1 << ADEN);
                _delay_ms(10);
                voltage = get_volt();
                
                if(voltage >= VOLTAGE_CRITICAL) {
                    // Voltage recovered! Break and continue normal operation
                    break;
                }
                
                // Still low, flash once and sleep again
                PORTB |= (1 << STATUS_LED);
                _delay_ms(50);
                PORTB &= ~(1 << STATUS_LED);
                
                ADCSRA &= ~(1 << ADEN);
            }
        }
        
        // If we're here, voltage is OK, continue to normal init
    }
    
    // ───────────────────────────────────────────────────────────────────────
    // STEP 3: Normal hardware initialization
    // ───────────────────────────────────────────────────────────────────────
    init_hw();
    
    // ───────────────────────────────────────────────────────────────────────
    // STEP 4: Startup indication
    // ───────────────────────────────────────────────────────────────────────
    for(uint8_t i = 0; i < 5; i++) {
        PORTB ^= (1 << WATCHDOG);
        _delay_ms(100);
    }
    
    // ───────────────────────────────────────────────────────────────────────
    // STEP 5: Main loop
    // ───────────────────────────────────────────────────────────────────────
    
    uint8_t heartbeat_counter = 0;
    uint8_t status_counter = 0;
    
    while(1) {
        wdt_reset();
        
        uint8_t voltage = get_volt();
        
        // Check for
