#include <msp430.h>
#include <stdlib.h>
#include <time.h>
#include <font_8x8.h>

#define CS BIT3   // Chip Select line
#define CD BIT1   // Command/Data mode line
#define MOSI BIT0 // Master-out Slave-in
#define SCK BIT2  // Serial clock

// TODO: ADD GAME CONSTANTS HERE
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

/* Draws a ball on the screen.  The ball is defined by
 * the upper left corner (x, y) */
void draw_ball(int x, int y)
{
    int ball_size = 4;
    int page_start, page_end, i, page;
    unsigned char data[4] = {0};

    // Calculate the start and end pages for the rectangle
    page_start = y / 8;
    page_end = (y + 4) / 8;

    for (page = page_start; page <= page_end; page++)
    {

        P3OUT &= ~CD;                               // set for commands
        data[0] = 0xB0 + page;                      // set page
        data[1] = 0x00 + (x & 0x0F);                // LSB of column address
        data[2] = 0x10 + ((x >> ball_size) & 0x0F); // MSB of column address
        spi_IO(data, 3);

        P3OUT |= CD; // set for data
        for (i = 0; i < ball_size; i++)
        {
            // Calculate the byte to draw based on the current page and y coordinate
            if (page == page_start)
            {
                data[i] |= 0x0F << (y % 8);
            }
            else if (page == page_end)
            {
                data[i] = 0xF0 >> (8 - (y + ball_size) % 8);
            }
        }

        spi_IO(data, ball_size);
    }
}

/* Clears a ball on the screen.  The ball is defined by
 * the upper left corner (x, y) */
void clear_ball(int x, int y)
{
    int ball_size = 4;
    int page_start, page_end, i, page;
    unsigned char data[4];

    // Calculate the start and end pages for the rectangle
    page_start = y / 8;
    page_end = (y + ball_size) / 8;

    for (page = page_start; page <= page_end; page++)
    {
        P3OUT &= ~CD;                       // set for commands
        data[0] = 0xB0 + page;              // set page
        data[1] = 0x00 + (x & 0x0F);        // LSB of column address
        data[2] = 0x10 + ((x >> 4) & 0x0F); // MSB of column address
        spi_IO(data, 3);

        P3OUT |= CD; // set for data
        for (i = 0; i < ball_size; i++)
        {
            // Calculate the byte to draw based on the current page and y coordinate
            data[i] = 0x00;
        }

        spi_IO(data, ball_size);
    }
}

/* Draws a rectangle on the screen.  The rectangle is defined by
 * the upper left corner (x, y) and the width and height */
void draw_rectangle(int x, int y, int width, int height)
{
    int page_start, page_end, i, page;

    // Calculate the start and end pages for the rectangle
    page_start = y / 8;
    page_end = (y + height) / 8;

    for (page = page_start; page <= page_end; page++)
    {
        unsigned char cmd_data[3] = {0};
        P3OUT &= ~CD;                           // set for commands
        cmd_data[0] = 0xB0 + page;              // set page
        cmd_data[1] = 0x00 + (x & 0x0F);        // LSB of column address
        cmd_data[2] = 0x10 + ((x & 0xF0) >> 4); // MSB of column address
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

/* Clears a rectangle on the screen.  The rectangle is defined by
 * the upper left corner (x, y) and the width and height */
void clear_rectangle(int x, int y, int width, int height)
{
    int page_start, page_end, i, page;

    // Calculate the start and end pages for the rectangle
    page_start = y / 8;
    page_end = (y + height) / 8;

    for (page = page_start; page <= page_end; page++)
    {
        unsigned char cmd_data[3] = {0};
        P3OUT &= ~CD;                           // set for commands
        cmd_data[0] = 0xB0 + page;              // set page
        cmd_data[1] = 0x00 + (x & 0x0F);        // LSB of column address
        cmd_data[2] = 0x10 + ((x & 0xF0) >> 4); // MSB of column address
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

/* Sends music select signal to other MSP430
 * sel = 0 | 1 | 2 | 3
 * 0 = score sound
 * 1 = paddle hit sound
 * 2 = intro music
 * 3 = game over music
 */
void play_music(int sel)
{
    P1OUT &= ~(BIT2 + BIT3);
    sel = sel << 2;
    P1OUT |= sel;
    P1OUT |= BIT4;
    __delay_cycles(5000); // DELAY FOR 5000 microseconds = 5 milliseconds
    P1OUT &= ~BIT4;
}

/* Moves the ball based on its velocity defined
 * by x_vel and y_vel inside the ball struct
 */
void move_ball(struct ball *ball)
{
    if (ball->y <= 0 || ball->y >= 59)
    {
        ball->y_vel = -ball->y_vel;
    }

    ball->x += ball->x_vel;
    ball->y += ball->y_vel;
}

/* Checks if the ball has collided with a paddle
 * Returns 1 if collision detected, 0 otherwise
 * Collision detetion has a BUFFER of 2 pixels
 * to allow for a little bit of leeway
 */
int collides(struct ball *ball, struct paddle *player, struct paddle *computer)
{

    // Computer paddle
    if (ball->x + ball->size > computer->x && ball->x <= computer->x + computer->width && ball->y >= computer->y && (ball->y) <= computer->y + computer->height)
    {
        return 1;
    }
    // Player paddle
    if (ball->x < player->x + player->width && ball->x >= player->x && ball->y >= player->y - ball->size && (ball->y) <= player->y + player->height)
    {
        return 1;
    }
    return 0;
}

/* Inceases the speed of the ball by 1 pixel
 * every 100 frames
 */
// TODO: Increase y_vel as well
void set_ball_speed(struct ball *ball, int count)
{
    if (count % 100 == 0 && ball->x_vel < 5 && ball->x_vel > -5)
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

/* Initializes PINS for SPI communication
 * SCK = P3.2
 * MOSI = P3.0
 * CS = P3.3
 * CD = P3.1
 */

void init_SPI()
{
    // These are the pins we need to drive.
    P3DIR |= SCK + MOSI + CS + CD;
    // De-select the LCD panel and set the clock high.
    P3OUT |= CS + SCK;
}

/* Initializes ADC on pin 6.0
 */
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

/* Initialize PINS for the 7-Segment Display and set to 0
 * P4.0(LSD) - P4.3(MSD) = Data
 * P8.1(LSD) - P8.2(MSD) = Latch
 * P7.0 = Strobe
 */
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

/* Plays game start music and draw through every page and clears them
 */
void start_animation()
{
    play_music(2);
    write_ones();
    write_zeros();
    write_ones();
    write_zeros();
}

/* Moves player to the y coordinate
 * If y is out of bounds, it will be set to the closest
 * possible value
 */
void move_player(struct paddle *player, int y)
{
    if (y <= 0)
    {
        player->y = 0;
    }
    else if (y > 47)
    {
        player->y = 47;
    }
    else
    {
        player->y = y;
    }
}

/* Moves center of the computer paddle to the y coordinate (+- 3 pixels)
 * of where the ball will be. If y is out of bounds,
 * it will be set to the closest possible value
 */
void move_ai_middle_hitter(struct paddle *computer, struct ball *ball)
{
    const int random_movement = rand() % (computer->height / 2 + 3);
    int loc = ball->y + 2 - computer->height / 2 + random_movement;

    if (ball->x_vel > 0)
    {
        if (loc < 0)
        {
            loc = 0 + random_movement;
        }
        else if (loc > 47)
        {
            loc = 47 - random_movement;
        }
        if ((computer->y - loc) > computer->height / 2)
        {
            loc = computer->y - computer->height / 2;
        }
        else if ((loc - computer->y) > computer->height / 2)
        {
            loc = computer->y + computer->height / 2;
        }
        computer->y = loc;
    }
}
/* Moves computer paddle to the y coordinate (+- 3 pixels)
 * If y is out of bounds, it will be set to the closest
 * possible value
 */
void move_ai_top_hitter(struct paddle *computer, int y)
{
    // const int random_movement = 3;
    // int loc = computer->y = y + rand() % 5 - 2;
    // //    if (ball->x_vel > 0)
    // {
    //     //        if (ball->x < 50)
    //     {
    //         //            loc = ball->y ;
    //     }
    //     if (loc < 0)
    //     {
    //         computer->y = 0 + rand() % 3;
    //     }
    //     else if (loc > 47)
    //     {
    //         computer->y = 47 - rand() % 3;
    //     }
    //     else
    //     {
    //         computer->y = loc;
    //     }
    // }
    if (y < 0)
    {
        computer->y = 0 + rand() % 3;
    }
    else if (y > 48)
    {
        computer->y = 48 - rand() % 3;
    }
    else
    {
        computer->y = y + rand() % 5 - 2;
    }
}

/* Moves the computer paddle towards the estimated location
 * of the ball, with some variability and occasional
 * mistakes to simulate human-like behavior.
 */
void move_ai_predictive(struct paddle *computer, struct paddle *player, struct ball *ball)
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
            // Move the paddle towards the ball's y-coordinate
            // add a non-zero random number to make the AI more human-like
            int rand_num = rand() % 7 - 3;
            target_y = ball->y - computer->height / 2 + rand_num;
            // target_y = ball->y - computer->height / 2 + rand() % 7 - 3;// adds a random
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

/* Get reading from ADC and return a number between 0 and 63
 */
char get_adc_position()
{
    ADC12CTL0 |= ADC12SC; // Start sampling
    while (ADC12CTL1 & ADC12BUSY)
        ; // while bit ADC12BUSY in register ADC12CTL1 is high wait
    char adc_position = ADC12MEM0 >> 6;
    return adc_position;
}

/* sets up the game by setting the initial values of the
 * player, computer, and ball
 */
void set_up_game(struct paddle *player, struct paddle *computer, struct ball *ball)
{
    // TODO: Make all of these constants
    // Generate a random number between 0 and 1
    int random_num = rand() % 2;
    random_num = (random_num == 0) ? 1 : -1;
    player->x = 10;
    player->y = 49;
    player->width = 7;
    player->height = 16;
    player->score = 0;
    player->y_vel = 0;

    computer->x = 85;
    computer->y = 49;
    computer->width = 7;
    computer->height = 16;
    computer->score = 0;

    ball->x = 47;
    ball->y = 28;
    ball->size = 4;
    ball->x_vel = 1;
    ball->y_vel = random_num;
}

/* Checks if the ball collides with either paddle
 * If it does, the ball's x velocity is reversed
 * and the ball's y velocity is set to the difference
 * between the ball's y coordinate and the center of the paddle
 */
void check_collision(struct ball *ball, struct paddle *player, struct paddle *computer)
{
    struct paddle *paddle;

    if (collides(ball, player, computer))
    {
        play_music(1);
        if (ball->x_vel < 0)
        {
            ball->x = player->x + player->width + 1;
            paddle = player;
        }
        else
        {
            ball->x = computer->x - ball->size - 1;
            paddle = computer;
        }
        ball->x_vel = -ball->x_vel;
        ball->y_vel = ((ball->y) + 2 - (paddle->y + paddle->height / 2)) / 2;
    }
}

/*  Check if ball is out of bounds
 * If it is, the score is updated and the ball is reset
 */
void update_score(struct paddle *player, struct paddle *computer, struct ball *ball)
{

    // Generate a random number between 0 and 1
    int random_num_x = rand() % 2;
    random_num_x = (random_num_x == 0) ? 1 : -1;
    int random_num_y = rand() % 2;
    random_num_y = (random_num_y == 0) ? 1 : -1;

    if (ball->x <= -4)
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
    else if (ball->x >= 97)
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

/* Checks if the game is over
 * If it is, the game over screen is displayed
 */
void check_game_over(struct paddle *player, struct paddle *computer, struct ball *ball)
{
    // TODO: Make this a constant
    if (player->score == 5 || computer->score == 5)
    {
        play_music(3);

        char gameover[] = {'G', 'A', 'M', 'E', ' ', 'O', 'V', 'E', 'R'};
        draw_string(15, 2, font_8x8, gameover);

        if (player->score == 5)
        {
            char win[] = {'Y', 'O', 'U', ' ', 'W', 'I', 'N'};
            draw_string(20, 4, font_8x8, win);
        }
        else
        {
            char lose[] = {'Y', 'O', 'U', ' ', 'L', 'O', 'S', 'E'};
            draw_string(20, 4, font_8x8, lose);
        }
        wait_for_player_input();
        start_animation();
        set_up_game(player, computer, ball);
        init_MPD();
    }
}

/* Send a byte to the display
 * This function is used to send characters to the display
 */
void send_byte(unsigned char char_to_write)
{
    int n;
    for (n = 8; n != 0; n--)
    {
        if (char_to_write & 0x80)
            P3OUT |= MOSI;
        else
            P3OUT &= ~MOSI;
        char_to_write <<= 1;

        // Pulse clock
        P3OUT &= ~SCK;
        __delay_cycles(1);
        P3OUT |= SCK;
        __delay_cycles(1);
    }
}
/* Draws a string to the display
    * This function is used to draw a string to the display
    * It takes in the column and page to start drawing at
    * It also takes in the font to use and the string to draw

*/

// TODO: See if you can make this not show extra chars
void draw_string(unsigned char column, unsigned char page, const unsigned char *font_adress, const char *str)
{

    unsigned int pos_array;                                                // Postion of character data in memory array
    unsigned char x, y, column_cnt, width_max;                             // temporary column and page adress, couloumn_cnt tand width_max are used to stay inside display area
    unsigned char start_code, last_code, width, page_height, bytes_p_char; // font information, needed for calculation
    const char *string;

    start_code = font_adress[2];   // get first defined character
    last_code = font_adress[3];    // get last defined character
    width = font_adress[4];        // width in pixel of one char
    page_height = font_adress[6];  // page count per char
    bytes_p_char = font_adress[7]; // bytes per char

    if (page_height + page > 8) // stay inside display area
        page_height = 8 - page;

    x = column; // store column for display last column check
    // The string is displayed character after character. If the font has more then one page,
    // the top page is printed first, then the next page and so on
    for (y = 0; y < page_height; y++)
    {
        unsigned char cmd_data[3] = {0};
        P3OUT &= ~CD;                           // set for commands
        cmd_data[0] = 0xB0 + page;              // set page
        cmd_data[1] = 0x00 + (x & 0x0F);        // LSB of column address
        cmd_data[2] = 0x10 + ((x & 0xF0) >> 4); // MSB of column address
        spi_IO(cmd_data, 3);                    // set startpositon and page
        column_cnt = column;                    // store column for display last column check
        string = str;                           // temporary pointer to the beginning of the string to print

        P3OUT |= CD;
        P3OUT &= ~CS;

        while (*string != 0)
        {
            if ((unsigned char)*string < start_code || (unsigned char)*string > last_code) // make sure data is valid
                string++;
            else
            {
                // calculate positon of ascii character in font array
                // bytes for header + (ascii - startcode) * bytes per char)
                pos_array = 8 + (unsigned int)(*string++ - start_code) * bytes_p_char;
                pos_array += y * width; // get the dot pattern for the part of the char to print

                if (column_cnt + width > 102) // stay inside display area
                    width_max = 102 - column_cnt;
                else
                    width_max = width;
                for (x = 0; x < width_max; x++) // print the whole string
                {
                    send_byte(font_adress[pos_array + x]);
                }
                column_cnt += width;
            }
        }
        P3OUT |= CS;
    }
}
/* Wait for player input with a dead zone*/
void wait_for_player_input()
{
    const int dead_zone = 5;
    char old = get_adc_position();
    char new = get_adc_position();

    while (new + dead_zone >= old &&new - dead_zone <= old)
    {
        new = get_adc_position();
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
    char str[] = {'P', 'O', 'N', 'G', ' '};
    draw_string(35, 1, font_8x8, str);
    char firstto5[] = {'F', 'I', 'R', 'S', 'T', ' ', 'T', 'O', ' ', '5', ' ', ' '};
    char wins[] = {'W', 'I', 'N', 'S', ' '};
    draw_string(10, 4, font_8x8, firstto5);
    draw_string(35, 5, font_8x8, wins);

    wait_for_player_input();
    start_animation();

    while (1)
    {
        count++;
        set_ball_speed(&ball, count);
        char adc_position = get_adc_position();
        clear_rectangle(player.x, player.y, player.width, player.height);
        clear_rectangle(computer.x, computer.y, computer.width, computer.height);
        clear_ball(ball.x, ball.y);
        // move_player(&player, adc_position);
        move_ai_top_hitter(&player, ball.y); // AI (Top Hitter) controlled PLAYER
                                             //        move_ai_middle_hitter(&player, &ball); // AI (Middle Hitter) controlled PLAYER
        // move_ai_top_hitter(&computer, ball.y); // AI (Top Hitter) controlled computer
        move_ai_middle_hitter(&computer, &ball); // AI (Middle Hitter) controlled computer
        // move_ai_predictive(&computer, &player, &ball); // AI (Predictive) controlled computer
        move_ball(&ball);
        check_collision(&ball, &player, &computer);
        update_score(&player, &computer, &ball);
        check_game_over(&player, &computer, &ball);
        draw_rectangle(player.x, player.y, player.width, player.height);
        draw_rectangle(computer.x, computer.y, computer.width, computer.height);
        draw_ball(ball.x, ball.y);
    }
}
