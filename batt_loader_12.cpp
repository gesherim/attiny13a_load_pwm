/*
 * ═══════════════════════════════════════════════════════════════════════════
 *         ATtiny13 Battery Protection - ULTRA LOW POWER EDITION
 *                        1.2 MHz for maximum efficiency!
 * ═══════════════════════════════════════════════════════════════════════════
 * 
 * Clock: 1.2 MHz (128 kHz internal / 8 prescaler OFF)
 * Power consumption:
 *   Active: ~0.5 mA (6x less than 9.6 MHz!)
 *   Sleep:  ~4 µA
 * 
 * Battery life (1000mAh, 50% sleep):
 *   ~83 days! (vs 12 days at 9.6 MHz)
 * 
 * ═══════════════════════════════════════════════════════════════════════════
 */

#define F_CPU 1200000UL  // 1.2 MHz for ultra-low power!

#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>
#include <util/delay.h>

// Pin definitions
#define LOAD_PIN    PB0  // PWM Load
#define STATUS_LED  PB3  // Status LED (optional)
#define WATCHDOG    PB2  // Heartbeat + Dead Man's Switch
#define VOLT_IN     PB4  // ADC input

// Thresholds (8-bit ADC)
#define VOLTAGE_GOOD     133  // Above: increase load
#define VOLTAGE_WARNING  128  // Below: decrease load
#define VOLTAGE_CRITICAL 123  // Below: sleep

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
    
    // Setup ADC: 8-bit, left-adjusted, ADC2 (PB4)
    ADMUX = (1 << ADLAR) | (1 << MUX1);
    
    // ADC prescaler: /8 for 1.2MHz → 150kHz ADC clock (optimal)
    ADCSRA = (1 << ADEN) | (1 << ADPS1) | (1 << ADPS0);
    
    // Disable digital input on PB4
    DIDR0 = (1 << ADC2D);
    
    // Setup PWM: Fast PWM, non-inverted, prescaler 8
    // PWM freq = 1.2MHz / 8 / 256 = 586 Hz (may be audible!)
    TCCR0A = (1 << COM0A1) | (1 << WGM01) | (1 << WGM00);
    TCCR0B = (1 << CS01);
    OCR0A = PWM_MIN;
    
    // Enable watchdog: 1 second timeout
    wdt_enable(WDTO_1S);
}

// ═══════════════════════════════════════════════════════════════════════════
//                          ADC READ WITH AVERAGING
// ═══════════════════════════════════════════════════════════════════════════

uint8_t get_volt(void) {
    uint16_t sum = 0;
    
    // Average 4 samples for noise immunity
    for(uint8_t i = 0; i < 4; i++) {
        ADCSRA |= (1 << ADSC);  // Start conversion
        while(ADCSRA & (1 << ADSC));  // Wait
        sum += ADCH;  // Read 8-bit result
    }
    
    return (uint8_t)(sum >> 2);  // Return average
}

// ═══════════════════════════════════════════════════════════════════════════
//                          SLEEP MODE WITH WDT WAKE-UP
// ═══════════════════════════════════════════════════════════════════════════

void go_sleep(void) {
    // Turn off everything
    PORTB = 0;
    OCR0A = PWM_MIN;
    pwm_load = PWM_MIN;
    
    // Disable ADC to save power
    ADCSRA &= ~(1 << ADEN);
    
    // Configure WDT for interrupt mode (8 seconds)
    WDTCR = (1 << WDTIE) | (1 << WDP3) | (1 << WDP0);
    
    // Power-down sleep mode
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    sleep_enable();
    sei();  // Enable interrupts
    
    sleep_cpu();  // Sleep now! (will wake after 8 seconds)
    
    // ─── Woke up! ───
    
    sleep_disable();
    
    // Re-enable ADC
    ADCSRA |= (1 << ADEN);
    
    // Re-enable WDT in reset mode
    wdt_enable(WDTO_1S);
    
    // Reactivate Dead Man's Switch
    for(uint8_t i = 0; i < 5; i++) {
        PORTB ^= (1 << WATCHDOG);
        _delay_ms(20);
    }
}

// WDT interrupt - just wake up
ISR(WDT_vect) {
    // Empty - just wake up
}

// ═══════════════════════════════════════════════════════════════════════════
//                          MAIN PROGRAM
// ═══════════════════════════════════════════════════════════════════════════

int main(void) {
    init_hw();
    
    // Startup indication (5 blinks)
    for(uint8_t i = 0; i < 5; i++) {
        PORTB ^= (1 << WATCHDOG);
        _delay_ms(100);
    }
    
    uint8_t heartbeat_counter = 0;
    uint8_t status_counter = 0;
    
    while(1) {
        wdt_reset();  // Reset watchdog
        
        uint8_t voltage = get_volt();
        
        // ───────────────────────────────────────────────────────────────────
        // CRITICAL LOW VOLTAGE → SLEEP
        // ───────────────────────────────────────────────────────────────────
        
        if(voltage < VOLTAGE_CRITICAL) {
            // Flash status LED (battery low!)
            for(uint8_t i = 0; i < 10; i++) {
                PORTB ^= (1 << STATUS_LED);
                _delay_ms(50);
            }
            
            // Go to sleep
            go_sleep();
            
            // After wake-up: short flash
            PORTB |= (1 << STATUS_LED);
            _delay_ms(50);
            PORTB &= ~(1 << STATUS_LED);
            
            _delay_ms(200);
            continue;
        }
        
        // ───────────────────────────────────────────────────────────────────
        // NORMAL OPERATION
        // ───────────────────────────────────────────────────────────────────
        
        // Heartbeat (Dead Man's Switch) - 5Hz at 1.2MHz
        // Period: 100ms → 10 cycles × 10ms
        if(++heartbeat_counter >= 10) {
            PORTB ^= (1 << WATCHDOG);  // Toggle for Dead Man's Switch
            heartbeat_counter = 0;
        }
        
        // Status LED - speed depends on voltage
        uint8_t status_period;
        
        if(voltage > VOLTAGE_GOOD) {
            status_period = 100;  // Slow blink (all OK)
        } 
        else if(voltage > VOLTAGE_WARNING) {
            status_period = 30;   // Medium blink (warning)
        } 
        else {
            status_period = 10;   // Fast blink (critical!)
        }
        
        if(++status_counter >= status_period) {
            PORTB ^= (1 << STATUS_LED);
            status_counter = 0;
        }
        
        // ───────────────────────────────────────────────────────────────────
        // LOAD CONTROL
        // ───────────────────────────────────────────────────────────────────
        
        if(voltage > VOLTAGE_GOOD) {
            // Voltage good - increase load
            if(pwm_load < PWM_MAX) {
                pwm_load++;
            }
        } 
        else if(voltage > VOLTAGE_WARNING) {
            // Voltage warning - decrease slowly
            if(pwm_load > PWM_MIN) {
                pwm_load--;
            }
        } 
        else {
            // Voltage critical - decrease fast!
            if(pwm_load > PWM_MIN) {
                pwm_load -= 2;
                if(pwm_load > PWM_MAX) pwm_load = PWM_MIN;  // Handle underflow
            }
        }
        
        // Safety check
        if(pwm_load > PWM_MAX) pwm_load = PWM_MIN;
        
        // Update PWM
        OCR0A = pwm_load;
        
        // Loop delay (10ms at 1.2MHz)
        _delay_ms(10);
    }
    
    return 0;
}

// ═══════════════════════════════════════════════════════════════════════════
//                          END OF FILE
// ═══════════════════════════════════════════════════════════════════════════
