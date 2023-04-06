#include <msp430.h>

#define CS BIT3   // Chip Select line
#define CD BIT1   // Command/Data mode line
#define MOSI BIT0 // Master-out Slave-in
#define SCK BIT2  // Serial clock

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

#include <stdlib.h>

void draw_rectangle(int x, int y, int width, int height)
{
    int page_start, page_end, i, j, page;

    // Calculate the start and end pages for the rectangle
    page_start = y / 8;
    page_end = (y + height) / 8;

    for (page = page_start; page <= page_end; page++)
    {
        unsigned char cmd_data[3] = {0};
        P3OUT &= ~CD;                           // set for commands
        cmd_data[0] = 0xB0 + page;              // set page
        cmd_data[1] = 0x00 + (x & 0x0F);        // LSB of column address
        cmd_data[2] = 0x10 + ((x >> 4) & 0x0F); // MSB of column address
        spi_IO(cmd_data, 3);

        // Create a dynamic data array based on width
        unsigned char *data = (unsigned char *)malloc(width * sizeof(unsigned char));

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

        // Free the memory allocated for the data array
        free(data);
    }
}

void clear_rectangle(int x, int y, int width, int height)
{
    int page_start, page_end, i, j, page;

    // Calculate the start and end pages for the rectangle
    page_start = y / 8;
    page_end = (y + height) / 8;

    for (page = page_start; page <= page_end; page++)
    {
        unsigned char cmd_data[3] = {0};
        P3OUT &= ~CD;                           // set for commands
        cmd_data[0] = 0xB0 + page;              // set page
        cmd_data[1] = 0x00 + (x & 0x0F);        // LSB of column address
        cmd_data[2] = 0x10 + ((x >> 4) & 0x0F); // MSB of column address
        spi_IO(cmd_data, 3);

        // Create a dynamic data array based on width
        unsigned char *data = (unsigned char *)malloc(width * sizeof(unsigned char));

        P3OUT |= CD; // set for data
        for (i = 0; i < width; i++)
        {
            // Calculate the byte to draw based on the current page and y coordinate
            data[i] = 0x00;
        }

        spi_IO(data, width);

        // Free the memory allocated for the data array
        free(data);
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
void set_ball_speed(struct ball *ball, int count)
{
    if (count % 100 == 0 && ball->x_vel < 10 && ball->x_vel > -10)
    {
        count = 0;
        if (ball->x_vel > 0)
        {
            ball->x_vel++;
        }
        else
        {
            ball->x_vel--;
        }
    }
}

void init_SPI()
{
    // These are the pins we need to drive.
    P3DIR |= SCK + MOSI + CS + CD;
    // De-select the LCD panel and set the clock high.
    P3OUT |= CS + SCK;
}
void init_ADC()
{
    ADC12CTL0 = ADC12SHT02 + ADC12ON; // Sampling time, ADC12 on
    ADC12CTL1 = ADC12SHP;             // sampling timer
    ADC12CTL0 |= ADC12ENC;            // ADC enable
    P6SEL |= 0x01;                    // P6.0 allow ADC on pin 6.0
    ADC12MCTL0 = ADC12INCH_0;         // selects which input results are
                                      // stored in memory ADC12MEM0. Input
                                      // one is selected on reset so this line is not needed
}

void init_MPD()
{
    P4DIR |= (BIT0 + BIT1 + BIT2 + BIT3);
    P8DIR |= (BIT1 + BIT2);
    P7DIR |= BIT0;
    int i;
    for (i = 0; i < 4; i++)
    {
        // GET NUMBER TO SET FROM ARR
        P4OUT = 0;

        // STROBE
        P7OUT = 0b00000000;
        P7OUT = 0b00000001;

        // SELECT NEXT DISPLAY FOR NEXT LOOP
        P8OUT -= (1 << 1);
    }
}

void start_animation()
{
    play_music(2);
    write_ones();
    write_zeros();
    write_ones();
    write_zeros();
}

void move_player(struct paddle *player, int y)
{
    if (y <= 0)
    {
        player->y = 0;
    }
    else if (y >= 49)
    {
        player->y = 48;
    }
    else
    {
        player->y = y;
    }
}

void move_computer_insane(struct paddle *computer, int y, int ball_vel)
{
    if (y + ball_vel - computer->height / 2 < 0)
    {
        computer->y = 0;
    }
    else if (y + ball_vel - computer->height / 2 > 49)
    {
        computer->y = 49;
    }
    else
    {
        computer->y = y + ball_vel - computer->height / 2;
    }
}
void move_computer_basic(struct paddle *computer, int y)
{
    if (y - computer->height / 2 < 0)
    {
        computer->y = 0;
    }
    else if (y - computer->height / 2 > 48)
    {
        computer->y = 49;
    }
    else
    {
        computer->y = y;
    }
}

void move_computer_adaptive(struct paddle *computer, struct paddle *player, struct ball *ball)
{
    int chance_of_mistake = 10; // 10% chance of making a mistake
    int mistake_margin = 5;     // How much the AI paddle will miss by
    int reaction_delay = 3;     // Delay in reacting to the ball's movement

    static int delay_counter = 0;
    static int target_y = -1;

    if (ball->x_vel > 0)
    {
        // React to the ball's movement after a certain delay
        if (delay_counter < reaction_delay)
        {
            delay_counter++;
            return;
        }

        delay_counter = 0;

        // Occasionally make a mistake
        if (rand() % 100 < chance_of_mistake)
        {
            target_y = ball->y + ((rand() % 2) ? mistake_margin : -mistake_margin);
        }
        else
        {
            // Predict the bouncing behavior of the ball
            int temp_y = ball->y;
            int temp_y_vel = ball->y_vel;
            int prediction_limit = 100;
            int prediction_count = 0;
            while (ball->x + ball->x_vel * (temp_y / abs(temp_y_vel)) < computer->x && prediction_count < prediction_limit)
            {
                temp_y += temp_y_vel;
                if (temp_y <= 0 || temp_y >= 60)
                {
                    temp_y_vel = -temp_y_vel;
                }
            }
            target_y = temp_y - computer->height / 2;
        }
    }

    // Keep the paddle within the screen boundaries
    if (target_y < 0)
    {
        target_y = 0;
    }
    else if (target_y > 48)
    {
        target_y = 48;
    }

    // Move the paddle smoothly towards the target position
    int speed_limit = player->height / 2;
    int y_diff = target_y - computer->y;

    if (y_diff > speed_limit)
    {
        computer->y += speed_limit;
    }
    else if (y_diff < -speed_limit)
    {
        computer->y -= speed_limit;
    }
    else
    {
        computer->y = target_y;
    }
}

void play_music(int sel)
{
    P1OUT &= ~(BIT2 + BIT3);
    sel = sel << 2;
    P1OUT |= sel;
    P1OUT |= BIT4;
    __delay_cycles(5000); // DELAY FOR 5000 microseconds = 5 milliseconds
    P1OUT &= ~BIT4;
}
void move_ball(struct ball *ball)
{
    if (ball->y <= 0 || ball->y >= 60)
    {
        ball->y_vel = -ball->y_vel;
    }

    ball->x += ball->x_vel;
    ball->y += ball->y_vel;
}

char get_adc_position()
{
    ADC12CTL0 |= ADC12SC; // Start sampling
    while (ADC12CTL1 & ADC12BUSY)
        ; // while bit ADC12BUSY in register ADC12CTL1 is high wait
    char adc_position = ADC12MEM0 >> 6;
    return adc_position;
}

void set_up_game(struct paddle *player, struct paddle *computer, struct ball *ball)
{

    // Generate a random number between 0 and 1
    int random_num = rand() % 2;
    random_num = (random_num == 0) ? 1 : -1;
    player->x = 10;
    player->y = 49;
    player->width = 7;
    player->height = 15;
    player->score = 0;
    player->y_vel = 0;

    computer->x = 85;
    computer->y = 49;
    computer->width = 7;
    computer->height = 15;
    computer->score = 0;

    ball->x = 47;
    ball->y = 28;
    ball->size = 4;
    ball->x_vel = 1;
    ball->y_vel = random_num;
}

// void check_collision(struct ball *ball, struct paddle *player, struct paddle *computer)
// {
//     int hit_margin = 1;
//     float ball_speed = sqrt(ball->x_vel * ball->x_vel + ball->y_vel * ball->y_vel);

//     // Check for collision with player paddle
//     if (ball->x <= player->x + player->width + hit_margin && ball->x >= player->x + player->width &&
//         ball->y + ball->size >= player->y && ball->y <= player->y + player->height)
//     {
//         play_music(1);
//         ball->x_vel = -ball->x_vel;
//         float relative_y = (float)(ball->y + ball->size / 2 - player->y) / player->height;
//         float angle = relative_y * MAX_BOUNCE_ANGLE - MAX_BOUNCE_ANGLE / 2;
//         ball->y_vel = (int)(ball_speed * sin(angle));
//     }

//     // Check for collision with computer paddle
//     if (ball->x + ball->size >= computer->x - hit_margin && ball->x + ball->size <= computer->x &&
//         ball->y + ball->size >= computer->y && ball->y <= computer->y + computer->height)
//     {
//         play_music(1);
//         ball->x_vel = -ball->x_vel;
//         float relative_y = (float)(ball->y + ball->size / 2 - computer->y) / computer->height;
//         float angle = relative_y * MAX_BOUNCE_ANGLE - MAX_BOUNCE_ANGLE / 2;
//         ball->y_vel = (int)(ball_speed * sin(angle));
//     }
// }

void check_collision(struct ball *ball, struct paddle *player, struct paddle *computer)
{

    if (collides(ball, player, computer))
    {
        play_music(1);
        if (ball->x_vel < 0)
        {
            ball->x = player->x + player->width + 1;
        }
        else
        {
            ball->x = computer->x - ball->size - 1;
        }
        ball->x_vel = -ball->x_vel;
    }
}

void update_score(struct paddle *player, struct paddle *computer, struct ball *ball)
{
    // Generate a random number between 0 and 1
    int random_num_x = rand() % 2;
    random_num_x = (random_num_x == 0) ? 1 : -1;
    int random_num_y = rand() % 2;
    random_num_y = (random_num_y == 0) ? 1 : -1;

    if (ball->x <= 10)
    {
        computer->score++;
        play_music(0);
        ball->x = 47;
        ball->y = 28;
        ball->size = 4;
        ball->x_vel = random_num_x;
        ball->y_vel = random_num_y;

        P4OUT = computer->score / 10;
        P8OUT = 2;
        P7OUT = 0b00000000;
        P7OUT = 0b00000001;
        P4OUT = computer->score % 10;
        P8OUT = 0;
        P7OUT = 0b00000000;
        P7OUT = 0b00000001;
    }
    else if (ball->x >= 87)
    {
        player->score++;
        play_music(0);
        ball->x = 47;
        ball->y = 28;
        ball->size = 4;
        ball->x_vel = random_num_x;
        ball->y_vel = random_num_y;
        P4OUT = player->score / 10;
        P8OUT = 6;
        P7OUT = 0b00000000;
        P7OUT = 0b00000001;
        P4OUT = player->score % 10;
        P8OUT = 4;
        P7OUT = 0b00000000;
        P7OUT = 0b00000001;
    }
}
void main(void)
{
    // Stop the watchdog timer so it doesn't reset our chip
    WDTCTL = WDTPW + WDTHOLD;
    P1DIR |= (BIT2 + BIT3 + BIT4);
    srand(time(0));
    init_MPD();
    init_SPI();
    __delay_cycles(5500); // Pause so everything has time to start up properly.
    init_lcd();
    init_ADC();
    start_animation();

    struct paddle player;
    struct paddle computer;
    struct ball ball;
    set_up_game(&player, &computer, &ball);

    int count = 0;

    while (1)
    {
        count++;
        set_ball_speed(&ball, count);
        char adc_position = get_adc_position();
        clear_rectangle(player.x, player.y, player.width, player.height);
        clear_rectangle(computer.x, computer.y, computer.width, computer.height);
        clear_rectangle(ball.x, ball.y, ball.size, ball.size);
        move_player(&player, adc_position);
        // move_computer_basic(&computer, ball.y);
        move_computer_insane(&computer, ball.y, ball.y_vel);
        // move_computer_adaptive(&computer, &player, ball.y, ball.y_vel, ball.x_vel);
        // move_computer_adaptive(&computer, &player, &ball);
        move_ball(&ball);
        check_collision(&ball, &player, &computer);
        update_score(&player, &computer, &ball);
        draw_rectangle(player.x, player.y, player.width, player.height);
        draw_rectangle(computer.x, computer.y, computer.width, computer.height);
        draw_rectangle(ball.x, ball.y, ball.size, ball.size);
    }
}
