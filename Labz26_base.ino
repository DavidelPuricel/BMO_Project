/***************************************************************************************************
 * INCLUDES
 ***************************************************************************************************/
#include "SPI.h"
#include "Adafruit_ST7735.h"
#include "pca9557_cdd.h"
#include "StringUtils.h"
#include "LcdUtils.h"
#include "SD.h"
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

/***************************************************************************************************
 * DEFINES — Timing
 ***************************************************************************************************/
#define CYCLE_TIME_1S 1000 
#define CYCLE_TIME_10MS 10 
#define CYCLE_TIME_5MS 5   
#define CYCLE_TIME_MS 100  
#define CYCLE_TIME_1MS 1   

/***************************************************************************************************
 * DEFINES — Push Buttons
 ***************************************************************************************************/
#define SW1_PIN 3 
#define SW2_PIN 2 
#define SW3_PIN 4 
#define SW4_PIN 9 

#define DEBOUNCE_MS 500 

/***************************************************************************************************
 * DEFINES — Joystick
 ***************************************************************************************************/
#define JOY_VRX_PIN A0 
#define JOY_VRY_PIN A1 
#define JOY_BUTTON A2  

/***************************************************************************************************
 * DEFINES — LCD Pins
 ***************************************************************************************************/
#if defined(D5)
#define LCD_RST_PIN D5
#else
#define LCD_RST_PIN 5
#endif

#if defined(D6)
#define LCD_CS_PIN D6
#else
#define LCD_CS_PIN 6
#endif

#if defined(D7)
#define LCD_DC_PIN D7
#else
#define LCD_DC_PIN 7
#endif

#if defined(D10)
#define LCD_BCKL_PIN D10
#else
#define LCD_BCKL_PIN 10
#endif

/***************************************************************************************************
 * DEFINES — SD Card
 ***************************************************************************************************/
#if defined(D8)
#define SD_CS_PIN D8
#else
#define SD_CS_PIN 8
#endif

#define SD_FILES_MAX_NUM 10      
#define SD_FILES_NAME_MAX_LEN 15 

/***************************************************************************************************
 * DEFINES — PCA9557 I2C GPIO Expander
 ***************************************************************************************************/
#define PCA_ADDRESS 25 

/***************************************************************************************************
 * DEFINES — Menu System
 ***************************************************************************************************/
#define MENU_INIT 0U    
#define MENU_DEFAULT 1U 
#define MENU_IMAG 2U    
#define MENU_JOY_POS 3U 
#define MENU_DICE 4U    
#define MENU_REAC 5U    
#define MENU_MAX 6U     

/***************************************************************************************************
 * DEFINES — Reaction Game States
 ***************************************************************************************************/
#define REAC_IDLE 0U    
#define REAC_WAITING 1U 
#define REAC_GO 2U      
#define REAC_RESULT 3U  

/***************************************************************************************************
 * DEFINES — Display Geometry
 ***************************************************************************************************/
#define SCREEN_W 128 
#define SCREEN_H 96  
#define DOT_RADIUS 3 

#define ST77XX_GRAY 0x7BEF 

/***************************************************************************************************
 * DEFINES — Bluetooth Connection
 ***************************************************************************************************/
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

/***************************************************************************************************
 * GLOBAL VARIABLES
 ***************************************************************************************************/

unsigned int CurrentMenu = MENU_INIT; 
unsigned long time0 = 0; 
unsigned long time1 = 0; 
int randNumber = 0; 

uint8_t reacState = REAC_IDLE;   
unsigned long reacFlashTime = 0; 
unsigned long reacDelay = 0;     
unsigned long reacStartWait = 0; 
unsigned long reacP1Time = 0;    
unsigned long reacP2Time = 0;    
bool reacP1Done = false;         
bool reacP2Done = false;         

int joyDotX = SCREEN_W / 2;     
int joyDotY = SCREEN_H / 2;     
int joyPrevX = -1;              
int joyPrevY = -1;              
bool joyCrosshairDrawn = false; 
int joyCenterX = 2048;          
int joyCenterY = 2048;          

int PhotoIndex = 1;    
File SD_RootDir;       
int SDCardFilesNr = 0; 

char SD_files_list[SD_FILES_MAX_NUM][SD_FILES_NAME_MAX_LEN]; 
static unsigned int currentImageCount = 1;                   

bool BoardInitialized = false; 
bool LcdBacklightOn = false;   
bool SchedulerStarted = false; 

volatile bool B1Pressed = false; 
volatile bool B2Pressed = false; 
volatile bool B3Pressed = false; 
volatile bool B4Pressed = false; 

volatile unsigned long lastISR_SW1 = 0;
volatile unsigned long lastISR_SW2 = 0;
volatile unsigned long lastISR_SW3 = 0;
volatile unsigned long lastISR_SW4 = 0;

Adafruit_ST7735 lcd = Adafruit_ST7735(LCD_CS_PIN, LCD_DC_PIN, -1);

int indexMenu = 0;
int status1=0;
int status2=0;
int status3=0;
int status4=0;

bool last1 = LOW;
bool last2 = LOW;
bool last3 = LOW;
bool last4 = LOW;

int menuLevel = 0;
int indexMenuJocuri = 0;

struct QuizQuestions{
      String question;
      String Variant_A;
      String Variant_B;
      int corect; 
      };
QuizQuestions myQuiz[] ={ {"Cat face 5+5?", "10", "5", 1},{"Ce culoare are BMO?", "Negru", "Verde", 2}, {"Care este capitala Frantei?", "Paris", "Bucuresti", 1} };

int indexIntrebari = 0;
int userScore = 0;
int cursorQuiz=1;

int capX=64;
int capY=48;
int mancareaX=20;
int mancareaY=20;
int directia =1;
int segmenteX[40];
int segmenteY[40];
int lungimeSarpe=3;
unsigned long candAMancatUltimaOara = 0;
bool jocActiv=true;

void afiseazaMeniu1();
void meniuSecundar();
void jocSnake();
void ecranFinalSnake();
void pornesteJocNouSnake();
void afisareIntrebari(int idx);
void procesareIntrebari(int alegere);
void afiseaza_jocurile();
void handleSelect();
static bool SERIAL_init();
static bool LCD_init();


//menuLevel = 0 inseamna meniul principal, cel cu mini-BMO
//menuLevel = 1 inseamna meniul secundar, cel cu lista de aplicatii
//menuLevel = 2 inseamna meniul cu toate jocurile
//menuLevel = 3 inseamna meniul cu pozele
//menuLevel = 4 inseamna interfata cu jocul de quiz



void setup() {
  SERIAL_init();
  PCA_initialize(PCA_ADDRESS);
  LCD_init();
  LcdUtils_init(&lcd);

  // --- CONFIGURARE PINI SPI ---
  pinMode(SD_CS_PIN, OUTPUT);
  pinMode(LCD_CS_PIN, OUTPUT);
  
  // Începem prin a dezactiva ambele (HIGH = Deselectat)
  digitalWrite(SD_CS_PIN, HIGH);
  digitalWrite(LCD_CS_PIN, HIGH);

  // --- INITIALIZARE SD ---
  // Activăm doar SD-ul pentru momentul inițializării
  digitalWrite(SD_CS_PIN, LOW); 
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("SD card failed!");
    digitalWrite(SD_CS_PIN, HIGH);
  } else {
    digitalWrite(SD_CS_PIN, HIGH); // Eliberăm după succes
    
    Serial.println("Reading SD Card files...");
    SD_RootDir = SD.open("/");
    SDCardFilesNr = SD_getFilesList(SD_RootDir, (char*)SD_files_list, SD_FILES_MAX_NUM, SD_FILES_NAME_MAX_LEN);
    
    for (int idx = 0; idx < SDCardFilesNr; idx++) {
      Serial.println(SD_files_list[idx]);
    }
  }
  // Pinii butoanelor
  pinMode(SW1_PIN, INPUT);
  pinMode(SW2_PIN, INPUT);
  pinMode(SW3_PIN, INPUT);
  pinMode(SW4_PIN, INPUT);

  lcd.fillScreen(BMO_GREEN);
  afiseazaMeniu1();

}


void loop(){
  static unsigned long lastAnim = 0;
  static bool manaSus = true;

  if (menuLevel == 0) { 
    if(millis() - lastAnim > 500){
      lastAnim = millis();
      lcd.fillRect(79, 35, 8, 20, BMO_GREEN); 
      if(manaSus) lcd.drawLine(79, 45, 83, 38, BMO_DARK); 
      else lcd.drawLine(79, 45, 84, 45, BMO_DARK); 
      manaSus = !manaSus;
    }
  }

  btn1pressmenu();
  btn2pressmenu();
  btn3pressmenu();
  btn4pressmenu();

  navigationManager();
  handleSelect();

}

void btn1pressmenu(void){
  bool curent1 = digitalRead(SW1_PIN);
  if(curent1==HIGH && last1==LOW) status1=1;
  last1=curent1;
}

void btn2pressmenu(void) {
  bool curent2 = digitalRead(SW2_PIN);
  if (curent2 == HIGH && last2 == LOW) status2=1;
  last2 = curent2;
}

void btn3pressmenu(void) {
  bool curent3 = digitalRead(SW3_PIN);
  if (curent3 == HIGH && last3 == LOW) status3=1;
  last3 = curent3;
}

void btn4pressmenu(void){
  bool curent4 = digitalRead(SW4_PIN);
  if(curent4==HIGH && last4==LOW) status4=1;
  last4=curent4;
}

void updateCursor1(void) {
  lcd.fillRect(5, 75, 10, 10, BMO_GREEN);  
  lcd.fillRect(65, 75, 10, 10, BMO_GREEN); 
  lcd.setTextColor(BMO_DARK);
  if(indexMenu == 0) {
    lcd.setCursor(5, 75); 
    lcd.print("> "); 
  } else {
    lcd.setCursor(65, 75); 
    lcd.print(">"); 
  }
}

void updateCursor2(void) {
  lcd.fillRect(0, 30, 14, 30, BMO_GREEN); 
  lcd.setTextColor(BMO_DARK);
  if (indexMenu == 0){ lcd.setCursor(2, 30); delay(250);}
  else if (indexMenu == 1) {lcd.setCursor(2, 40); delay(250);}
  else if (indexMenu == 2) {lcd.setCursor(2, 50); delay(250);}
  lcd.print(">");
}

void navigationManager(void) {
  if (menuLevel == 0) {
    if (status2 == 1) { indexMenu = 0; updateCursor1(); status2 = 0; }
    if (status3 == 1) { indexMenu = 1; updateCursor1(); status3 = 0; }
  } 


  else if (menuLevel == 1) {//navigare meniu secundar cu toate aplicatiile
    if (status2 == 1) {
      menuLevel = 0; 
      indexMenu = 0;
      afiseazaMeniu1(); 
      status1=0; status2=0; status3=0; status4=0;
      delay(250);
    }
    if (status1 == 1) { 
      indexMenu++;
      if (indexMenu > 2) indexMenu = 0;
      updateCursor2();
      status1 = 0;
      delay(200);
    }
    if (status4 == 1) { 
      indexMenu--;
      if (indexMenu < 0) indexMenu = 2;
      updateCursor2();
      status4 = 0;
      delay(200);
    }
  }


  else if (menuLevel == 2) { //asta e pentru meniul de jocuri
    if (status4 == 1) { 
      menuLevel = 1; 
      status1=0; status2=0; status3=0; status4=0;
      meniuSecundar(); 
      delay(300); 
    }
    if(status3==1){
      indexMenuJocuri++;;
      if(indexMenuJocuri>=3){
        indexMenuJocuri=0;
      }
      afiseaza_jocurile();
      delay(300);
      status3=0;
    }
    if(status2==1){
      indexMenuJocuri--;
      if(indexMenuJocuri<0){
        indexMenuJocuri=2;
      }
      afiseaza_jocurile();
      delay(300);
      status2=0;
    }
    
  }


  else if(menuLevel==3){ //asta este pentru meniul de poze
    if(status3==1){
      PhotoIndex++;
      if(PhotoIndex>=SDCardFilesNr) PhotoIndex=0;
      afiseazaPoza(PhotoIndex);
      status3=0;
      delay(200);
    }
    if(status2==1){
      PhotoIndex--;
      if(PhotoIndex<0) PhotoIndex=SDCardFilesNr;
      afiseazaPoza(PhotoIndex);
      status2=0;
      delay(200);
      
    }
    if (status4 == 1) { 
      menuLevel = 1; 
      status1=0; status2=0; status3=0; status4=0;
      meniuSecundar(); 
      delay(300); 
    }
    
  }


  else if(menuLevel==4){ //asta este navigare raspunsuri quiz
    if(status4==1){
      menuLevel=2;
      afiseaza_jocurile();
      status1=0; status2=0; status3=0; status4=0;
      delay(300);
    }
    if(status2==1){
      cursorQuiz=1;
      afisareIntrebari(indexIntrebari);
      status2=0;
      delay(300);
    }
    if(status3==1){
      cursorQuiz=2;
      afisareIntrebari(indexIntrebari);
      status3=0;
      delay(300);
    }
    if(status1==1){
      procesareIntrebari(cursorQuiz);
      status1=0;
      delay(300);
    }
    
  }


  else if(menuLevel==5){ //navigare joc snake
    
    jocSnake();
  }


  else if(menuLevel==6){ //navigare joc minesweeper
    if(status4==1){
      menuLevel=2;
      status1=0; status2=0; status3=0; status4=0;
      afiseaza_jocurile();
      delay(300);
    }
  }
}


void handleSelect(void){
  if(menuLevel == 0 && status1 == 1){
    if(indexMenu == 0){
      menuLevel = 1;
      meniuSecundar();
    }
    else {
      lcd.fillScreen(BMO_GREEN);
      lcd.setCursor(35, 30);
      lcd.print("OK, MAYBE LATER.");
    }
    status1 = 0;
  }

  else if (menuLevel == 1 && status3 == 1){
    int currentChoice = indexMenu; 
    menuLevel = 2;
    lcd.fillScreen(BMO_GREEN);
    lcd.setCursor(10, 80);
    if(currentChoice == 0){
      menuLevel = 2;
      indexMenuJocuri=0;
      afiseaza_jocurile();
      status1 = 0;
      status2 = 0;
      status3 = 0;
      status4 = 0;
      delay(300); 
    }

    if(currentChoice == 1){
      menuLevel = 3;
      PhotoIndex = 0;
      afiseazaPoza(PhotoIndex);
      status1 = 0;
      status2 = 0;
      status3 = 0;
      status4 = 0;
      delay(500); 
    }

    if(currentChoice == 2) lcd.print("PLAYING MUSIC...");
    
    status1=0; status2=0; status3=0; status4=0; 
  }
  else if(menuLevel == 2 && status1 == 1){
    if(indexMenuJocuri == 0){ // Quiz
      menuLevel = 4;
      indexIntrebari = 0;
      cursorQuiz = 1;
      afisareIntrebari(indexIntrebari);
    }
    else if(indexMenuJocuri == 1){ // Snake
      menuLevel = 5;
      lcd.fillScreen(BMO_GREEN);
      pornesteJocNouSnake();
      delay(300);
    }
    else if(indexMenuJocuri == 2){ // Minesweeper
      menuLevel = 6;
      lcd.fillScreen(BMO_GREEN);
      lcd.setTextSize(1);
      lcd.setCursor(10, 40);
      lcd.print("Mine not done yet");
    }
    
    status1 = 0; status2 = 0; status3 = 0; status4 = 0;
    delay(500);
  }
}


void afiseazaMeniu1(void) {
  lcd.fillScreen(BMO_GREEN);
  lcd.setTextColor(BMO_DARK);
  lcd.setTextSize(1);
  lcd.setCursor(15, 0); lcd.print("Who wants to play");
  lcd.setCursor(28, 10); lcd.print("Video Games?");
  lcd.setCursor(20, 75); lcd.print("Let's");
  lcd.setCursor(18, 85); lcd.print("begin!");
  lcd.setCursor(77, 75); lcd.print("Maybe");
  lcd.setCursor(77, 85); lcd.print("later.");
  
  lcd.fillRoundRect(54, 30, 22, 30, 3, BMO_DARK);
  lcd.fillRoundRect(57, 33, 16, 14, 1, BMO_GREEN);
  lcd.drawPixel(62, 38, BMO_DARK);
  lcd.drawPixel(68, 38, BMO_DARK);
  lcd.drawLine(63, 43, 65, 45, BMO_DARK);
  lcd.drawLine(65, 45, 67, 43, BMO_DARK);
  lcd.drawLine(54, 45, 48, 55, BMO_DARK);
  lcd.drawFastVLine(61, 60, 4, BMO_DARK);
  lcd.drawFastVLine(68, 60, 4, BMO_DARK);

  updateCursor1(); 
}

void meniuSecundar(void){
  indexMenu = 0; 
  lcd.fillScreen(BMO_GREEN);
  lcd.setTextColor(BMO_DARK);
  lcd.setTextSize(1);
  lcd.setCursor(7, 10); lcd.print("------BMO MENU------");
  lcd.setCursor(15, 30); lcd.print("1. Games");
  lcd.setCursor(15, 40); lcd.print("2. Pictures");
  lcd.setCursor(15, 50); lcd.print("3. Music");
  lcd.setCursor(5, 85); 
  lcd.setTextColor(BMO_DARK); 
  lcd.print("Scor: "); lcd.print(userScore);
  updateCursor2(); // Desenează cursorul fix la poziția 0
}

void afiseazaPoza(int index) { //aka meniu tertiar (menuLevel=3)
  if (SDCardFilesNr == 0) {
    Serial.println("EROARE: Nu am gasit niciun fisier .bmp pe card!");
    return;
  }
  char* nume = &SD_files_list[index][0];
  Serial.print("Incerc sa deschid: ");
  Serial.println(nume);
  
  LcdUtils_bmpDraw(nume, 0, 0);
}

  void afiseaza_jocurile(void) { //aka meniu cuaternar (menuLevel=4)
  lcd.fillScreen(BMO_GREEN);
  lcd.setTextColor(BMO_DARK);
  lcd.setTextSize(1);
  lcd.setCursor(10, 10);
  lcd.print("SELECT GAME:");
  lcd.setCursor(5, 85); 
  lcd.setTextColor(BMO_DARK); 
  lcd.print("Scor: "); lcd.print(userScore);

  lcd.setTextSize(2); 
  lcd.setCursor(10, 40);

  switch(indexMenuJocuri) {
    case 0: 
      lcd.print("QUIZ GAME"); 
      lcd.setTextSize(1);
      lcd.setCursor(10, 70);
      lcd.print("Press SW1 to Start");
      break;
    case 1: 
      lcd.print("SNAKE"); 
      lcd.setTextSize(1);
      lcd.setCursor(10, 70);
      lcd.print("Press SW1 to Start");
      break;
    case 2: 
      lcd.setCursor(2, 40);
      lcd.print("MINESWEEPER"); 
      break;
  }
  
}


/***************************************************************************************************
 * GAMES FUNCTIONS
 ***************************************************************************************************/
  void afisareIntrebari(int idx) {
  lcd.fillScreen(ST77XX_BLACK);
  lcd.setTextSize(1);
  lcd.setTextColor(ST77XX_WHITE);

  lcd.setCursor(5, 5); 
  lcd.println(myQuiz[idx].question);

  if (cursorQuiz == 1) {
    lcd.setCursor(2, 30); 
    lcd.print(">");
  }
  lcd.setCursor(15, 30);
  lcd.print("A: "); lcd.print(myQuiz[idx].Variant_A);

  if (cursorQuiz == 2) {
    lcd.setCursor(2, 55); 
    lcd.print(">");
  }
  lcd.setCursor(15, 55);
  lcd.print("B: "); lcd.print(myQuiz[idx].Variant_B);

  lcd.setCursor(5, 85); 
  lcd.setTextColor(ST77XX_GREEN); 
  lcd.print("Scor: "); lcd.print(userScore);
}


  void procesareIntrebari(int alegere){

        lcd.fillScreen(BMO_GREEN);
        lcd.setCursor(30, 45);
        lcd.setTextSize(2);
        lcd.setTextColor(BMO_DARK);
      if(alegere == myQuiz[indexIntrebari].corect){
        userScore+=10;
        
        lcd.print("CORECT!");
      }
      else{
        lcd.print("UPS....");
      }
      delay(2500);

      indexIntrebari++;
      if(indexIntrebari>=3){
        indexIntrebari = 0;
      }
      cursorQuiz=1;
      afisareIntrebari(indexIntrebari);


  }





 void jocSnake(void) {
  if (jocActiv) {
    // 1. GESTIONARE DIRECȚIE (Butoanele schimbă DOAR variabila 'directia')
    // SW4 = SUS
    if (status4 == 1) { 
      if (directia != 2) directia = 0; // Mergi SUS (0) doar dacă nu mergi deja JOS (2)
      status4 = 0; 
    }
    // SW1 = JOS (Săgeata de jos conform cerinței tale)
    if (status1 == 1) { 
      if (directia != 0) directia = 2; // Mergi JOS (2) doar dacă nu mergi deja SUS (0)
      status1 = 0; 
    }
    // SW2 = STÂNGA
    if (status2 == 1) { 
      if (directia != 1) directia = 3; // Mergi STÂNGA (3) doar dacă nu mergi deja DREAPTA (1)
      status2 = 0; 
    }
    // SW3 = DREAPTA
    if (status3 == 1) { 
      if (directia != 3) directia = 1; // Mergi DREAPTA (1) doar dacă nu mergi deja STÂNGA (3)
      status3 = 0; 
    }

    // 2. MIȘCARE AUTOMATĂ (Se execută la intervalul de 150ms)
    if (millis() - candAMancatUltimaOara > 150) {
      candAMancatUltimaOara = millis();

      // Actualizăm poziția segmentelor corpului (de la coadă spre cap)
      for (int i = lungimeSarpe - 1; i > 0; i--) {
        segmenteX[i] = segmenteX[i - 1];
        segmenteY[i] = segmenteY[i - 1];
      }
      
      // Vechea poziție a capului devine primul segment al corpului
      segmenteX[0] = capX;
      segmenteY[0] = capY;

      // Calculăm noua poziție a capului în funcție de direcție
      if (directia == 0) capY -= 4; // SUS
      if (directia == 1) capX += 4; // DREAPTA
      if (directia == 2) capY += 4; // JOS
      if (directia == 3) capX -= 4; // STÂNGA

      // 3. VERIFICARE COLIZIUNI
      // Lovire pereți
      if (capX < 0 || capX >= 128 || capY < 0 || capY >= 96) {
        jocActiv = false;
        ecranFinalSnake();
        return;
      }
      
      // Lovire corp propriu
      for (int i = 0; i < lungimeSarpe; i++) {
        if (capX == segmenteX[i] && capY == segmenteY[i]) {
          jocActiv = false;
          ecranFinalSnake();
          return;
        }
      }

      // Verificare dacă a mâncat
      if (abs(capX - mancareaX) < 4 && abs(capY - mancareaY) < 4) {
        lungimeSarpe++;
        userScore += 5;
        // Generăm mâncare nouă (multiplu de 4 pentru a fi aliniată cu șarpele)
        mancareaX = (random(1, 30) * 4);
        mancareaY = (random(2, 22) * 4);
      }

      // 4. DESENARE (Rămâne în interiorul timer-ului pentru a nu licări ecranul)
      lcd.fillScreen(ST77XX_BLACK);
      
      // Desenăm mâncarea
      lcd.fillRect(mancareaX, mancareaY, 4, 4, ST77XX_RED);
      
      // Desenăm corpul
      for (int i = 0; i < lungimeSarpe; i++) {
        lcd.fillRect(segmenteX[i], segmenteY[i], 4, 4, BMO_GREEN);
      }
      
      // Desenăm capul (alb pentru a-l distinge)
      lcd.fillRect(capX, capY, 4, 4, ST77XX_WHITE);
      
      // Afișăm scorul în colț
      lcd.setCursor(2, 2);
      lcd.setTextColor(ST77XX_WHITE);
      lcd.setTextSize(1);
      lcd.print("Scor: "); lcd.print(userScore);
    }
  } else {
    // LOGICĂ ECRAN FINAL (Game Over)
    if (status1 == 1) { // SW1 pentru Restart
      status1 = 0;
      pornesteJocNouSnake();
    }
    if (status4 == 1) { // SW4 pentru Ieșire în meniu
      status4 = 0;
      menuLevel = 2;
      afiseaza_jocurile();
    }
  }
}
  void ecranFinalSnake() {
  lcd.fillScreen(BMO_DARK);
  lcd.setTextColor(ST77XX_WHITE);
  lcd.setTextSize(2);
  lcd.setCursor(15, 20);
  lcd.print("AI PIERDUT");
  
  lcd.setTextSize(1);
  lcd.setCursor(10, 50);
  lcd.print("SW1: JOACA DIN NOU");
  lcd.setCursor(10, 70);
  lcd.print("SW4: IESIRE MENIU");
  
  lcd.setCursor(40, 85);
  lcd.print("Scor: "); lcd.print(userScore);
}

void pornesteJocNouSnake() {
  capX = 64; 
  capY = 48;
  lungimeSarpe = 3;
  directia = 1;
  userScore = 0;
  jocActiv = true;
  mancareaX = 40;
  mancareaY = 40;
  lcd.fillScreen(ST77XX_BLACK);
}
/***************************************************************************************************
 * INITIALIZATION FUNCTIONS
 ***************************************************************************************************/

static bool LCD_init(void) {
  pinMode(LCD_CS_PIN, OUTPUT);
  digitalWrite(LCD_CS_PIN, HIGH);
  pinMode(LCD_BCKL_PIN, OUTPUT);
  digitalWrite(LCD_BCKL_PIN, HIGH);

  pinMode(LCD_RST_PIN, OUTPUT);
  digitalWrite(LCD_RST_PIN, HIGH);
  delay(100);
  digitalWrite(LCD_RST_PIN, LOW);
  delay(100);
  digitalWrite(LCD_RST_PIN, HIGH);
  delay(200);

  SPI.begin();
  lcd.initR(INITR_TDO128x96);
  lcd.setRotation(0);
  lcd.fillScreen(BMO_GREEN);
  lcd.setTextWrap(false);
  return true;
}

static bool SERIAL_init(void) {
  Serial.begin(115200);
  unsigned long startWait = millis();
  while (!Serial && (millis() - startWait < 3000)) delay(10);
  Serial.print("BMO System Ready\n");
  return true;
}
uint8_t SD_getFilesList(File Dir, char* FilesList, uint8_t maxListSize, uint8_t maxNameSize) {
  uint8_t count = 0;
  while (true) {
    File entry = Dir.openNextFile();
    if (!entry) break; 
    
    String fileName = entry.name();
    if (!entry.isDirectory() && fileName.endsWith(".bmp")) {
      strncpy(&FilesList[count * maxNameSize], fileName.c_str(), maxNameSize - 1);
      count++;
    }
    entry.close();
    if (count >= maxListSize) break;
  }
  return count;
}