
#ifndef i2cLEDScreen
#define i2cLEDScreen
#ifndef string
#include <string>
#endif
#define LINE1 0x80 // 1st line
#define LINE2 0xC0 // 2nd line
#define LINE3 0x94 // 2nd line
#define LINE4 0xD4 // 2nd line

void lcd_init(void);
void lcd_byte(int bits, int mode);
void lcd_toggle_enable(int bits);

// added by Lewis
void typeInt(int i);
void typeFloat(float myFloat);
void lcdLoc(int line); // move cursor
void ClrLcd(void);     // clr LCD return home
void typeln(const char *s);
void typeChar(char val);
void init_i2c_screen();
void printToScreen(std::string input, int line);
#endif