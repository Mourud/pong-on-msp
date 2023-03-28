#include <msp430.h>

#define CS BIT3         // Chip Select line
#define CD BIT1         // Command/Data mode line
#define MOSI BIT0       // Master-out Slave-in
#define SCK BIT2        // Serial clock

// #if 20110706 > __MSPGCC__
// /* A crude delay function. */
// void __delay_cycles( unsigned long int n ) {
//     volatile unsigned int i = n/6;
//     while( i-- ) ;
// }
// #endif

/* Write data to slave device.  Since the LCD panel is
 * write-only, we don't worry about reading any bits.
 * Destroys the data array (normally received data would
 * go in its place). */
void spi_IO( unsigned char data[], int bytes ) {
    int i, n;

    // Set Chip Select low, so LCD panel knows we are talking to it.
    P3OUT &= ~CS;
    __delay_cycles( 5 );

    for( n = 0; n < bytes; n++ ) {
        for( i = 0; i < 8; i++ ) {
            // Put bits on the line, most significant bit first.
            if( data[n] & 0x80 ) {
                P3OUT |= MOSI;
            } else {
                P3OUT &= ~MOSI;
            }
            data[n] <<= 1;

            // Pulse the clock low and wait to send the bit.  According to
            // the data sheet, data is transferred on the rising edge.
            P3OUT &= ~SCK;
            __delay_cycles( 5 );

            // Send the clock back high and wait to set the next bit.  Normally
            // we'd also read the data bits here, but the LCD is write-only.
            P3OUT |= SCK;
            __delay_cycles( 5 );
        }
    }

    // Set Chip Select back high to finish the communication.
    P3OUT |= CS;
}

/* Sets the LCD to command mode, and sends a 7-byte
 * sequence to initialize the panel. */
void init_lcd( void ) {
    unsigned char data[] = {
        0x40, // display start line 0
        0xA1, // SEG reverse
        0xC0, // Normal COM0-COM63
        0xA4, // Disable->Set All Pixel to ON
        0xA6, // Display inverse off
        0xA2, // Set Bias 1/9 (Duty 1/65)
        0x2F, // Booster, Regulator and Follower on
        0x27,
        0x81, // Set contrast
        0x10,
        0xFA, // Set temp compensation ...
        0x90, // ... curve to -0.11 %/degC
        0xAF  // Display On
    };

    P3OUT &= ~CD;       // set for commands

    spi_IO( data, sizeof(data));
}

/* Writes zeros to the contents of display RAM, effectively resetting
 * all of the pixels on the screen.  The screen width is 6*17 = 102 pixels,
 * so I used and array of size 17 and loop 6 times.  Each page in RAM
 * spans 8 pixels vertically, and looping through the 8 pages covers
 * the 8*8 = 64 pixel height of the display. */
void write_zeros( void ) {
    unsigned char zeros[17];
    int i, j, page;

    for( page = 0; page < 8; page++ ) {
        P3OUT &= ~CD;   // set for commands
        zeros[0] = 0xB0 + page; // set page
        zeros[1] = 0x00;        // LSB of column address is 0
        zeros[2] = 0x10;        // MSB of column address is 0
        spi_IO( zeros, 3 );

        P3OUT |= CD;    // set for data
        for( i = 0; i < 6; i++ ) {
            for( j = 0; j < 17; j++ ) zeros[j] = 0x00;

            spi_IO( zeros, sizeof( zeros ));
        }
    }
}

/* Writes ones to the contents of display RAM, effectively setting
 * all of the pixels on the screen.  The screen width is 6*17 = 102 pixels,
 * so I used and array of size 17 and loop 6 times.  Each page in RAM
 * spans 8 pixels vertically, and looping through the 8 pages covers
 * the 8*8 = 64 pixel height of the display. */
void write_ones( void ) {
    unsigned char ones[17];
    int i, j, page;

    for( page = 0; page < 8; page++ ) {
        P3OUT &= ~CD;   // set for commands
        ones[0] = 0xB0 + page;  // set page
        ones[1] = 0x00; // LSB of column address is 0
        ones[2] = 0x10; // MSB of column address is 0
        spi_IO( ones, 3 );

        P3OUT |= CD;    // set for data
        for( i = 0; i < 6; i++ ) {
            for( j = 0; j < 17; j++ ) ones[j] = 0xFF;

            spi_IO( ones, sizeof( ones ));
        }
    }
}

void main( void ) {
    // Stop the watchdog timer so it doesn't reset our chip
    WDTCTL = WDTPW + WDTHOLD;

    // These are the pins we need to drive.
    P3DIR |= SCK + MOSI + CS + CD;

    // De-select the LCD panel and set the clock high.
    P3OUT |= CS + SCK;

    // Pause so everything has time to start up properly.
    __delay_cycles( 5500 );

    // Initialize the LCD panel.
    init_lcd();

    // Blacken and clear the LCD two times.
    while(1){
    write_ones();
    write_zeros();
    write_ones();
    write_zeros();
    }

    for( ;; ) {
        __bis_SR_register( LPM3_bits + GIE );
    }
}
