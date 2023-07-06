#include <msp430.h>

#define NOTE_B0  31
#define NOTE_C1  33
#define NOTE_CS1 35
#define NOTE_D1  37
#define NOTE_DS1 39
#define NOTE_E1  41
#define NOTE_F1  44
#define NOTE_FS1 46
#define NOTE_G1  49
#define NOTE_GS1 52
#define NOTE_A1  55
#define NOTE_AS1 58
#define NOTE_B1  62
#define NOTE_C2  65
#define NOTE_CS2 69
#define NOTE_D2  73
#define NOTE_DS2 78
#define NOTE_E2  82
#define NOTE_F2  87
#define NOTE_FS2 93
#define NOTE_G2  98
#define NOTE_GS2 104
#define NOTE_A2  110
#define NOTE_AS2 117
#define NOTE_B2  123
#define NOTE_C3  131
#define NOTE_CS3 139
#define NOTE_D3  147
#define NOTE_DS3 156
#define NOTE_E3  165
#define NOTE_F3  175
#define NOTE_FS3 185
#define NOTE_G3  196
#define NOTE_GS3 208
#define NOTE_A3  220
#define NOTE_AS3 233
#define NOTE_B3  247
#define NOTE_C4  262
#define NOTE_CS4 277
#define NOTE_D4  294
#define NOTE_DS4 311
#define NOTE_E4  330
#define NOTE_F4  349
#define NOTE_FS4 370
#define NOTE_G4  392
#define NOTE_GS4 415
#define NOTE_A4  440
#define NOTE_AS4 466
#define NOTE_B4  494
#define NOTE_C5  523
#define NOTE_CS5 554
#define NOTE_D5  587
#define NOTE_DS5 622
#define NOTE_E5  659
#define NOTE_F5  698
#define NOTE_FS5 740
#define NOTE_G5  784
#define NOTE_GS5 831
#define NOTE_A5  880
#define NOTE_AS5 932
#define NOTE_B5  988
#define NOTE_C6  1047
#define NOTE_CS6 1109
#define NOTE_D6  1175
#define NOTE_DS6 1245
#define NOTE_E6  1319
#define NOTE_F6  1397
#define NOTE_FS6 1480
#define NOTE_G6  1568
#define NOTE_GS6 1661
#define NOTE_A6  1760
#define NOTE_AS6 1865
#define NOTE_B6  1976
#define NOTE_C7  2093
#define NOTE_CS7 2217
#define NOTE_D7  2349
#define NOTE_DS7 2489
#define NOTE_E7  2637
#define NOTE_F7  2794
#define NOTE_FS7 2960
#define NOTE_G7  3136
#define NOTE_GS7 3322
#define NOTE_A7  3520
#define NOTE_AS7 3729
#define NOTE_B7  3951
#define NOTE_C8  4186
#define NOTE_CS8 4435
#define NOTE_D8  4699
#define NOTE_DS8 4978
/**
 * main.c
 */
void init_PWM()
{
    P1DIR |= BIT2; // Output on Pin 1.2
    P1SEL |= BIT2; // Pin 1.2 selected as PWM
    // microseconds
    TA0CCTL1 = OUTMOD_7; // TA0CCR1 reset/set-high voltage
    // below count, low voltage when past
    TA0CTL = TASSEL_2 + MC_1 + TAIE + ID_0;
    // Timer A control set to SMCLK, 1MHz
    // and count up mode MC_1
}
void reset_msp430()
{
    WDTCTL = 0x6900 | WDTPW | WDTCNTCL;                   // Write watchdog password and clear the timer
    WDTCTL = 0x6900 | WDTPW | WDTCNTCL | WDTIS0 | WDTIS1; // W}
}
void play_music(int sel)
{
    if (sel == 3) // play game over music
    {
        int music[] = {NOTE_G4, NOTE_C4, NOTE_G4};
        int i = 0;
        for (i = 0; i < 3; i++)
        {
            int period = 1000000 / music[i];
            TA0CCR0 = period;
            TA0CCR1 = period / 2;
            // delay for 0.5 seconds
            __delay_cycles(1000000);
        }
    }
    if (sel == 2)
    {
        int music[] = {
          NOTE_C4, NOTE_G4, NOTE_F4, NOTE_G4,
          NOTE_C4, NOTE_G4, NOTE_F4, NOTE_G4,
          NOTE_G4, NOTE_G4, NOTE_C4, NOTE_C4,
          NOTE_C4, NOTE_C4, NOTE_F4, NOTE_G4
        };
        int i = 0;
        for (i = 0; i < 14; i++)
        {
            int period = 1000000 / music[i];
            TA0CCR0 = period;
            TA0CCR1 = period / 2;
            // delay for 0.5 seconds
            __delay_cycles(150000);
        }
    }
    int music[2][3] = {
        {300, 350, 300}, // score
        {261, 261, 392}, // ball_hit
    };
    int i = 0;
    for (i = 0; i < 3; i++)
    {
        int period = 1000000 / music[sel][i];
        TA0CCR0 = period;
        TA0CCR1 = period / 2;
        __delay_cycles(50000);
    }
    TA0CCR0 = 0;
    TA0CCR1 = 0;
}
int main(void)
{
    WDTCTL = WDTPW | WDTHOLD; // stop watchdog timer

    P6REN = BIT0 + BIT1;
    P1REN |= BIT4;
    P1OUT &= ~BIT4; // Enable pull-down resistor for P1.4
    P1IE |= BIT4;   // Enable input at P1.4 as an interrupt
    P1IES |= BIT4;  // Trigger on the rising edge for P1.4

    init_PWM();

    _BIS_SR(LPM0_bits + GIE); // Turn on interrupts and go into the lowest power mode with GPIO enabled
                              // Turn on interrupts and go into the lowest
                              // power mode (the program stops here)
                              // Notice the strange format of the function, it is an "intrinsic"
                              // ie. not part of C; it is specific to this microprocessor

    return 0;
}

// set up interrupt
void __attribute__((interrupt(PORT1_VECTOR))) PORT1_ISR(void) // Port 1 interrupt service routine
{
    P1IFG &= ~BIT4;        // Clear interrupt flag
    __delay_cycles(20000); // Add debounce delay
    int sel = P6IN & (BIT0 + BIT1);
    play_music(sel);
    reset_msp430();
}
