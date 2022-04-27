#include "HX711.h"
#include <dht.h>
#include <LiquidCrystal_I2C.h>

//set pins as constant so cant be changed
const int LED = 12;
const int Buzzer = 11;
const int BUTTON_PIN = 4;

typedef enum {START, INIT, COLLECT_DATA, ALERT, TEST_COMPLETE} SystemState;
SystemState sysState = START;

const double NEXT_WEIGHT = 0.1;

double load;
//ultrasonic sensor pins
#define echoPin 13  // attach pin D2 Arduino to pin Echo of HC-SR04
#define trigPin 10 //attach pin D3 Arduino to pin Trig of HC-SR04

// load sensor pins
#define LOADCELL_DOUT_PIN  7
#define LOADCELL_SCK_PIN  6


// Temp / humidity sensor pin
#define DHT11_PIN 8

// LCD screen pins
LiquidCrystal_I2C lcd(0x27, 16, 2);

HX711 scale;

long calibration_factor = -215000;

// ultrasonic range sensor variables
long duration; // variable for the duration of sound wave travel
double distance; // variable for the distance measurement
const double offset = 0.015;

// load sensor variables
double prevLoad = 0;
double currentLoad = 0;

// temp humidity variables
dht DHT;

// button variable
int buttonState = 0;
int buttonCount = 0;

double prevDist = 0;
double currentDistance = 0;

int prevBtn = LOW;
bool btnPressed = false;

int counter = 0;

void setup() {
  // -------ultrasonic sensor setup begins----------------

  pinMode(trigPin, OUTPUT); // Sets the trigPin as an OUTPUT
  pinMode(echoPin, INPUT); // Sets the echoPin as an INPUT
  Serial.begin(9600); // // Serial Communication is starting with 9600 of baudrate speed

  // -------ultrasonic sensor setup ends------------------

  // -------load sensor setup begins----------------------

  Serial.begin(9600);

  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  scale.set_scale(calibration_factor);
  scale.tare();   //Reset the scale to 0

  long zero_factor = scale.read_average(); //Get a baseline reading

  //--------load sensor setup ends------------------------

  // lcd screen setup
  lcd.begin();
  lcd.backlight();
  lcd.clear();

  //button
  // initialize serial communication at 9600 bits per second:
  Serial.begin(9600);
  pinMode(BUTTON_PIN, INPUT);

  //LED
  pinMode(LED, OUTPUT);

  //Buzzer
  pinMode(Buzzer, OUTPUT);
 
}

void loop() {
  pollButton();
  ticFunc();
  delay(50);
}


//Catch Rising Edge for Button Press
void pollButton() {

  int value = digitalRead(BUTTON_PIN);
  if (value == HIGH && prevBtn == LOW) {
    btnPressed = true;
  }
  else {
    btnPressed = false;
  }
  prevBtn = value;

}


void ticFunc() {

  //State Transitions
  //runs once when system is first turned on, clears lcd and goes to initialization state
  switch (sysState) {
    case START: {
        lcd.clear();
        sysState = INIT;
        break;
      }

    //clears lcd and goes to data collection state
    case INIT: {
        if (btnPressed) {
          lcd.clear();
          sysState = COLLECT_DATA;
        }
        break;
      }

    //clears lcd and goes to alert state if weight is approaching 5kg else goes to testing complete state
    case COLLECT_DATA: {
        if (currentLoad > 4.5) {
          lcd.clear();
          sysState = ALERT;
        }
        else if (btnPressed) {
          lcd.clear();
          sysState = TEST_COMPLETE;
        }
        break;
      }

    //clears lcd and goes back to initialization state
    case ALERT : {
        if (btnPressed) {
          lcd.clear();
          sysState = INIT;
        }
        break;
      }

    //clears lcd and goes back to initialization state
    case TEST_COMPLETE: {
      lcd.clear();
        sysState = INIT;
        break;
      }
  }


  //STATE ACTIONS
  switch (sysState) {

    //turns off buzzer and led and prompts user to load material and to press button to continue i.e. next state
    case INIT: {
        digitalWrite(LED, LOW);
        noTone(Buzzer);
        lcd.setCursor(0, 0);
        lcd.print("Load Material");
        lcd.setCursor(0, 1);
        lcd.print("BTN to start");
        break;
      }

    //getting rolling average of load and distance and dislays to lcd as well as promts user to press button to continue i.e. next state
    case COLLECT_DATA: {
        currentLoad = getLoad();
        currentDistance = getDistance();

        for (int i = 0; i < 10; i++) {
          updateDistance();
          updateLoad();
        }

        Serial.begin(9600);
        lcd.setCursor(0, 0);
        lcd.print((updateDistance() / 100) + offset , 4);
        lcd.print("m");
        lcd.print(" ");
        Serial.print((updateDistance() / 100) + offset , 4);
        Serial.print(" ");

        if (((distance / 100) + offset) > 0.25) {
          lcd.print(updateLoad() - 0.05 , 3);
          lcd.print("kg");
          Serial.println(updateLoad() - 0.05 , 3);
        }
        else {
          lcd.print(updateLoad() + 0.1, 3);
          lcd.print("kg");
          Serial.println(updateLoad() + 0.1, 3);
        }

        lcd.setCursor(0, 1);
        lcd.print("BTN when done");
        break;
      }

    //turns onf buzzer and led alerting user approaching 5kg and to press button to restart system
    case ALERT: {
        Serial.end();
        digitalWrite(LED, HIGH);
        tone(Buzzer, 1000);
        lcd.setCursor(0, 0);
        lcd.print("Error Max Load");
        lcd.setCursor(0, 1);
        lcd.print("BTN to restart");
        break;
      }

    //tells user testing is complete and to user cool win term to export data to excel, instructions on readme on github
    case TEST_COMPLETE: {
        lcd.setCursor(0, 0);
        lcd.print("TESTING");
        lcd.setCursor(0, 1);
        lcd.print("COMPLETE");
        delay(5000);
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("GET DATA USING");
        lcd.setCursor(0, 1);
        lcd.print("CoolTerm + Excel");
        prevDist = 0;
        delay(5000);
        Serial.end();
        break;
      }
  }
}

//system functions

//gets distance and calibrates based on temp/huminity
double getDistance() {
  double duration, distance, prevDist;
  double speed;
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  duration = pulseIn(echoPin, HIGH);
  speed = 331.4 + (0.606 * DHT.temperature) + (0.0124 * DHT.humidity);
  distance = (duration / 2) * (speed / 10000);
  return distance;
}

//gets load with offset for precise weight
double getLoad() {
  currentLoad = (((scale.get_units() * 0.453592) - 0.65 ) ) ;
  return currentLoad;
}

//rolling average of distance to elimanate noise in signal
double updateDistance() {
  double currDist = getDistance();
  distance = ((1.0 - NEXT_WEIGHT) * distance) + (NEXT_WEIGHT * currDist);
}

//rolling average of load to elimanate noise in signal
double updateLoad() {
  double currLoad = getLoad();
  load = ((1.0 - NEXT_WEIGHT) * load) + (NEXT_WEIGHT * currLoad);
}
