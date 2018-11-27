#include "Arduino.h"
#include "SoftwareSerial.h"
#include "DFRobotDFPlayerMini.h"

SoftwareSerial mySoftwareSerial(10, 11); // RX, TX
DFRobotDFPlayerMini myDFPlayer;
void printDetail(uint8_t type, int value);

/* Carrera DIGITAL 124/132/143 arduino: Infrared car detector

   Dipl.-Ing. Peter Niehues CC BY-NC-SA 3.0

   This is an example software to decode infrared identifier of carrera digital slot cars.
   For further information (german only)
   visit http://www.wasserstoffe.de/carrera-hacks/startampel/

   This work is licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 3.0 Unported License.
   To view a copy of this license, visit http://creativecommons.org/licenses/by-nc-sa/3.0/.

   This license allows you to remix, tweak, and build upon my work non-commercially, as long as
   you credit Peter Niehues and license the new creations under the identical terms.

*/
int carID                     = 99;                  // save current car ID
boolean identified            = false;               // indicates that car has already been indentified
unsigned long previousMicros  = 0;                   // last runtime of interrupt call
unsigned long currentMicros   = 0;                   // current runtime
unsigned long interval        = 0;                   // time interval between last and current interrupt call
int btnState                  = HIGH;
int previousBtnState          = HIGH;

const int maxDrivers          = 3;                   // Nombre maximal de voitures
int raceLaps                  = 10;                  // Nombre de tour à effectuer au total
unsigned long raceStartTime   = 0;
unsigned long raceBestLap     = 0;

int startSequence                     = 0;
unsigned long startSequenceBegin      = 0;
int wrongStartSequence                = 0;
unsigned long wrongstartSequenceBegin = 0;
int wrongStartCar                     = 99;
int finishLineSequence                = 0;
unsigned long finishLineSequenceBegin = 0;

String players[maxDrivers]      = {"Joueur 1", "Joueur 2", "Joueur 3"}; // Manettes
String drivers[maxDrivers]      = {"Mario", "Peach", "Luigi"};          // Voitures
int driversPlayers[maxDrivers]  = {2, 1, 0}; // Association manette/voiture
byte driversVoices[3][5] = {
  {6,7,8,10,11}, // Voix Mario
  {12, 13, 14, 15}, // Voix Peach
  {9,9,9,9,9}, // Voix Luigi
};

// prevoir une structure drivers
/*
struct Driver = {
  byte car_id,
  String name,
  byte voices[4],
  byte lapCount = 0,
  unsigned long bestLap = 0,
  unsigned long lastLap = 0,
  unsigned long totalTime = 0,
  byte rank = 0
}

// Dans le setup, initialiser un tableau de pilote
array initDrivers() {
  for(int i=0; i < maxDrivers; i++) {
    struct Driver driver = { i, driversVoices[], drivers[driversPlayers[i]] };
    players[i] = driver;
  }
  
  return players
}
*/

int lapCount[maxDrivers]                 = {0, 0, 0}; // Compteur de tours
unsigned long bestLaps[maxDrivers]       = {0, 0, 0}; // Meilleurs temps au tour
unsigned long lastLaps[maxDrivers]       = {0, 0, 0}; // Temps cumulé au dernier tour
unsigned long totalTime[maxDrivers]      = {0, 0, 0}; // Temps total
int ranking[maxDrivers]                  = {};

// Set songs
const int songBootUp     = 1;
const int songStartRace  = 4;
const int songLastLap    = 18;
const int songFinishLine = 19;
const int songCountdown  = 20;

// Set inputs/outpus
enum { RED3, RED2, RED1, GO };
const byte startLine[4] = { 7, 8, 9, 6 };

const int startRace   = 4;

void setup() {                                          //////
  Serial.begin( 115200 );                               // initialize serial bus
  attachInterrupt(0, infraredDetect, FALLING);          // whenever level on Pin 2 falls, start detection routine
  pinMode(startRace, INPUT_PULLUP);

  for (int i = 0; i < sizeof(startLine); i++)
    pinMode(startLine[i], OUTPUT);

  // DF player
  mySoftwareSerial.begin(9600);
  if (!myDFPlayer.begin(mySoftwareSerial)) {  //Use softwareSerial to communicate with mp3.
    Serial.println(F("Unable to begin:"));
    Serial.println(F("1.Please recheck the connection!"));
    Serial.println(F("2.Please insert the SD card!"));
    while(true);
  }
  Serial.println(F("DFPlayer Mini online."));
  
  myDFPlayer.volume(25);  //Set volume value. From 0 to 30
  myDFPlayer.play(songBootUp);  //Play the first mp3
}                                                       //////

void loop() {                                           //////
  if (wrongStartSequence > 0) {
    // Séquence de Faux départ
    wrongStartSequenceWatch();
  } else {
    int btnState = digitalRead(startRace);
    if (btnState == LOW && previousBtnState != LOW) {
      if (startSequence > 0) {
        // Sequence de départ en cours, on annule la course
        Serial.println("Course annulée");
        resetStartSequence();
      } else {
        // Lancement de la course
        resetStartSequence();
        startSequence = 1;
        startSequenceBegin = millis();
      }
      previousBtnState = LOW;
      delay(50); /// Debounce
    } else if (btnState && previousBtnState != HIGH) {
      previousBtnState = HIGH;
      delay(50);
    } else if (startSequence > 0) {
      startSequenceWatch();
    }

    if (finishLineSequence > 0) {
      // Un concurrent franchit la ligne d'arrivée
      finishLineSequenceWatch();
    }
  }


  if ( carID < 8 ) {                                    // real car ID's are always smaler then 8
    // Serial.println( carID, DEC );                       // print ID to serial
    computeLapStat(carID);
    Serial.println(drivers[driversPlayers[carID]] + " vient de passer\n");
    
    carID = 99;                                         // set ID to unreal level
  }
}                                                       //////

void infraredDetect() {                                 //////
  currentMicros = micros();                             // save current runtime
  interval = currentMicros - previousMicros;            // interval to last interrupt call
  if ( interval > 30000 ) {                             // minimal time gap between two cars
    identified = false;                                 // a new car has to be detected
    previousMicros = currentMicros;                     // save runtime for next interrupt call
    return;
  }                                            //
  if ( identified ) return;                             // if car already detected, skip interrupt
  if ( interval < 60 ) {                                // must be an error
    previousMicros = currentMicros;                     // save runtime for a new try
    return;
  }                                            //
  carID = (interval / 64) - 1;                          // at this point interval is valid, calculate car ID
  identified = true;                                    // indicate that we have already identified the car
  previousMicros = currentMicros;                       // save runtime for next interrupt call
  if (startSequence > 0 && startSequence < 6) {
    wrongStartCar = carID;
    wrongStartSequence = 1;
  }
}

/**

*/
void startSequenceWatch() {
  if (startSequence > 0) {
    if (startSequence == 1) {
      Serial.println("Allumage des feux !");
      Serial.println("Début de la course dans 8.5s");
      myDFPlayer.play(songStartRace);  //Play the first mp3
      digitalWrite(startLine[RED1], HIGH);
      digitalWrite(startLine[RED2], HIGH);
      digitalWrite(startLine[RED3], HIGH);
      digitalWrite(startLine[GO], LOW);
      startSequence = 2;
    } else if (startSequence == 2 && millis() - startSequenceBegin > 4000) {
      myDFPlayer.play(songCountdown);
      delay(500);
      digitalWrite(startLine[RED1], LOW);
      startSequence = 3;
    } else if (startSequence == 3 && millis() - startSequenceBegin > 5500) {
      digitalWrite(startLine[RED2], LOW);
      startSequence = 4;
    } else if (startSequence == 4 && millis() - startSequenceBegin > 6500) {
      digitalWrite(startLine[RED3], LOW);
      startSequence = 5;
    } else if (startSequence == 5 && millis() - startSequenceBegin > 7500) {
      digitalWrite(startLine[GO], HIGH);
      raceStartTime = millis();
      startSequence = 6;
    } else if (startSequence == 6 && millis() - startSequenceBegin > 8500) {
      resetStartSequence();
    }
  }
}

/**

*/
void resetStartSequence() {
  startSequence = 0;
  
  for (int i = 0; i < 3; i++) {
    lapCount[i]    = 0; // Compteur de tours
    bestLaps[i]    = 0; // Meilleurs temps au tour
    lastLaps[i]    = 0; // Temps cumulé au dernier tour
    totalTime[i]   = 0; // Temps total
  }

  // Réinit de la ligne de départ
  for (int i = 0; i < sizeof(startLine); i++)
    digitalWrite(startLine[i], LOW);
 
}

/**

*/
void wrongStartSequenceWatch() {
  if (wrongStartSequence == 1) {
    Serial.println("!! FAUX DEPART de " + drivers[driversPlayers[wrongStartCar]] + " !!");
    resetStartSequence();
    wrongStartSequence++;
  } else if (wrongStartSequence < 5) {
    digitalWrite(startLine[RED1], HIGH);
    digitalWrite(startLine[RED2], HIGH);
    digitalWrite(startLine[RED3], HIGH);
    delay(500);
    digitalWrite(startLine[RED1], LOW);
    digitalWrite(startLine[RED2], LOW);
    digitalWrite(startLine[RED3], LOW);
    delay(500);
    wrongStartSequence++;
  } else if (wrongStartSequence >= 5) {
    wrongStartSequence = 0;
    wrongStartCar = 99;
  }
}

/**

*/
void finishLineSequenceWatch() {
  if (finishLineSequence < 5) {
    digitalWrite(startLine[GO], HIGH);
    delay(500);
    digitalWrite(startLine[GO], LOW);
    delay(500);
    finishLineSequence++;
  } else if (wrongStartSequence >= 5) {
    digitalWrite(startLine[GO], LOW);
    finishLineSequence = 0;
    wrongStartCar = 99;
  }
}

/**

*/
void computeLapStat(int _carID) {
  if (_carID < 8) {
    lapCount[_carID]++;

    if (lapCount[_carID] - 1 == 0) {
      Serial.println(drivers[driversPlayers[_carID]] + " commence la course");
    } else {
      unsigned long now = millis();
      unsigned long lapTime = now - (lastLaps[_carID] > 0 ? lastLaps[_carID] : raceStartTime);
      lastLaps[_carID]    = now;
      totalTime[_carID]   = now - raceStartTime; // Actualisation du temps total
      Serial.print("Temps perso au tour : ");
      Serial.println(getHumanTime(lapTime));
      
      if (lapTime < bestLaps[_carID] || bestLaps[_carID] == 0) {
        // Meilleur temps au tour
        bestLaps[_carID] = lapTime;
        Serial.println("Meilleur temps perso au tour !! ");
        if (lapTime < raceBestLap || raceBestLap == 0) {
          // Meilleur temps global au tour
          Serial.println("Meilleur temps général au tour");
          raceBestLap = lapTime;
        }
      }

      if (lapCount[_carID] - 1 == raceLaps) {
        addToRanking(_carID);
        Serial.println(drivers[driversPlayers[_carID]] + " a terminé la course !!!!");
        Serial.print("Temps total : ");
        Serial.println(getHumanTime(lastLaps[_carID]));
        showRanking();
        finishLineSequence = 1;
        myDFPlayer.play(songFinishLine);  //Play the last lap jingle
      } else if (lapCount[_carID] - 1 == raceLaps - 1) {
        Serial.println(drivers[driversPlayers[_carID]] + " attaque son dernier tour !");
        myDFPlayer.play(songLastLap);  //Play the last lap jingle
      } else {
        Serial.println(drivers[driversPlayers[_carID]] + " vient de terminer son tour N°" + (lapCount[_carID] - 1));
        myDFPlayer.play(driversVoices[driversPlayers[_carID]][random(0, 3)]);
      }
    }
  }
}

void addToRanking(int _carID) {
  for (int i=0 ; i < maxDrivers ; i++){
    if (!ranking[i]) {
        ranking[i] = _carID;
        Serial.println(ranking[i]);
        Serial.println(driversPlayers[ranking[i]]);
        break;
    }
  }
}

void showRanking(){
  Serial.println("######################################");
  for (int i=0 ; i < maxDrivers ; i++){
    if (ranking[i]) {
        Serial.print("Position " + (String)(i+1) + " : ");
        Serial.println(drivers[driversPlayers[ranking[i]]]);
    }
  }
  Serial.println("######################################");
}

/**
 * Retourne un temps sous la forme MM:SS.msec
 */
String getHumanTime(long unsigned timestamp) {
  int msecs = timestamp / 100;
  int secs = (timestamp / 1000) % 60;
  int mins = (timestamp / 1000) / 60;
  

  return (mins > 0 && mins < 10 ? "0" : "") + (mins > 0 ? (String)mins + ":" : "") + (secs < 10 ? "0" : "") + (String)secs + "." + (String)msecs +"'";
}
/*
   End of carrera arduino infrared car detector
*/
