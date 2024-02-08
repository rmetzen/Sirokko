/* Automatisierung einer Sirokko OETF10N

V1.7 Ein Sensor DS18B20, ein Sensor PT1000
    zusätzlich Rotary Encoder zur Sollwertveränderung sowie zum Starten/Stoppen

    In dieser Version wird die Zulufttemperatur über einen PT1000 gemessen, da der
    zulässige Messbereich des DS18B20 von der Heizung überschritten wird. Der PT1000 ist über eine
    Spannungsteilerschaltung (1k Widerstand) verschaltet. Die Messgenauigkeit ist zwar sehr gering,
    jedoch für den Zweck völlig ausreichend. Aufgrund der miserablen Messgenauigkeit wird der Mittelwert
    von 5 Messungen ermittelt. 
    
    Diese Version enthält einen Zweipunktregler mit Hysterese, über den Rotary wird
    zwischen den Betriebsarten "Aus" und "Automatik" gewählt

    Die Version enthält nun die Überwachung, ob der Brenner tatsächlich gestartet ist.
    Dies erfolgt über eine Überwachung der Zulufttemperaturzunahme.
    
    Es erfolgt keine Überwachung, ob das Öl während des Betriebes ausgegangen ist, also ob
    während des regulären Heizbetriebens pötzlich die Zulufttemperatur einbricht.
    Das würde auf einen leeren Tank hindeuten.

    Schaltausgänge: 
    Gebläse ein/aus
    Trafo (Glühkerze) ein/aus
    Magnetventil Ölzufuhr auf/zu (Stromlos geschlossen)

    Analoge Eingänge: (genaugenommen sind es OneWire Buseingänge)
    Raumtemperatur   (entspricht Ansaugtemperatur bzw. Ablufttemperatur über I2C Onwire Bus)
    Zulufttemperatur (entspricht Ausblastemperatur über Spannungsteiler an Analogeingang)
    
    Digitale Eingänge:
    Raumtemperaturwahl (über Drehgeber)

*/

#include "OneWire.h"              // OneWire Bus 
#include "DallasTemperature.h"    // DS18B20 Temperaturfühler für Zu- und Ablufttemperatur
#include "heltec.h"
#include "AiEsp32RotaryEncoder.h" // Drehgeber 

#define ROTARY_ENCODER_A_PIN 32
#define ROTARY_ENCODER_B_PIN 33
#define ROTARY_ENCODER_BUTTON_PIN 25
#define ROTARY_ENCODER_STEPS 4

AiEsp32RotaryEncoder rotaryEncoder = AiEsp32RotaryEncoder(ROTARY_ENCODER_A_PIN, ROTARY_ENCODER_B_PIN, ROTARY_ENCODER_BUTTON_PIN, -1, ROTARY_ENCODER_STEPS);


OneWire oneWire0(14); // Bus-Instanz, um die Abluft/Raumtemperatur zu messen
DallasTemperature TAbluft(&oneWire0);


// Allgemeine Programmvariablen
int RaumSoll;              // Raum-Solltemperatur
int Zulufttemperatur;      // Lufttemperatur am Ausblas der Sirokko
float Ablufttemperatur;    // Lufttemperatur am Ansaug der Sirokko, entspricht Raumtemperatur
float ZulufttemperaturUeberwachung;    // Lufttemperatur am Ausblas der Sirokko, bei Ende des Glühvorganges
int ZulufttemperaturAnstieg = 5;       // um diesen Wert muss die Lufttemperatur am Ausblas der Sirokko steigen, sonst liegt eine Brennerstörung vor  ++++++++++++++++++++


// Hier die Pinbelegung, ggf. anpassen
const byte Geblaese_PIN = 18;         // Pin für Gebläse ein/aus
const byte OelMV_PIN = 23;            // Pin für Öl-Magnetventil auf/zu
const byte Gluehkerze_PIN = 19;       // Pin für Glühkerze ein/aus
const byte PT1000_PIN = 36;           // ADC Pin für PT 1000 Zulufttemperatur



// Timing-Variablen
unsigned long TempMessenStartMillis;                    // Timer-Startzeit 
const unsigned long IntervallTempMessen = 5000;        // Abfrageintervall Temperaturen jede Sekunde

unsigned long BrennerStartMillis;                       // Timer-Startzeit 
const unsigned long DauerBrennerstart = 60000;          // 60 sek Maximale Dauer, um einen Brennerstart (Glühen) durchzuführen          ++++++++++++++++++++++
  
unsigned long NachlaufStartMillis;                      // Timer-Startzeit 
const unsigned long DauerNachlauf = 240000;             // 240 sek Gebläsenachlauf zum Abkühlen                                         ++++++++++++++++++++++

unsigned long BrennerueberwachungMillis;                // Timer-Startzeit zur Überwachung, ob der Brenner gestartet ist
const unsigned long DauerTemperaturueberwachung = 30000;              // 30 sek nach Brennerstart wir der Zulufttemperaturanstieg geprüft             ++++++++++++++++++++++


int Hysterese=2;            // Hysterese zur Temperaturregelung

boolean Heizen = false;     // Heizbetrieb ist aktiv bei True
boolean Nachlauf = false;   // Nachlüftzeit ist aktiv bei True
boolean Geblaese = false;   // Geblaese ist aus
boolean Gluehkerze = false; // Gluehkerze ist aus
boolean OelMV = false;      // Öl-Magnetventil ist zu
boolean Automatik = false;  // Automatikbetrieb bei Start ausgeschaltet
boolean BrennerFehler = false;     // Fehler beim Brennerstart
boolean BrennerUeberwachung = false;     // True während der Brennerüberwachung




/* *************************************************
 * ***    Interrupt für Drehgeber einrichten     ***
   *************************************************
*/

void IRAM_ATTR readEncoderISR()
{
    rotaryEncoder.readEncoder_ISR();
}

/* *************************************************
 * ***    ab hier folgt das Setup                ***
   *************************************************
*/

void setup() {

  TAbluft.begin();                  // Temperaturmessung starten
  
 
  Heltec.begin(true /*DisplayEnable Enable*/, false /*LoRa Disable*/, true /*Serial Enable*/);
  //Heltec.display->flipScreenVertically();      // Display einstellen
  Heltec.display->setFont(ArialMT_Plain_16);   // Schriftart und Größe 10,16,24
 
    // clear the display
    Heltec.display->clear();
    Heltec.display->setTextAlignment(TEXT_ALIGN_LEFT);
    Heltec.display->drawString(0, 0, "Ruedis ");
    Heltec.display->drawString(0, 22, "Sirokko V1.7");
    Heltec.display->setFont(ArialMT_Plain_10);
    Heltec.display->drawString(0, 45, "los gehts...");
    Heltec.display->display();                           // Das vorstehende auf dem Display ausgeben
 
    delay(1000);                       // 1 Sekunde warten
    Heltec.display->clear();

Serial.begin(9600);                 //Serielle Schnittstelle für Debug starten

    rotaryEncoder.begin();
    rotaryEncoder.setup(readEncoderISR);
    rotaryEncoder.setBoundaries(10, 30, false); //Minimale und maximale einstellbare Raumtemperatur
    rotaryEncoder.setAcceleration(0);           // keine beschleunigte Werteänderung bei schnellem Drehen
    rotaryEncoder.setEncoderValue(18);          //Grundwert einstellen auf 18 Grad Celsius

pinMode(Geblaese_PIN,OUTPUT);        // Geblaese Relay Pin als Output festlegen
digitalWrite(Geblaese_PIN,HIGH);     // Geblaese ausschalten bei Neustart
pinMode(Gluehkerze_PIN,OUTPUT);      // Gluehkerze Relay Pin als Output festlegen
digitalWrite(Gluehkerze_PIN,HIGH);   // Gluehkerze ausschalten bei Neustart
pinMode(OelMV_PIN,OUTPUT);           // Öl-Magnetventil Relay Pin als Output festlegen
digitalWrite(OelMV_PIN,HIGH);        // Öl-Magnetventil schliessen bei Neustart

RaumSoll = (rotaryEncoder.readEncoder());        
TempMessen();       //DS18B20 Ablufttemperatur und PT1000 Zulufttemperatur messen
}


/* *************************************************
 * ***    ab hier folgt das Hauptprogramm        ***
   *************************************************
*/

void loop() {

RaumSoll = (rotaryEncoder.readEncoder());

if (rotaryEncoder.isEncoderButtonClicked())
    {
      DrehgeberButton();
    }


// periodisch die Temperaturfühler abfragen

if (millis () - TempMessenStartMillis >= IntervallTempMessen)    //Timerzeit überschritten ? dann ggf. aktuelle Temperaturen messen
    {
      TempMessen();       //DS18B20/PT1000 Zu- und Ablufttemperatur messen
      TempMessenStartMillis = millis();  //Timer neu starten
    }

    
// ab hier die automatische Temperaturregelung

if ((Ablufttemperatur > (RaumSoll)) && (Heizen == true) && (Automatik == true)) {
  Serial.println("es ist zu warm, ausschalten..."); 
  Heizen=false;
  GluehkerzeAus ();     // falls während des Glühvorganges abgeschaltet wird
}
if ((Ablufttemperatur < (RaumSoll-Hysterese)) && (Heizen == false) && (Automatik == true) && (BrennerFehler ==false)) {
  Serial.println("es ist zu kalt, einschalten..."); 
  Heizen=true;
  BrennerStartMillis = millis();
}


// ab hier die Schaltvorgänge

if (Heizen == false) {   // kein Heizbetrieb

if (OelMV == true){
    OelAus ();            // schliesst die Ölzufuhr
    Serial.println("Ölzufuhr schliessen...");
    NachlaufStartMillis = millis ();
    Nachlauf = true;   
}
}

if ((millis() - NachlaufStartMillis > DauerNachlauf) &&  (Nachlauf == true))    //Timerzeit Spülen abgelaufen ? Gebläse abschalten
{
  if (Geblaese == true) { //nur Schalten, wenn es an ist
  Serial.println("Gebläse ausschalten..."); 
  GeblaeseAus ();
  Nachlauf = false;
  }
}
 
if ((Heizen == true) && (BrennerFehler == false))      // Regulärer Heizbetrieb
{
    GeblaeseEin ();       // schaltet das Gebläse ein
    OelEin ();            //öffnet die Ölzufuhr


 if ((millis () - BrennerStartMillis <= DauerBrennerstart) && (Heizen == true) && (BrennerFehler == false))  //Timerzeit zum Brennerstart läuft ? Glühkerze ein
    {
   if (Gluehkerze == false) { //nur Schalten, wenn es aus ist
    GluehkerzeEin ();     // schaltet die Glühkerze ein
    Heltec.display->setFont(ArialMT_Plain_10);
    Heltec.display->drawString(119, 40, "X");
    Serial.println("Brennerstart eingeleitet...");
    }
    }
 
 
 if ((millis () - BrennerStartMillis > DauerBrennerstart) && (Heizen == true)) 
    { 
    if (Gluehkerze == true) { //nur Schalten, wenn es an ist
    GluehkerzeAus ();     // schaltet die Glühkerze aus
    Serial.println("Glühvorgang beendet...");
    ZulufttemperaturUeberwachung = Zulufttemperatur ;  // Zulufttemperatur bei Ende des Glühvorganges merken, um Brennerstart zu überwachen
    BrennerueberwachungMillis = millis (); // Zeitpunkt Glühende merken um Brennerstart zu überwachen
    BrennerUeberwachung = true; // Brennerüberwachung läuft
    }
    }    

 if ((millis () - BrennerueberwachungMillis > DauerTemperaturueberwachung) && (BrennerUeberwachung == true)) 
 {
 if (Zulufttemperatur < (ZulufttemperaturUeberwachung + ZulufttemperaturAnstieg)) // Überwachung Temperaturanstieg nach Brennerstart
    {
      BrennerFehler = true;   // Falls kein ausreichender Temperaturanstieg, liegt ein Brennerfehler vor
      BrennerUeberwachung = false;   // Zeitraum der Brennerüberwachung ist beendet, Brennerstart war NICHT erfolgreich
    }
  else {
    BrennerUeberwachung = false;     // Zeitraum der Brennerüberwachung ist beendet, Brennerstart war erfolgreich
    Serial.println("Brennerstart erfolgreich ...");
  }
 }

    

 if (BrennerFehler == true)
    { 
    Heizen = false;
    GeblaeseAus ();
    OelAus ();
    Serial.println("Brennerstörung während des Startes !!!");
    }
}

DisplayAktualisieren(); // Abschliessend die Displayangaben aktualisieren

}   
// **** Ende Hauptprogrammm *******************************************************


/* *************************************************
 * ***    ab hier folgen die Unterprogramme      ***
   *************************************************
*/
 
//*********************************************************************************
// Temperaturen messen
//*********************************************************************************

void TempMessen() {
  Serial.println();

Zulufttemperatur = 0;
for(int i = 0; i < 20; ++i) // Mittelwert aus 20 Messungen bilden
{
  Zulufttemperatur += ((analogRead(PT1000_PIN)-1830)/2.92);
  delay(1);
}
Zulufttemperatur = (Zulufttemperatur/20);

  Serial.print("Zulufttemperatur: ");
  Serial.println(Zulufttemperatur);
  

  TAbluft.requestTemperatures();
  Ablufttemperatur = TAbluft.getTempCByIndex(0);
  Serial.print("Ablufttemperatur: ");
  Serial.println(Ablufttemperatur);

  Serial.print("RaumSolltemperatur: ");
  Serial.println(RaumSoll);

}


//*********************************************************************************
// DrehgeberButten auswerten
//*********************************************************************************

void DrehgeberButton() {
Serial.println("Drehknopf wurde gedrückt...");

if (Automatik == false)     // kein Automatikbetrieb
{
    Automatik = true;       // Automatikbetrieb einschalten
    BrennerStartMillis = millis();              // Problemstelle ?***********************
    Serial.println("Automatikbetrieb ... ");
    BrennerFehler = false; // eventuelle Brennerstörung rücksetzen

    }

else if (Automatik==true)
{
    Automatik = false;       // Automatikbetrieb ausschalten
    Heizen = false;         
    GluehkerzeAus ();        // falls während des Glühvorganges abgeschaltet wird
    Serial.println("Automatik AUS ... ");
}

}



//*********************************************************************************
// Die Displayangaben aktualisieren
//*********************************************************************************

void DisplayAktualisieren() {

Heltec.display->clear();
if (Automatik == false)     // Automatik ausgeschaltet
{
  Heltec.display->setFont(ArialMT_Plain_24);
  Heltec.display->drawString(0, 0, "AUS");
}
if (Automatik == true)     // Automatik eingeschaltet
{
  Heltec.display->setFont(ArialMT_Plain_24);
  Heltec.display->drawString(0, 0, "AUTO");
}   

if (Gluehkerze == true)     // Glühvorgang eingeschaltet
{
  Heltec.display->setFont(ArialMT_Plain_16);
  Heltec.display->drawString(70, 5, "Glühen");
}   


if (BrennerFehler == true)     // Brennerstörung bei Start
{
  Heltec.display->setFont(ArialMT_Plain_16);
  Heltec.display->drawString(70, 5, "Störung");
}   


if ((Geblaese == true) && (OelMV == true) && (Gluehkerze == false) && (BrennerFehler == false))     // Glühvorgang beendet, regluärer Heizbetrieb
{
  Heltec.display->setFont(ArialMT_Plain_16);
  Heltec.display->drawString(70, 5, "Heizen");
}  

if ((Geblaese == true) && (OelMV == false) && (BrennerFehler ==false))     // Spülvorgang 
{
  Heltec.display->setFont(ArialMT_Plain_16);
  Heltec.display->drawString(70, 5, "Spülen");
} 

  Heltec.display->setFont(ArialMT_Plain_16);
  Heltec.display->drawString(0, 25, "Sollwert: ");
  Heltec.display->drawString(70, 25, String(RaumSoll));
  Heltec.display->drawString(90, 25, "°C");
  
  Heltec.display->setFont(ArialMT_Plain_10);
  Heltec.display->drawString(0, 42, "Gebläse ");
if (Geblaese == true)     // Gebläse in Betrieb
{
  Heltec.display->drawString(42, 42, "X");
}   

     
  Heltec.display->drawString(54, 42, "Öl ");
if (OelMV == true)     // Öl-Magnetventil ist offen
{
  Heltec.display->drawString(68, 42, "X");
}   

  
  Heltec.display->drawString(82, 42, "Glühen ");
if (Gluehkerze == true)     // Glüchkerze ist ein 
{
  Heltec.display->drawString(118, 42, "X");
}   

        
  Heltec.display->setFont(ArialMT_Plain_10);
  Heltec.display->drawString(0, 54, "Raum ");
  Heltec.display->drawString(70, 54, "Zuluft ");
  Heltec.display->setFont(ArialMT_Plain_10);
  Heltec.display->drawString(30, 54, String(Ablufttemperatur)); // Ablufttemperatur entspricht der Raumtemperatur
  Heltec.display->drawString(100, 54, String(Zulufttemperatur));


    
Heltec.display->display();                           // Das vorstehende auf dem Display ausgeben
}


//*********************************************************************************
// Schaltvorgänge
//*********************************************************************************

void GeblaeseEin (){
digitalWrite(Geblaese_PIN, LOW);       // schaltet das Gebläse ein
Geblaese = true;
}

void GeblaeseAus (){
digitalWrite(Geblaese_PIN, HIGH);       // schaltet das Gebläse aus
Geblaese = false;
}


void GluehkerzeEin (){
digitalWrite(Gluehkerze_PIN, LOW);     // schaltet die Glühkerze ein
Gluehkerze = true;
}

void GluehkerzeAus (){
digitalWrite(Gluehkerze_PIN, HIGH);     // schaltet die Glühkerze aus
Gluehkerze = false;
}

void OelEin (){
digitalWrite(OelMV_PIN, LOW);     // Öl-Magnetventil auf
OelMV = true;
}

void OelAus (){
digitalWrite(OelMV_PIN, HIGH);     // Öl Magnetventil zu
OelMV = false;
}

      
