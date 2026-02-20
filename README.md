═══════════════════════════════════════════════════════════════════════════
 *              ATtiny13 BATTERY PROTECTION SYSTEM - ULTIMATE EDITION
 *                          WITH HARDWARE WATCHDOG
 * ═══════════════════════════════════════════════════════════════════════════
 * 
 * Author: Viktor Nichyporuk
 * Date: 2026
 * Version: 2.0 - ULTIMATE with Dead Man's Switch
 * MCU: ATtiny13A
 * Clock: 9.6 MHz (internal RC oscillator)
 * Flash usage: ~720 bytes / 1024 bytes
 * 
 * ═══════════════════════════════════════════════════════════════════════════
 *                              DESCRIPTION
 * ═══════════════════════════════════════════════════════════════════════════
 * 
 * Professional battery protection controller with:
 * - Dual PWM load control (0-250 PWM on each channel)
 * - 8-bit ADC battery voltage monitoring
 * - Automatic sleep mode when voltage < threshold
 * - Visual status indication via two LEDs
 * - SOFTWARE watchdog timer (WDT) protection
 * - HARDWARE watchdog (Dead Man's Switch) - UNIQUE FEATURE!
 * - Ultra-low power consumption in sleep (~4µA)
 * 
 * ═══════════════════════════════════════════════════════════════════════════
 *                              PIN CONFIGURATION
 * ═══════════════════════════════════════════════════════════════════════════
 * 
 *                           ATtiny13A DIP-8
 *                          ┌─────────────┐
 *                   RESET ─┤1  PB5   VCC├─ 8  VCC (+5V)
 *                          │             │
 *          Status LED (S) ─┤2  PB3   PB2├─ 7  Heartbeat + HW Watchdog ⚡
 *                          │             │
 *      Battery Voltage (A) ─┤3  PB4   PB1├─ 6  PWM Load 2 (L2)
 *                          │             │
 *                     GND ─┤4  GND   PB0├─ 5  PWM Load 1 (L1)
 *                          └─────────────┘
 * 
 * ═══════════════════════════════════════════════════════════════════════════
 *                    HARDWARE CONNECTIONS - ULTIMATE VERSION
 * ═══════════════════════════════════════════════════════════════════════════
 * 
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │  POWER SUPPLY WITH FILTERING                                            │
 * └─────────────────────────────────────────────────────────────────────────┘
 * 
 *  Vbat+ ──┤>|──[Ferrite]──┬──[100µF]──┬──[100nF]──┬── 5V Regulator IN
 *        1N5819            │           │           │   (7805 or LM2596)
 *       (Reverse          GND         GND      [TVS 5.1V]
 *       Protection)                              │
 *                                                GND
 * 
 *  5V Regulator OUT ──┬── ATtiny VCC (Pin 8)
 *                     │
 *                     └──[100µF]──┬──[100nF]── GND
 *                                 │
 *                                GND
 * 
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │  ⚡ DEAD MAN'S SWITCH - HARDWARE WATCHDOG (P-MOSFET High-Side) ⚡        │
 * └─────────────────────────────────────────────────────────────────────────┘
 * 
 *  This is the KILLER FEATURE! Hardware protection that works even if
 *  software completely hangs or crashes!
 * 
 *                    ┌─── Optional LED for visual indication
 *                    │
 *  PB2 (Pin 7) ──────┼──[1kΩ]──┬──[10µF]──┬── Gate P-MOSFET (IRF9540)
 *                    │         │          │
 *                 [LED Red]  [10kΩ]      GND
 *                    │         │
 *                 [1kΩ]       GND
 *                    │
 *                   GND
 * 
 *  P-MOSFET Connection (IRF9540 or similar):
 * 
 *    Vbat+ ────────────────┤Source (S)
 *                          │
 *                        Gate (G) ← from PB2 RC circuit
 *                          │
 *                        Drain (D)├─── VCC_LOADS (power to load MOSFETs)
 *                          │
 *                      [100µF]
 *                          │
 *                         GND
 * 
 *  ┌────────────────────────────────────────────────────────────────────┐
 *  │  HOW DEAD MAN'S SWITCH WORKS:                                      │
 *  ├────────────────────────────────────────────────────────────────────┤
 *  │  1. PB2 toggles at 5Hz (every 100ms)                              │
 *  │  2. Each HIGH pulse charges 10µF capacitor through 1kΩ            │
 *  │  3. Capacitor voltage keeps P-MOSFET gate LOW → MOSFET ON         │
 *  │  4. VCC_LOADS powered → loads can operate                         │
 *  │                                                                    │
 *  │  IF ATTINY HANGS (software crash, infinite loop, etc.):           │
 *  │  5. PB2 stops toggling (stuck HIGH or LOW)                        │
 *  │  6. Capacitor discharges through 10kΩ resistor                    │
 *  │  7. Time constant: τ = R×C = 10kΩ × 10µF = 100ms                  │
 *  │  8. After ~300ms (3τ): Gate voltage rises                         │
 *  │  9. P-MOSFET turns OFF → VCC_LOADS disconnected                   │
 *  │  10. ALL LOADS SAFELY POWERED DOWN! ✅                             │
 *  │                                                                    │
 *  │  AUTOMATIC RECOVERY:                                               │
 *  │  11. WDT (1 second) resets ATtiny                                 │
 *  │  12. PB2 starts toggling again                                    │
 *  │  13. Capacitor charges → MOSFET ON → power restored               │
 *  │  14. System back to normal operation! 🔄                           │
 *  └────────────────────────────────────────────────────────────────────┘
 * 
 *  Component Selection:
 *    - IRF9540: P-channel MOSFET
 *      • Vds(max) = -100V
 *      • Id(max) = -23A (plenty for loads!)
 *      • Vgs(th) = -2V to -4V
 *      • Rds(on) = 0.2Ω @ Vgs=-10V
 *    - 10µF: Timing capacitor (ceramic or electrolytic, 16V+)
 *    - 10kΩ: Discharge resistor (1/4W)
 *    - 1kΩ: Charge current limiter (1/4W)
 *    - Optional: Red LED + 1kΩ for visual heartbeat
 * 
 *  Timing Analysis:
 *    PB2 toggle period: 100ms (5Hz)
 *    Charge time (through 1kΩ): ~10ms to reach threshold
 *    Discharge time (through 10kΩ): ~100ms time constant
 *    Cutoff time: ~300ms after PB2 stops
 *    WDT timeout: 1000ms
 *    Recovery time: ~1.3 seconds total (WDT + startup)
 * 
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │  VOLTAGE DIVIDER FOR BATTERY MONITORING                                 │
 * └─────────────────────────────────────────────────────────────────────────┘
 * 
 *  Vbat+ ──[R1: 10kΩ]──┬──[R2: 3.3kΩ]── GND
 *                      │
 *                    PB4 (Pin 3, ADC2)
 * 
 * Calculation:
 *   Vadc = Vbat × R2 / (R1 + R2)
 *   Vadc = Vbat × 3.3k / 13.3k = Vbat × 0.248
 * 
 * Example for 12V battery:
 *   Vadc = 12V × 0.248 = 2.98V (safe for ATtiny)
 *   ADC reading = 2.98V / 5V × 255 = 152 (8-bit)
 * 
 * Threshold calculation:
 *   For 10.5V minimum (12V battery):
 *   Vadc = 10.5V × 0.248 = 2.6V
 *   ADC = 2.6V / 5V × 255 = 133
 *   Use threshold = 128 (with margin)
 * 
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │  STATUS LED (PB3)                                                       │
 * └─────────────────────────────────────────────────────────────────────────┘
 * 
 *  PB3 (Pin 2) ──[LED Green/Yellow]──[1kΩ]── GND
 * 
 * LED Polarity: Anode to PB3, Cathode to resistor
 * Current: ~2mA (safe for ATtiny)
 * 
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │  PWM LOAD CONTROL (via N-Channel MOSFETs)                               │
 * │  ⚠️ POWERED FROM VCC_LOADS (via Dead Man's Switch!)                     │
 * └─────────────────────────────────────────────────────────────────────────┘
 * 
 *  PB0 (Pin 5) ──[1kΩ]──┬──[10kΩ to GND]──┤Gate
 *                       │                  │ N-MOSFET (IRLZ44N)
 *                    [100nF]         Load 1├┤Drain
 *                       │                  │
 *                      GND          VCC_LOADS┘ ← from Dead Man's Switch!
 *                                           │
 *                                      GND (Source)
 * 
 *  PB1 (Pin 6) ──[1kΩ]──┬──[10kΩ to GND]──┤Gate
 *                       │                  │ N-MOSFET (IRLZ44N)
 *                    [100nF]         Load 2├┤Drain
 *                       │                  │
 *                      GND          VCC_LOADS┘ ← from Dead Man's Switch!
 *                                           │
 *                                      GND (Source)
 * 
 * Components:
 *   - IRLZ44N: Logic-level N-MOSFET (Vgs(th) = 1-2V, Id = 47A max)
 *   - 1kΩ: Gate series resistor (limits current, reduces ringing)
 *   - 10kΩ: Pull-down resistor (ensures MOSFET off when ATtiny resets)
 *   - 100nF: Gate-Source capacitor (reduces switching noise)
 * 
 * PWM Frequency: 9.6MHz / 8 / 256 = 4.7kHz
 * PWM Resolution: 8-bit (0-255, limited to 0-250 in software)
 * 
 * ═══════════════════════════════════════════════════════════════════════════
 *                    COMPLETE SYSTEM BLOCK DIAGRAM
 * ═══════════════════════════════════════════════════════════════════════════
 * 
 *                         ┌─────────────────┐
 *                         │   Battery       │
 *                         │   (12V)         │
 *                         └────────┬────────┘
 *                                  │
 *                         ┌────────▼────────┐
 *                         │  Protection     │
 *                         │  (Diode, TVS)   │
 *                         └────────┬────────┘
 *                                  │
 *                         ┌────────▼────────┐
 *                         │  Voltage        │
 *                         │  Divider        │───► PB4 (ADC)
 *                         └────────┬────────┘
 *                                  │
 *                         ┌────────▼────────┐
 *                         │  5V Regulator   │
 *                         └────────┬────────┘
 *                                  │
 *                         ┌────────▼────────┐
 *                         │   ATtiny13      │
 *                         │                 │
 *                         │  PB2 ───────────┼──► Dead Man's Switch
 *                         │  PB3 ───────────┼──► Status LED
 *                         │  PB0 ───────────┼──► PWM Load 1
 *                         │  PB1 ───────────┼──► PWM Load 2
 *                         └─────────────────┘
 *                                  │
 *                         ┌────────▼────────┐
 *                         │  Dead Man's     │
 *                         │  Switch         │
 *                         │  (P-MOSFET)     │
 *                         └────────┬────────┘
 *                                  │
 *                              VCC_LOADS
 *                                  │
 *                         ┌────────┴────────┐
 *                         │                 │
 *                    ┌────▼────┐      ┌────▼────┐
 *                    │ Load 1  │      │ Load 2  │
 *                    │ MOSFET  │      │ MOSFET  │
 *                    └────┬────┘      └────┬────┘
 *                         │                 │
 *                    ┌────▼────┐      ┌────▼────┐
 *                    │ Load 1  │      │ Load 2  │
 *                    │ Device  │      │ Device  │
 *                    └─────────┘      └─────────┘
 * 
 * ═══════════════════════════════════════════════════════════════════════════
 *                          LED SIGNALIZATION MODES
 * ═══════════════════════════════════════════════════════════════════════════
 * 
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │  MODE 1: EXCELLENT VOLTAGE (v > 133)                                    │
 * └─────────────────────────────────────────────────────────────────────────┘
 * 
 *  PB2 (Heartbeat):  ██░░██░░██░░██░░  (5Hz fast toggle)
 *  PB3 (Status):     ████████░░░░░░░░  (0.5Hz slow blink)
 * 
 *  Meaning: "All good! Battery fully charged, system operating normally"
 *  Action: Gradually increasing load (PWM++)
 *  Dead Man's Switch: Active, capacitor constantly recharged
 * 
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │  MODE 2: WARNING VOLTAGE (128 < v < 133)                                │
 * └─────────────────────────────────────────────────────────────────────────┘
 * 
 *  PB2 (Heartbeat):  ██░░██░░██░░██░░  (5Hz fast toggle)
 *  PB3 (Status):     ██░░██░░██░░██░░  (1.5Hz medium blink)
 * 
 *  Meaning: "Warning! Battery voltage dropping, reducing load"
 *  Action: Slowly decreasing load (PWM--)
 *  Dead Man's Switch: Active, capacitor constantly recharged
 * 
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │  MODE 3: CRITICAL VOLTAGE (123 < v < 128)                               │
 * └─────────────────────────────────────────────────────────────────────────┘
 * 
 *  PB2 (Heartbeat):  ██░░██░░██░░██░░  (5Hz fast toggle)
 *  PB3 (Status):     █░█░█░█░█░█░█░█░  (5Hz fast blink - ALERT!)
 * 
 *  Meaning: "Critical! Battery almost empty, emergency load reduction"
 *  Action: Rapidly decreasing load (PWM -= 2)
 *  Dead Man's Switch: Active, capacitor constantly recharged
 * 
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │  MODE 4: SLEEP MODE (v < 123)                                           │
 * └─────────────────────────────────────────────────────────────────────────┘
 * 
 *  On power-up or after wake-up:
 *    1. Measure voltage
 *    2. If v < 123:
 *       PB3: █░░░░░░░░░░░░░░░  (50ms short flash)
 *       PB2: ░░░░░░░░░░░░░░░░  (OFF)
 *       Then: Enter sleep for 8 seconds
 * 
 *  PB2 (Heartbeat):  ░░░░░░░░░░░░░░░░  (OFF - sleep mode)
 *  PB3 (Status):     ░░░░░░░░░░░░░░░░  (OFF - sleep mode)
 * 
 *  Meaning: "Battery critically low! Sleeping to save power"
 *  Action: All loads OFF, MCU in power-down mode (~4µA consumption)
 *  Dead Man's Switch: Inactive, but loads already off via software
 *  
 *  After 8 seconds: Wake up → Measure → Flash → Sleep again (if still low)
 * 
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │  MODE 5: SYSTEM HANG DETECTED (Dead Man's Switch triggered!)            │
 * └─────────────────────────────────────────────────────────────────────────┘
 * 
 *  PB2 (Heartbeat):  ████████████████  (STUCK HIGH or LOW)
 *  PB3 (Status):     ████████████████  (STUCK - whatever state)
 * 
 *  What happens:
 *    1. PB2 stops toggling (software hung)
 *    2. After ~300ms: Dead Man's Switch cuts power to loads
 *    3. After ~1000ms: WDT resets ATtiny
 *    4. System restarts, PB2 toggles again
 *    5. Dead Man's Switch re-enables loads
 *    6. Back to normal operation!
 * 
 *  Meaning: "EMERGENCY! Software crashed, hardware protection activated!"
 *  Action: Hardware automatically cuts power, then auto-recovery
 *  This is the KILLER FEATURE that saves your battery! 🛡️
 * 
 * ═══════════════════════════════════════════════════════════════════════════
 *                          POWER CONSUMPTION
 * ═══════════════════════════════════════════════════════════════════════════
 * 
 *  Active Mode (v > 123):
 *    ATtiny13:        ~3.0 mA  (at 9.6MHz)
 *    LED PB2:         ~2.0 mA  (heartbeat, optional)
 *    LED PB3:         ~2.0 mA  (status, average)
 *    P-MOSFET:        ~0.1 mA  (gate drive)
 *    PWM + Loads:     depends on load
 *    ─────────────────────────────────
 *    Total:           ~7 mA + loads
 * 
 *  Sleep Mode (v < 123):
 *    ATtiny13:        ~4 µA    (power-down mode)
 *    LEDs:            0 mA     (off)
 *    P-MOSFET:        ~0 µA    (off, no gate current)
 *    PWM:             0 mA     (off)
 *    ─────────────────────────────────
 *    Total:           ~4 µA    (1750x less!)
 * 
 *  Dead Man's Switch overhead:
 *    Quiescent:       ~0.1 mA  (negligible)
 * 
 * ═══════════════════════════════════════════════════════════════════════════
 *                          SAFETY FEATURES
 * ═══════════════════════════════════════════════════════════════════════════
 * 
 *  1. SOFTWARE Watchdog Timer (WDT):
 *     - 1 second timeout in active mode
 *     - Resets MCU if main loop hangs
 *     - Used as wake-up timer in sleep mode (8 seconds)
 * 
 *  2. HARDWARE Watchdog (Dead Man's Switch): ⚡ NEW! ⚡
 *     - Independent of software
 *     - Cuts power to loads if PB2 stops toggling
 *     - ~300ms response time
 *     - Automatic recovery after WDT reset
 *     - ULTIMATE PROTECTION!
 * 
 *  3. PWM Limiting:
 *     - Maximum PWM = 250 (not 255) for safety margin
 *     - Corrupted values (> 250) reset to 0
 * 
 *  4. ADC Averaging:
 *     - 4 samples averaged to reduce noise
 *     - Prevents false triggering from voltage spikes
 * 
 *  5. Hysteresis:
 *     - Sleep threshold: 123 (128 - 5)
 *     - Wake threshold: 128
 *     - Prevents oscillation at threshold boundary
 * 
 *  6. Hardware Protection:
 *     - Reverse polarity protection (diode)
 *     - Overvoltage protection (TVS diode)
 *     - EMI filtering (ferrite bead + capacitors)
 *     - MOSFET pull-down resistors (safe state on reset)
 * 
 * ═══════════════════════════════════════════════════════════════════════════
 *                          BILL OF MATERIALS (BOM)
 * ═══════════════════════════════════════════════════════════════════════════
 * 
 *  Semiconductors:
 *    1× ATtiny13A-PU              ~$0.50
 *    1× IRF9540 (P-MOSFET)        ~$0.80  ← Dead Man's Switch
 *    2× IRLZ44N (N-MOSFET)        ~$1.20  (2× $0.60)
 *    1× 1N5819 (Schottky)         ~$0.10
 *    1× TVS 5.1V                  ~$0.15
 *    1× 7805 or LM2596 (5V reg)   ~$0.50
 * 
 *  Passives:
 *    Resistors (1/4W):
 *      2× 1kΩ (LED current limit)  ~$0.04
 *      3× 1kΩ (MOSFET gate)        ~$0.06
 *      1× 10kΩ (voltage divider)   ~$0.02
 *      1× 3.3kΩ (voltage divider)  ~$0.02
 *      3× 10kΩ (MOSFET pulldown)   ~$0.06
 *      1× 10kΩ (DMS discharge)     ~$0.02  ← Dead Man's Switch
 * 
 *    Capacitors:
 *      3× 100µF electrolytic       ~$0.30
 *      4× 100nF ceramic            ~$0.20
 *      1× 10µF (DMS timing)        ~$0.10  ← Dead Man's Switch
 *      2× 100nF (MOSFET gate)      ~$0.10
 * 
 *    LEDs:
 *      1× Red LED (5mm)            ~$0.10
 *      1× Green/Yellow LED (5mm)   ~$0.10
 * 
 *    Misc:
 *      1× Ferrite Bead             ~$0.05
 *      1× DIP-8 socket (optional)  ~$0.10
 * 
 *  ─────────────────────────────────────────
 *  TOTAL:                          ~$4.52
 * 
 *  Note: Prices are approximate, bulk discounts available
 *  Dead Man's Switch adds only ~$0.92 to total cost!
 * 
 * ═══════════════════════════════════════════════════════════════════════════
 *                          COMPILATION & FLASHING
 * ═══════════════════════════════════════════════════════════════════════════
 * 
 *  Compile:
 *    avr-gcc -mmcu=attiny13 -DF_CPU=9600000UL -Os -o battery.elf battery.c
 *    avr-objcopy -O ihex battery.elf battery.hex
 * 
 *  Check size:
 *    avr-size battery.elf
 * 
 *  Flash:
 *    avrdude -c usbasp -p attiny13 -U flash:w:battery.hex:i
 * 
 *  Set fuses (9.6MHz internal, BOD 4.3V):
 *    avrdude -c usbasp -p attiny13 -U lfuse:w:0x6a:m -U hfuse:w:0xfb:m
 * 
 * ═══════════════════════════════════════════════════════════════════════════
 */
