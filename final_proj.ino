/**
 *   @file   final_proj.ino
 *   @author    Mel Steppe, Sean Zhao
 *   @date      21-March-2025
 *   @brief     Automatic cat feeder program using FreeRTOS 
 *   @details   File includes 3 tasks to drive an automatic cat feeder inlcuding RFID detection and motor driver, reset task,
 *   and changing the interval length task. 
 */

//Libraries
#include <ESP32Servo.h>
#include "RTClib.h"
#include <SPI.h>
#include <Wire.h>
#include <EEPROM.h>
#include <LiquidCrystal_I2C.h>
#include <MFRC522v2.h>
#include <MFRC522DriverSPI.h>
#include <MFRC522DriverPinSimple.h>
#include <MFRC522Debug.h>

// Define SPI pins
#define SS_PIN 8
#define SCK 12
#define MOSI 11
#define MISO 13

// Define I2C pins
#define SDApin 4
#define SCLpin 5

// Define EEPROM settings
#define EEPROM_SIZE 64      // Adjust based on storage needs
#define MAX_USERS 10        // Max RFID users
#define UID_SIZE 4          // 4 bytes per UID

// Initialize MFRC RFID reader
MFRC522DriverPinSimple ss_pin(SS_PIN);
MFRC522DriverSPI driver{ss_pin};
MFRC522 rfid{driver};

// Initialize LCD 
LiquidCrystal_I2C lcd(0x27,16,2);

// Initialize RTC Module
RTC_DS3231 rtc; 
DateTime nowDT;

// Initialize Servo motor
Servo myservo;  

// Useful Variables
int pos = 0;    // variable to store the servo position
bool beenFed = false; // signal to determine when cat is allowed to be fed
int interval = 5; // length of interval in minutes
//const int addRFIDbutton = 38; 
const int incrementButton = 37; //pin for increment button
const int decrementButton = 36; //pin for decrement button
int oldIncrVal = LOW; //button state
int oldDecrVal = LOW; //button state


void saveUIDToEEPROM(byte *uid); 
bool isRecognized(byte *uid);
void loadUIDsFromEEPROM();

/**
 * The standard Arduino setup function used for peripherial intitializations and creation of tasks.
 */
void setup() {
  Serial.begin(115200); 
  myservo.attach(7);
  Wire.begin(SDApin, SCLpin);
  lcd.init();
  lcd.backlight();
  lcd.clear();
  rtc.begin();
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__))); //takes current time from Computer

  SPI.begin(SCK, MISO, MOSI, SS_PIN);
  rfid.PCD_Init();
    
  // Initialize EEPROM
  if (!EEPROM.begin(EEPROM_SIZE)) {
      Serial.println("Failed to initialize EEPROM");
      return;
  }

  loadUIDsFromEEPROM();  // Load stored UIDs into memory

  //pinMode(addRFIDbutton, INPUT_PULLUP); //removed task
  pinMode(incrementButton, INPUT_PULLUP);
  pinMode(decrementButton, INPUT_PULLUP);
 
  xTaskCreatePinnedToCore(resetFeederTask, "Feeder Reset", 4096, NULL, 1, NULL, 1); // Core 1
  xTaskCreatePinnedToCore(servoMotorTask, "Servo Motor", 4096, NULL, 1, NULL, 0); // Core 0
  xTaskCreatePinnedToCore(adjustIntervalTask, "Adjust Interval", 2048, NULL, 1, NULL, 1); // Core 0
 
}


/**
 * The standard Arduino loop function used for repeating tasks. 
 * @note Unnecessary for freeRTOS programming.
 */
void loop() {}


/**
 * Reset Feeder Task: Gets the current time with the RTC, and changes the beenFed flag when the interval resets.
 */
void resetFeederTask(void* parameter){
  while (1){
    nowDT = rtc.now();
    byte myMinute = nowDT.minute();
    byte mySec = nowDT.second();

    if ((myMinute % interval == 0) && (mySec == 0)) {
      beenFed = false;
      Serial.println("Ready to Feed");
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}


/**
 * Servo Motor Task: Checks for a valid RFID UID, and if it is a recognized user and feeding is allowed,
 * triggers the servo motor to release cat food. 
 */
void servoMotorTask(void* parameter){
  while(1){
    if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
        vTaskDelay(pdMS_TO_TICKS(50));
        continue;
    }
    if (isRecognized(rfid.uid.uidByte)) {
      if (!beenFed) {
        Serial.print("Granted");

        for (pos = 0; pos <= 45; pos += 1) { // goes from 0 degrees to 180 degrees
          // in steps of 1 degree
          myservo.write(pos);              // tell servo to go to position in variable 'pos'
          vTaskDelay(pdMS_TO_TICKS(100));                      // waits 15 ms for the servo to reach the position
        }
          for (pos = 45; pos >= 0; pos -= 1) { // goes from 180 degrees to 0 degrees
          myservo.write(pos);              // tell servo to go to position in variable 'pos'
          vTaskDelay(pdMS_TO_TICKS(100));                      // waits 15 ms for the servo to reach the position
        }

       
        beenFed = true;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
    lcd.clear();
  }
  
}

/**
 * Adjust Interval Task: Takes in input from two buttons to increase or decrease the interval length
 * and displays the new length on the LCD. 
 */
void adjustIntervalTask(void* parameter){
  while (1){
    int incrSignal = digitalRead(incrementButton);
    int decrSignal = digitalRead(decrementButton);

    if (incrSignal != oldIncrVal){
      if (incrSignal == LOW) {
        interval++;
        Serial.println(interval);
        lcd.print(interval);
        lcd.print(" minutes");
      }
      oldIncrVal = incrSignal;
    }

    if (decrSignal != oldDecrVal) {
      if (decrSignal == LOW) {
        interval--;
        Serial.println(interval);
        lcd.print(interval);
        lcd.print(" minutes");
      }
      oldDecrVal = decrSignal;
    }

    vTaskDelay(pdMS_TO_TICKS(1000));
    lcd.clear();
    

  }

}

// removed task to add new UID to authorized users
// void addRFIDTask(void* parameter){
//   while(1) {
//     int value = digitalRead(addRFIDbutton);

//     if(value == HIGH){
//       if(!isRecognized(rfid.uid.uidByte)){
//         saveUIDToEEPROM(rfid.uid.uidByte);
//         lcd.print("Added new Cat");
//       }
//     }
//     vTaskDelay(pdMS_TO_TICKS(1000));
//     lcd.clear();
//   }

  
//}

// Removed task to save new UIDs to eeprom
// void saveUIDToEEPROM(byte *uid) {
//     for (int i = 0; i < MAX_USERS; i++) {
//         bool emptySlot = true;
//         for (int j = 0; j < UID_SIZE; j++) {
//             if (EEPROM.read(i * UID_SIZE + j) != 0xFF) {
//                 emptySlot = false;
//                 break;
//             }
//         }
//         if (emptySlot) {
//             for (int j = 0; j < UID_SIZE; j++) {
//                 EEPROM.write(i * UID_SIZE + j, uid[j]);
//             }
//             EEPROM.commit(); // Save changes to flash memory
//             return;
//         }
//     }
//     Serial.println("No space left to store new users!");
// }



/**
 * isRecognized: Checks if a UID is already in EEPROM.
 */
bool isRecognized(byte *uid) {
    for (int i = 0; i < MAX_USERS; i++) {
        bool match = true;
        for (int j = 0; j < UID_SIZE; j++) {
            if (EEPROM.read(i * UID_SIZE + j) != uid[j]) {
                match = false;
                break;
            }
        }
        if (match) return true;
    }
    return false;
}

/**
  * LoadUIDsFromEEPROM: Load UIDs from EEPROM into memory for long term storage
  */
void loadUIDsFromEEPROM() {
    Serial.println("Loaded stored RFID users:");
    for (int i = 0; i < MAX_USERS; i++) {
        byte uid[UID_SIZE];
        bool emptySlot = true;

        for (int j = 0; j < UID_SIZE; j++) {
            uid[j] = EEPROM.read(i * UID_SIZE + j);
            if (uid[j] != 0xFF) emptySlot = false;
        }

        if (!emptySlot) {
            Serial.print("User ");
            Serial.print(i + 1);
            Serial.print(": ");
            for (int j = 0; j < UID_SIZE; j++) {
                Serial.print(uid[j] < 0x10 ? " 0" : " ");
                Serial.print(uid[j], HEX);
            }
            Serial.println();
        }
    }
}