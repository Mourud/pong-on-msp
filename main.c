#include <msp430.h>

#define CS BIT3   // Chip Select line
#define CD BIT1   // Command/Data mode line
#define MOSI BIT0 // Master-out Slave-in
#define SCK BIT2  // Serial clock


unsigned char game_screen[8][102] = {0};
struct ball
{
    int x;
    int y;
    int size;
    int x_vel;
    int y_vel;
};

struct paddle
{
    int x;
    int y;
    int height;
    int width;
    int y_vel;
    int score;
};

int updateScore(struct paddle *paddle);
int getScore(struct paddle *paddle);

// TODO: How to display on LCD
// TODO: Handling inputs

int exit = 0;

int updateScore(struct paddle *paddle)
{
    paddle->score++;
    return 0;
}

int getScore(struct paddle *paddle)
{
    return paddle->score;
}

// void movePaddle(struct paddle *paddle, double force)
//{
//     paddle->x_vel = force;
//     paddle->y_vel = force;
//     paddle->x += paddle->x_vel;
//     paddle->y += paddle->y_vel;
// }

// void gameLoop(){
//     while(!exit){
//         //update ball
//         //update paddle
//         //update score
//         //update display
//     }
// }

int onCollide()
{
}

/* Write data to slave device.  Since the LCD panel is
 * write-only, we don't worry about reading any bits.
 * Destroys the data array (normally received data would
 * go in its place). */
void spi_IO(unsigned char data[], int bytes)
{
    int i, n;

    // Set Chip Select low, so LCD panel knows we are talking to it.
    P3OUT &= ~CS;
    __delay_cycles(1);

    for (n = 0; n < bytes; n++)
    {
        for (i = 0; i < 8; i++)
        {
            // Put bits on the line, most significant bit first.
            if (data[n] & 0x80)
            {
                P3OUT |= MOSI;
            }
            else
            {
                P3OUT &= ~MOSI;
            }
            data[n] <<= 1;

            // Pulse the clock low and wait to send the bit.  According to
            // the data sheet, data is transferred on the rising edge.
            P3OUT &= ~SCK;
            __delay_cycles(1);

            // Send the clock back high and wait to set the next bit.  Normally
            // we'd also read the data bits here, but the LCD is write-only.
            P3OUT |= SCK;
            __delay_cycles(1);
        }
    }

    // Set Chip Select back high to finish the communication.
    P3OUT |= CS;
}

/* Sets the LCD to command mode, and sends a 7-byte
 * sequence to initialize the panel. */
void init_lcd(void)
{
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

    P3OUT &= ~CD; // set for commands

    spi_IO(data, sizeof(data));
}

/* Writes zeros to the contents of display RAM, effectively resetting
 * all of the pixels on the screen.  The screen width is 6*17 = 102 pixels,
 * so I used and array of size 17 and loop 6 times.  Each page in RAM
 * spans 8 pixels vertically, and looping through the 8 pages covers
 * the 8*8 = 64 pixel height of the display. */
void write_zeros(void)
{
    unsigned char zeros[17];
    int i, j, page;

    for (page = 0; page < 8; page++)
    {
        P3OUT &= ~CD;           // set for commands
        zeros[0] = 0xB0 + page; // set page
        zeros[1] = 0x00;        // LSB of column address is 0
        zeros[2] = 0x10;        // MSB of column address is 0
        spi_IO(zeros, 3);

        P3OUT |= CD; // set for data
        for (i = 0; i < 6; i++)
        {
            for (j = 0; j < 17; j++)
                zeros[j] = 0x00;

            spi_IO(zeros, sizeof(zeros));
        }
    }
}

/* Writes ones to the contents of display RAM, effectively setting
 * all of the pixels on the screen.  The screen width is 6*17 = 102 pixels,
 * so I used and array of size 17 and loop 6 times.  Each page in RAM
 * spans 8 pixels vertically, and looping through the 8 pages covers
 * the 8*8 = 64 pixel height of the display. */
void write_ones(void)
{
    unsigned char ones[17];
    int i, j, page;

    for (page = 0; page < 8; page++)
    {
        P3OUT &= ~CD;          // set for commands
        ones[0] = 0xB0 + page; // set page
        ones[1] = 0x00;        // LSB of column address is 0
        ones[2] = 0x10;        // MSB of column address is 0
        spi_IO(ones, 3);

        P3OUT |= CD; // set for data
        for (i = 0; i < 6; i++)
        {
            for (j = 0; j < 17; j++)
                ones[j] = 0xFF;

            spi_IO(ones, sizeof(ones));
        }
    }
}

void draw_rectangle(int x, int y, int width, int height)
{
    int page_start, page_end, i, j, page;
    unsigned char data[17];

    // Calculate the start and end pages for the rectangle
    page_start = y / 8;
    page_end = (y + height) / 8;

    for (page = page_start; page <= page_end; page++)
    {

        P3OUT &= ~CD;                       // set for commands
        data[0] = 0xB0 + page;              // set page
        data[1] = 0x00 + (x & 0x0F);        // LSB of column address
        data[2] = 0x10 + ((x >> 4) & 0x0F); // MSB of column address
        spi_IO(data, 3);

        P3OUT |= CD; // set for data
        for (i = 0; i < width; i++)
        {
            // Calculate the byte to draw based on the current page and y coordinate
            if (page == page_start)
            {
                data[i] |= 0xFF << (y % 8);
            }
            else if (page == page_end)
            {
                data[i] = 0xFF >> (8 - (y + height) % 8);
            }
            else
            {
                data[i] = 0xFF;
            }
        }

        spi_IO(data, width);
    }
}

void clear_rectangle(int x, int y, int width, int height)
{
    int page_start, page_end, i, j, page;
    unsigned char data[17];

    // Calculate the start and end pages for the rectangle
    page_start = y / 8;
    page_end = (y + height) / 8;

    for (page = page_start; page <= page_end; page++)
    {
        P3OUT &= ~CD;                       // set for commands
        data[0] = 0xB0 + page;              // set page
        data[1] = 0x00 + (x & 0x0F);        // LSB of column address
        data[2] = 0x10 + ((x >> 4) & 0x0F); // MSB of column address
        spi_IO(data, 3);

        P3OUT |= CD; // set for data
        for (i = 0; i < width; i++)
        {
            // Calculate the byte to draw based on the current page and y coordinate
            data[i] = 0x00;
        }

        spi_IO(data, width);
    }
}

void test(void)
{
    int x, y;

    for (x = 0; x < 102; x++)
    {
        unsigned char ones[102] = {0};

        for (y = 0; y < 64; y++)
        {
            if (x == y)
            {
                int i, j, page;

                int bit = y % 8;
                page = y / 8;

                P3OUT &= ~CD;          // set for commands
                ones[0] = 0xB0 + page; // set page
                ones[1] = 0x00;        // LSB of column address is 0
                ones[2] = 0x10;        // MSB of column address is 0
                spi_IO(ones, 3);
                unsigned char b = 1;
                b = b << bit;
                unsigned char data = 0;
                data |= b;

                P3OUT |= CD; // set for data

                ones[x] = data;

                spi_IO(ones, sizeof(ones));
                P3OUT &= ~CD;          // set for commands
                ones[0] = 0xB0 + page; // set page
                ones[1] = 0x00;        // LSB of column address is 0
                ones[2] = 0x10;        // MSB of column address is 0
                spi_IO(ones, 3);
                P3OUT |= CD; // set for data
                ones[x] = 0;
                __delay_cycles(100000);
                spi_IO(ones, sizeof(ones));
            }
        }
    }
}

int collides(struct ball *ball, struct paddle *player, struct paddle *computer)
{
    if ((ball->x + ball->size) > computer->x && ball->y > computer->y && (ball->y + ball->size) < (computer->y + computer->height))
    {
        return 1;
    }
    if (ball->x < (player->x + player->width) && ball->y > player->y && (ball->y + ball->size) < (player->y + player->height))
    {
        return 1;
    }
    return 0;
}

void main(void)
{
    // Stop the watchdog timer so it doesn't reset our chip
    WDTCTL = WDTPW + WDTHOLD;

    // These are the pins we need to drive.
    P3DIR |= SCK + MOSI + CS + CD;

    // De-select the LCD panel and set the clock high.
    P3OUT |= CS + SCK;

    // Pause so everything has time to start up properly.
    __delay_cycles(5500);

    // Initialize the LCD panel.
    init_lcd();

    //    ADC
    ADC12CTL0 = ADC12SHT02 + ADC12ON; // Sampling time, ADC12 on
    ADC12CTL1 = ADC12SHP;             // sampling timer
    ADC12CTL0 |= ADC12ENC;            // ADC enable
    P6SEL |= 0x01;                    // P6.0 allow ADC on pin 6.0
    ADC12MCTL0 = ADC12INCH_0;         // selects which input results are
                                      // stored in memory ADC12MEM0. Input
                                      // one is selected on reset so this line is not needed

    P1DIR |= BIT2; // Output on Pin 1.2
    P1SEL |= BIT2; // Pin 1.2 selected as PWM

    // microseconds
    TA0CCTL1 = OUTMOD_7; // TA0CCR1 reset/set-high voltage
    // below count, low voltage when past

    TA0CTL = TASSEL_2 + MC_1 + TAIE + ID_0;
    // Timer A control set to SMCLK, 1MHz
    // and count up mode MC_1

    // Notes of Baba Black Sheep
    int notes[] = {261, 261, 392};
    int score[] = {261, 392, 250};

    write_ones();
    write_zeros();
    write_ones();
    write_zeros();
    int i = 0;

    struct ball
    {
        int x;
        int y;
        int size;
        int x_vel;
        int y_vel;
    };

    struct paddle
    {
        int x;
        int y;
        int height;
        int width;
        int y_vel;
        int score;
    };
    struct paddle player;
    player.x = 10;
    player.y = 49;
    player.width = 7;
    player.height = 15;
    player.score = 0;
    player.y_vel = 0;

    struct paddle computer;
    computer.x = 85;
    computer.y = 49;
    computer.width = 7;
    computer.height = 15;
    computer.score = 0;

    struct ball ball;
    ball.x = 47;
    ball.y = 28;
    ball.size = 4;
    ball.x_vel = 1;
    ball.y_vel = 1;

    // test();
    struct paddle old_player;
    struct paddle old_computer;
    struct ball old_ball;
    int count = 0;

    while (1)
    {
        count++;
        if (count % 100 == 0 && ball.x_vel < 10 && ball.x_vel > -10)
        {
            if (ball.x_vel > 0)
            {
                ball.x_vel++;
            }
            else
            {
                ball.x_vel--;
            }
        }

        ADC12CTL0 |= ADC12SC; // Start sampling
        while (ADC12CTL1 & ADC12BUSY)
            ; // while bit ADC12BUSY in register ADC12CTL1 is high wait
        char c = ADC12MEM0 >> 6;

        clear_rectangle(player.x, player.y, player.width, player.height);
        clear_rectangle(computer.x, computer.y, computer.width, computer.height);
        clear_rectangle(ball.x, ball.y, ball.size, ball.size);
        if (c <= 0)
        {
            player.y = 0;
        }
        else if (c >= 49)
        {
            player.y = 49;
        }
        else
        {
            player.y = c;
        }
        if (ball.y + ball.y_vel - computer.height / 2 < 0)
        {
            computer.y = 0;
        }
        else if (ball.y + ball.y_vel - computer.height / 2 > 49)
        {
            computer.y = 49;
        }
        else
        {
            computer.y = ball.y + ball.y_vel - computer.height / 2;
        }
        // implement ball movement with bounce
        ball.x += ball.x_vel;
        ball.y += ball.y_vel;
        if (ball.y <= 0 || ball.y >= 60)
        {
            ball.y_vel = -ball.y_vel;
        }
        if (collides(&ball, &player, &computer))
        {
            for (i = 0; i < 6; i++)
            {
                int period = 1000000 / notes[i];
                TA0CCR0 = period;
                TA0CCR1 = period / 2;
                __delay_cycles(50000);
            }
            TA0CCR0 = 0;
            TA0CCR1 = 0;
            if (ball.x_vel < 0)
            {
                ball.x = player.x + player.width + 1;
            }
            else
            {
                ball.x = computer.x - ball.size - 1;
            }
            ball.x_vel = -ball.x_vel;
        }
        if (ball.x <= 0)
        {
            
            ball.x = 47;
            ball.y = 28;
            ball.x_vel = 1;
            ball.y_vel = 1;
            computer.score++;
            for (i = 0; i < 6; i++)
            {
                int period = 1000000 / score[i];
                TA0CCR0 = period;
                TA0CCR1 = period / 2;
                __delay_cycles(50000);
            }
            TA0CCR0 = 0;
            TA0CCR1 = 0;
        }
        if (ball.x >= 100)
        {
            
            ball.x = 47;
            ball.y = 28;
            ball.x_vel = 1;
            ball.y_vel = 1;
            player.score++;
            for (i = 0; i < 6; i++)
            {
                int period = 1000000 / score[i];
                TA0CCR0 = period;
                TA0CCR1 = period / 2;
                __delay_cycles(50000);
            }
            TA0CCR0 = 0;
            TA0CCR1 = 0;
        }

        draw_rectangle(player.x, player.y, player.width, player.height);
        draw_rectangle(computer.x, computer.y, computer.width, computer.height);
        draw_rectangle(ball.x, ball.y, ball.size, ball.size);
    }

    for (;;)
    {
        __bis_SR_register(LPM3_bits + GIE);
    }
}
