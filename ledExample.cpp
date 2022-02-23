#include <stdio.h>
#include <unistd.h>
//#include <pigpio.h>

#include <ws2811/ws2811.h>

#include <stdlib.h>

int width = 100;

#define TARGET_FREQ WS2811_TARGET_FREQ
#define DMA 10

ws2811_t ledstrip =
    {
        .freq = TARGET_FREQ,
        .dmanum = DMA,
        .channel =
            {
                [0] =
                    {
                        .gpionum = 12,
                        .invert = 0,
                        .count = 100,
                        .strip_type = WS2811_STRIP_GBR,
                        .brightness = 255,
                    },
                [1] =
                    {
                        .gpionum = 0,
                        .invert = 0,
                        .count = 0,
                        .brightness = 0,
                    },
            },
};

//blue green red
ws2811_led_t dotcolors[] =
    {
        0x00110000, // blue (higher numbers == brighter colours)
        0x00001100, // green
        0x00000011, // red
        0x00002000, // 
        0x00002020, // 
        0x00000020, // 
        0x00100010, // 
        0x00200010, // 
};
void clearLEDS()
{
    // clear LEDs
    for (int i = 0; i < 96; i++)
    {
        ledstrip.channel[0].leds[i] = 0;
    }
}

int main(int argc, char **argv)
{
    ws2811_return_t ret;
    if ((ret = ws2811_init(&ledstrip)) != WS2811_SUCCESS)
    {
        fprintf(stderr, "ws2811_init failed: %s\n", ws2811_get_return_t_str(ret));
        return ret;
    }
    // for (int loop = 0; loop < 15; loop++)
    // {

    //     for (int i = 0; i < 96; i++)
    //     {

    //         ledstrip.channel[0].leds[i] = dotcolors[loop%8];
    //         ws2811_render(&ledstrip);
    //         usleep(10000);
    //         ledstrip.channel[0].leds[i] = 0;
    //         ws2811_render(&ledstrip);
    //     }
    // }
    char *p;
    int col = strtol(argv[1], &p, 16);
    printf("%s -> %d\n", argv[1], col);
    for (int i = 0; i < 96; i++)
    {
        ledstrip.channel[0].leds[i] = col;
    }
    ws2811_render(&ledstrip);
    // clearLEDS();
    // ws2811_render(&ledstrip);
    return 0;
}
