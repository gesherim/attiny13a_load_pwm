/*
 * ═══════════════════════════════════════════════════════════════════════════
 *      ATtiny13 Battery Protection - WITH UART TX DEBUG
 * ═══════════════════════════════════════════════════════════════════════════
 * 
 * Features:
 *   ✅ Brown-out Detection (BOD) check
 *   ✅ UART TX on PB1 for debugging (9600 baud @ 1.2MHz)
 *   ✅ Single PWM load on PB0
 *   ✅ Full diagnostics output via serial
 * 
 * Pin configuration:
 *   PB0 - PWM Load (single load)
 *   PB1 - UART TX (debug output) 📡
 *   PB2 - Heartbeat / Dead Man's Switch
 *   PB3 - Status LED
 *   PB4 - ADC (voltage sense)
 * 
 * UART settings:
 *   Baud: 9600 (at 1.2MHz: bit time = 125µs)
 *   Format: 8N1 (8 data bits, no parity, 1 stop bit)
 *   TX only (no RX)
 * 
 * Connect to PC:
 *   PB1 → USB-UART adapter RX
 *   GND → USB-UART adapter GND
 *   Open terminal: 9600 baud, 8N1
 * 
 * ═══════════════════════════════════════════════════════════════════════════
 */

#define F_CPU 1200000UL

#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/delay.h>

// Pin definitions
#define LOAD_PIN    PB0  // PWM Load
#define UART_TX     PB1  // UART TX for debug 📡
#define WATCHDOG    PB2  // Heartbeat + Dead Man's Switch
#define STATUS_LED  PB3  // Status LED
#define VOLT_IN     PB4  // ADC input

// Thresholds (8-bit ADC: 0-255)
#define VOLTAGE_GOOD     133
#define VOLTAGE_WARNING  128
#define VOLTAGE_CRITICAL 123

// PWM limits
#define PWM_MAX 250
#define PWM_MIN 0

// UART settings (9600 baud @ 1.2MHz)
#define UART_BAUD 9600
#define UART_BIT_TIME_US (1000000UL / UART_BAUD)  // 104µs per bit

// Global variables
uint8_t pwm_load = PWM_MIN;
uint8_t reset_source = 0;

// ═══════════════════════════════════════════════════════════════════════════
//                          SOFTWARE UART TX
// ═══════════════════════════════════════════════════════════════════════════

/**
 * Initialize UART TX pin
 */
void uart_init(void) {
    DDRB |= (1 << UART_TX);      // Set TX as output
    PORTB |= (1 << UART_TX);     // Set TX high (idle state)
}

/**
 * Send one byte via software UART
 * Format: 8N1 (8 data bits, no parity, 1 stop bit)
 * 
 * Timing for 9600 baud @ 1.2MHz:
 *   Bit time = 1/9600 = 104µs
 *   At 1.2MHz: 104µs = 125 CPU cycles
 */
void uart_tx_byte(uint8_t data) {
    uint8_t i;
    
    // Disable interrupts for precise timing
    cli();
    
    // Start bit (LOW)
    PORTB &= ~(1 << UART_TX);
    _delay_us(UART_BIT_TIME_US);
    
    // Data bits (LSB first)
    for(i = 0; i < 8; i++) {
        if(data & 0x01) {
            PORTB |= (1 << UART_TX);   // Send 1
        } else {
            PORTB &= ~(1 << UART_TX);  // Send 0
        }
        _delay_us(UART_BIT_TIME_US);
        data >>= 1;
    }
    
    // Stop bit (HIGH)
    PORTB |= (1 << UART_TX);
    _delay_us(UART_BIT_TIME_US);
    
    // Re-enable interrupts
    sei();
}

/**
 * Send string from SRAM
 */
void uart_tx_str(const char *str) {
    while(*str) {
        uart_tx_byte(*str++);
    }
}

/**
 * Send string from PROGMEM (Flash)
 */
void uart_tx_str_P(const char *str) {
    char c;
    while((c = pgm_read_byte(str++))) {
        uart_tx_byte(c);
    }
}

/**
 * Send unsigned 8-bit number as decimal
 */
void uart_tx_u8(uint8_t num) {
    char buf[4];
    uint8_t i = 0;
    
    // Convert to string (reverse order)
    do {
        buf[i++] = '0' + (num % 10);
        num /= 10;
    } while(num > 0);
    
    // Send in correct order
    while(i > 0) {
        uart_tx_byte(buf[--i]);
    }
}

/**
 * Send unsigned 8-bit number as hex
 */
void uart_tx_hex(uint8_t num) {
    const char hex[] = "0123456789ABCDEF";
    uart_tx_byte(hex[num >> 4]);
    uart_tx_byte(hex[num & 0x0F]);
}

/**
 * Send newline
 */
void uart_tx_nl(void) {
    uart_tx_byte('\r');
    uart_tx_byte('\n');
}

// ═══════════════════════════════════════════════════════════════════════════
//                    RESET SOURCE DETECTION WITH UART DEBUG
// ═══════════════════════════════════════════════════════════════════════════

// Strings in PROGMEM to save RAM
const char str_banner[] PROGMEM = "\r\n=== ATtiny13 Battery Protection ===\r\n";
const char str_reset[] PROGMEM = "Reset source: 0x";
const char str_por[] PROGMEM = " [Power-On]\r\n";
const char str_ext[] PROGMEM = " [External]\r\n";
const char str_bod[] PROGMEM = " [Brown-Out!]\r\n";
const char str_wdt[] PROGMEM = " [Watchdog]\r\n";
const char str_voltage[] PROGMEM = "Voltage: ";
const char str_pwm[] PROGMEM = "PWM: ";
const char str_sleep[] PROGMEM = "Going to sleep...\r\n";
const char str_wake[] PROGMEM = "Woke up!\r\n";

void check_reset_source(void) {
    reset_source = MCUSR;
    MCUSR = 0;
    wdt_disable();
    
    // Initialize UART first for debug output
    uart_init();
    
    // Small delay to let UART stabilize
    _delay_ms(10);
    
    // Send banner
    uart_tx_str_P(str_banner);
    
    // Send reset source
    uart_tx_str_P(str_reset);
    uart_tx_hex(reset_source);
    
    // Decode reset source
    if(reset_source & (1 << PORF)) {
        uart_tx_str_P(str_por);
    }
    if(reset_source & (1 << EXTRF)) {
        uart_tx_str_P(str_ext);
    }
    if(reset_source & (1 << BORF)) {
        uart_tx_str_P(str_bod);
        
        // Visual indication on LEDs
        DDRB |= (1 << STATUS_LED) | (1 << WATCHDOG);
        for(uint8_t i = 0; i < 10; i++) {
            PORTB ^= (1 << STATUS_LED) | (1 << WATCHDOG);
            _delay_ms(50);
        }
        PORTB &= ~((1 << STATUS_LED) | (1 << WATCHDOG));
    }
    if(reset_source & (1 << WDRF)) {
        uart_tx_str_P(str_wdt);
    }
    
    uart_tx_nl();
}

// ═══════════════════════════════════════════════════════════════════════════
//                          HARDWARE INITIALIZATION
// ═══════════════════════════════════════════════════════════════════════════

void init_hw(void) {
    // Set outputs: PB0 (PWM), PB1 (UART), PB2 (WD), PB3 (LED)
    DDRB = (1 << LOAD_PIN) | (1 << UART_TX) | (1 << WATCHDOG) | (1 << STATUS_LED);
    PORTB = (1 << UART_TX);  // UART TX idle high
    
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
//                          ADC READ
// ═══════════════════════════════════════════════════════════════════════════

uint8_t get_volt(void) {
    ADCSRA |= (1 << ADSC);
    while(ADCSRA & (1 << ADSC));
    return ADCH;
}

// ═══════════════════════════════════════════════════════════════════════════
//                          SLEEP MODE
// ═══════════════════════════════════════════════════════════════════════════

void go_sleep(void) {
    // Send debug message
    uart_tx_str_P(str_sleep);
    _delay_ms(10);  // Wait for UART to finish
    
    // Fully disable PWM
    TCCR0A = 0;
    TCCR0B = 0;
    
    // Turn off all outputs except UART TX (keep high)
    PORTB = (1 << UART_TX);
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
    
    // Send debug message
    _delay_ms(10);
    uart_tx_str_P(str_wake);
    
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
    // Check reset source (initializes UART)
    check_reset_source();
    
    // If BOD occurred, check voltage immediately
    if(reset_source & (1 << BORF)) {
        DDRB = (1 << UART_TX) | (1 << STATUS_LED) | (1 << WATCHDOG);
        PORTB = (1 << UART_TX);
        ADMUX = (1 << ADLAR) | (1 << MUX1);
        ADCSRA = (1 << ADEN) | (1 << ADPS1) | (1 << ADPS0);
        DIDR0 = (1 << ADC2D);
        
        _delay_ms(10);
        
        uint8_t voltage = get_volt();
        
        uart_tx_str_P(str_voltage);
        uart_tx_u8(voltage);
        uart_tx_str_P(PSTR(" (after BOD)\r\n"));
        
        if(voltage < VOLTAGE_CRITICAL) {
            uart_tx_str_P(PSTR("CRITICAL! Going to sleep...\r\n"));
            _delay_ms(10);
            
            // Sleep until voltage recovers
            ADCSRA &= ~(1 << ADEN);
            WDTCR = (1 << WDTIE) | (1 << WDP3) | (1 << WDP0);
            
            while(1) {
                set_sleep_mode(SLEEP_MODE_PWR_DOWN);
                sleep_enable();
                sei();
                sleep_cpu();
                sleep_disable();
                
                ADCSRA |= (1 << ADEN);
                _delay_ms(10);
                voltage = get_volt();
                
                if(voltage >= VOLTAGE_CRITICAL) {
                    uart_tx_str_P(PSTR("Voltage recovered!\r\n"));
                    break;
                }
                
                PORTB ^= (1 << STATUS_LED);
                _delay_ms(50);
                PORTB ^= (1 << STATUS_LED);
                
                ADCSRA &= ~(1 << ADEN);
            }
        }
    }
    
    // Normal hardware initialization
    init_hw();
    
    uart_tx_str_P(PSTR("System started!\r\n"));
    uart_tx_nl();
    
    // Startup indication
    for(uint8_t i = 0; i < 5; i++) {
        PORTB ^= (1 << WATCHDOG);
        _delay_ms(100);
    }
    
    // Main loop
    uint8_t heartbeat_counter = 0;
    uint8_t status_counter = 0;
    uint8_t debug_counter = 0;
    
    while(1) {
        wdt_reset();
        
        uint8_t voltage = get_volt();
        
        // Send debug info every 100 cycles (1 second)
        if(++debug_counter >= 100) {
            uart_tx_str_P(str_voltage);
            uart_tx_u8(voltage);
            uart_tx_str_P(PSTR("  "));
            uart_tx_str_P(str_pwm);
            uart_tx_u8(pwm_load);
            uart_tx_nl();
            debug_counter = 0;
        }
        
        // Check for critical low voltage
        if(voltage < VOLTAGE_CRITICAL) {
            uart_tx_str_P(PSTR("CRITICAL VOLTAGE!\r\n"));
            
            for(uint8_t i = 0; i < 10; i++) {
                PORTB ^= (1 << STATUS_LED);
                _delay_ms(50);
            }
            
            go_sleep();
            
            PORTB |= (1 << STATUS_LED);
            _delay_ms(50);
            PORTB &= ~(1 << STATUS_LED);
            
            _delay_ms(200);
            continue;
        }
        
        // Heartbeat (Dead Man's Switch)
        if(++heartbeat_counter >= 10) {
            PORTB ^= (1 << WATCHDOG);
            heartbeat_counter = 0;
        }
        
        // Status LED
        uint8_t status_period;
        
        if(voltage > VOLTAGE_GOOD) {
            status_period = 100;
        } 
        else if(voltage > VOLTAGE_WARNING) {
            status_period = 30;
        } 
        else {
            status_period = 10;
        }
        
        if(++status_counter >= status_period) {
            PORTB ^= (1 << STATUS_LED);
            status_counter = 0;
        }
        
        // Load control
        if(voltage > VOLTAGE_GOOD) {
            if(pwm_load < PWM_MAX) {
                pwm_load++;
            }
        } 
        else if(voltage > VOLTAGE_WARNING) {
            if(pwm_load > PWM_MIN) {
                pwm_load--;
            }
        } 
        else {
            if(pwm_load > PWM_MIN) {
                pwm_load -= 2;
                if(pwm_load > PWM_MAX) pwm_load = PWM_MIN;
            }
        }
        
        if(pwm_load > PWM_MAX) pwm_load = PWM_MIN;
        
        OCR0A = pwm_load;
        
        _delay_ms(10);
    }
    
    return 0;
}

// ═══════════════════════════════════════════════════════════════════════════
//                          END OF FILE
// ═══════════════════════════════════════════════════════════════════════════
