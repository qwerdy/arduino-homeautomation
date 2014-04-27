IRsend irsend;

#define ON                0XFFE01F
#define OFF               0xFF609F
#define BRIGHTNESS_UP     0xFFA05F
#define BRIGHTNESS_DOWN   0xFF20DF
#define FLASH             0xFFF00F
#define STROBE            0xFFE817
#define FADE              0xFFD827
#define SMOOTH            0xFFC837
 
#define RED               0xFF906F
#define GREEN             0XFF10EF
#define BLUE              0xFF50AF
#define WHITE             0xFFD02F
 
#define ORANGE            0xFFB04F
#define YELLOW_DARK       0xFFA857
#define YELLOW_MEDIUM     0xFF9867
#define YELLOW_LIGHT      0xFF8877
 
#define GREEN_LIGHT       0XFF30CF
#define GREEN_BLUE1       0XFF28D7
#define GREEN_BLUE2       0XFF18E7
#define GREEN_BLUE3       0XFF08F7
 
#define BLUE_RED          0XFF708F
#define PURPLE_DARK       0XFF6897
#define PURPLE_LIGHT      0XFF58A7
#define PINK              0XFF48B7

void ir_send(char code) {
  unsigned long send_code = 0;

  switch (code) {
    case 'O':
      send_code = ON;
      break;
    case 'F':
      send_code = OFF;
      break;
    case 'A':
      send_code = ON;
      light_auto_on = 2;
      break;
    case 'u':
      send_code = BRIGHTNESS_UP;
      break;
    case 'd':
      send_code = BRIGHTNESS_DOWN;
      break;
    case 'r':
      send_code = RED;
      break;
    case 'g':
      send_code = GREEN;
      break;
    case 'b':
      send_code = BLUE;
      break;
    case 'o':
      send_code = ORANGE;
      break;
    case 'p':
      send_code = PINK;
      break;
    case 'y':
      send_code = YELLOW_LIGHT;
      break;
    case 'B':
      send_code = GREEN_BLUE3;
      break;
    case 'Y':
      send_code = YELLOW_MEDIUM;
      break;
    case 'w':
      send_code = WHITE;
      break;
    case '1':
      send_code = FLASH;
      break;
    case '2':
      send_code = STROBE;
      break;
    case '3':
      send_code = FADE;
      break;
    case '4':
      send_code = SMOOTH;
      break;
    }
 
 if(send_code) {
    irsend.sendNEC(send_code, 32);
    irsend.sendNEC(send_code, 32);
 }
}
