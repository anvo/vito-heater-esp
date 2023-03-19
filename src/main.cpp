/*
 * Copyright (c) 2023 anvo
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */

#include <sstream>
#include <iomanip>
#include <Arduino.h>
#include <VitoWiFi.h>
#include <SoftwareSerial.h>
#include <WiFiManager.h> 

#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>

const char *HOSTNAME = "vito-heater-esp";

const char* SLOW = "1h";
const char* FAST = "5m";

const unsigned long READ_INTERVAL_SLOW = 60*60*1000;
const unsigned long READ_INTERVAL_FAST = 5*60*1000;
const unsigned long SETUP_WAIT = 10*1000;

VitoWiFi_setProtocol(P300);


// Serial is used for VitoWifi so we can not use it for serial output. 
// We remap Serial to port  GPIO15/D8 (TX) and GPIO13/D7 (RX) and attach SoftwareSerial to port 1/3
SoftwareSerial debugConsole(3,1); 

template<typename DP> 
class DPHolder {
  DP dataPoint;

  char* value = new char[10];
  bool set = false;

  public:
  DPHolder(const char* name, const char* group, uint16_t address):
    dataPoint(name, group, address) {
      dataPoint.setCallback([this](const IDatapoint& dp, DPValue dpvalue){
        dpvalue.getString(this->value, 10);
        this->set = true;

        debugConsole.print("DPHolder ");
        debugConsole.print(dp.getName());
        debugConsole.print(": ");
        debugConsole.println(this->value);        
      });
      
    }    
    
    const bool isSet() {
      return this->set;
    }

    const char * getValue() {
      return value;
    }

    const char* getName() {
      return dataPoint.getName();
    }
};



/*
Gerät
    - Reglerkennung (7389) [GWGReglerkennung2010~0x00F9 (Byte)]
    - Software-Index des Gerätes (7567) [Software_Index~0x00FB (String)]
    - Bedienteil Software-Index (91) [Bedienteil_SW_Index~0x7330 (Array)]    
*/

/*
Solar
    - Solar Kollektortemperatur (5272) [Solarkollektortemperatur~0x6564 (SInt)] HIDDEN:("Solar Kollektortemperatur">{'CompanyId': '24', 'Unit': 'Grad C', 'DataType': 'Float', 'Stepping': 0.1, 'LowerBorder': -20.0, 'UpperBorder': 250.0} OR "Solar Kollektortemperatur"<{'CompanyId': '24', 'Unit': 'Grad C', 'DataType': 'Float', 'Stepping': 0.1, 'LowerBorder': -20.0, 'UpperBorder': 250.0} OR "Status Solar Kollektorsensor"="Nicht vorhanden")
    - Solar Speichertemperatur (5276) [SolarSpeichertemperatur~0x6566 (Int)] HIDDEN:("Solar Speichertemperatur">{'CompanyId': '24', 'Unit': 'Grad C', 'DataType': 'Float', 'Stepping': 0.1, 'LowerBorder': 0.0, 'UpperBorder': 127.0} OR "Solar Speichertemperatur"<{'CompanyId': '24', 'Unit': 'Grad C', 'DataType': 'Float', 'Stepping': 0.1, 'LowerBorder': 0.0, 'UpperBorder': 127.0})
    - Solar Betriebsstunden (5277) [SolarStunden~0x6568 (Int)]
    - Solar Wärmemenge (5279) [SolarWaerme~0x6560 (Int4)]
    - Solar Solarpumpe (5274) [SolarPumpe~0x6552 (Byte)]
    - Solar Nachladeunterdrückung (5273) [SolarNachlade~0x6551 (Byte)]
*/
DPHolder<DPTemp>    Solarkollektortemperatur("Solarkollektortemperatur", FAST, 0x6564);
DPHolder<DPTemp>    SolarSpeichertemperatur("SolarSpeichertemperatur", FAST, 0x6566);
DPHolder<DPCountS>  SolarStunden("SolarStunden", SLOW, 0x6568);
DPHolder<DPCount>   SolarWaerme("SolarWaerme", SLOW, 0x6560);
DPHolder<DPStat>    SolarPumpe("SolarPumpe", FAST, 0x6552);
DPHolder<DPStat>    SolarNachlade("SolarNachlade", FAST, 0x6551);

/*
Kessel
    - Brenner-Betriebsstunden (104) [BetriebsstundenBrennerBedienungGWG~0x08A7 (Int)]
    - Brenner (600) [GWG_Flamme~0x55D3 (Byte)]
    - Brennerstarts (111) [BrennerstartsGWG~0x088A (Int4)]
    - Modulationsgrad (4165) [nvoBoilerState_BLR_value~0xA305 (Byte)] HIDDEN:("aktueller Brennertyp"≠"modulierend" AND "aktueller Brennertyp"≠"modulierend" AND "GWG53: Brennertyp"≠"modulierender Brenner")
    - Abgastemperatur (5372) [TiefpassTemperaturwert_AGTS~0x0816 (Int)] HIDDEN:("Status Sensor 15"="Unterbrechung" OR "Status Sensor 15"="nicht vorhanden")
    - Aussentemperatur (5373) [TiefpassTemperaturwert_ATS~0x5525 (SInt)]
    - Aussentemperatur gedämpft (314) [Gemischte_AT~0x5527 (SInt)]
    - Kesseltemperatur (5374) [TiefpassTemperaturwert_KTS~0x0810 (Int)]    
    - Kesselsolltemperatur (2954) [Kesselsoll_eff~0x555A (Byte)]        
    - Interne Pumpe Drehzahl (787) [InternePumpeDrehzahl~0x7660 (Int)] HIDDEN:(30_KennIntUmwPumpe="0 stufig")
    - Interne Pumpe (245) [DigitalAusgang_InternePumpe~0x7660 (Byte)]   
    - Differenztemperatur 1 (8494) [Neptun_Differenztemperatur1~0x88F6 (Byte)]    
    - Zirkulationspumpe (7181) [Zirkulationspumpe~0x6515 (Byte)]    
*/
DPHolder<DPCount>  BetriebsstundenBrennerBedienungGWG("BetriebsstundenBrennerBedienungGWG", SLOW, 0x08A7); //Unit: seconds
DPHolder<DPStat>    GWG_Flamme("GWG_Flamme", FAST, 0x55D3);
DPHolder<DPCount>   BrennerstartsGWG("BrennerstartsGWG", FAST, 0x088A);
DPHolder<DPCountS>  nvoBoilerState_BLR_value("nvoBoilerState_BLR_value", FAST, 0xA305);
DPHolder<DPTemp>    TiefpassTemperaturwert_AGTS("TiefpassTemperaturwert_AGTS", FAST, 0x0816);
DPHolder<DPTemp>    TiefpassTemperaturwert_ATS("TiefpassTemperaturwert_ATS", FAST, 0x5525);
DPHolder<DPTemp>    Gemischte_AT("Gemischte_AT", FAST, 0x5527);
DPHolder<DPTemp>    TiefpassTemperaturwert_KTS("TiefpassTemperaturwert_KTS", FAST, 0x0810);
DPHolder<DPTemp>    Kesselsoll_eff("Kesselsoll_eff", FAST, 0x555A);
DPHolder<DPCountS>  InternePumpeDrehzahl("InternePumpeDrehzahl", FAST, 0x7660);
DPHolder<DPStat>    DigitalAusgang_InternePumpe("DigitalAusgang_InternePumpe", FAST, 0x7660);
DPHolder<DPStat>    Zirkulationspumpe("Zirkulationspumpe", FAST, 0x6515);

/*
Heizkreis
    - Vorlauftemperatur A1M1 (5375) [TiefpassTemperaturwert_KTS_A1~0x0810 (Int)]
    - Vorlauftemperatur Soll A1M1 (6063) [VT_SolltemperaturA1M1~0x2544 (Int)]   
    - Heizkreispumpe A1M1 (729) [HK_PumpenzustandA1M1~0x2906 (Byte)]
    - Heizkreispumpe A1M1 Drehzahl (778) [HKP_A1Drehzahl~0x7663 (Int)] HIDDEN:(E5_KennPumpHzkA1="0 stufig")         
*/
DPHolder<DPTemp>  TiefpassTemperaturwert_KTS_A1("TiefpassTemperaturwert_KTS_A1", FAST, 0x0810);
DPHolder<DPTemp>  VT_SolltemperaturA1M1("VT_SolltemperaturA1M1", FAST, 0x2544);
DPHolder<DPStat>  HK_PumpenzustandA1M1("HK_PumpenzustandA1M1", FAST, 0x2906);
DPHolder<DPCount> HKP_A1Drehzahl("HKP_A1Drehzahl", FAST, 0x7663);

/*
Warmwasser   
    - Warmwasserbereitung (7179) [WW_Status_NR1~0x650A (Byte)]
    - Temperatur Speicher Ladesensor Komfortsensor (5381) [TiefpassTemperaturwertWW1~0x0812 (Int)]
    - Auslauftemperatur (5382) [TiefpassTemperaturwerWW2~0x0814 (Int)] HIDDEN:("Status Sensor 5B"="nicht vorhanden" OR "Status Sensor 5B"="Unterbrechung")
    - Warmwassertemperatur Soll (effektiv) (7177) [WW_SolltemperaturAktuell~0x6500 (Int)]        
    - Gem. Vorlauftemperatur (5380) [TiefpassTemperaturwert_VTS~0x081A (Int)] HIDDEN:("Status Sensor VTS"="Nicht vorhanden" OR "(52) Sensor Hydraulische Weiche"="0 nicht vorhanden""Status Sensor VTS"="Nicht vorhanden")    
    - Speicherladepumpe (5280) [Speicherladepumpe~0x6513 (Byte)]

*/
DPHolder<DPStat>  WW_Status_NR1("WW_Status_NR1", FAST, 0x650A);
DPHolder<DPTemp>  TiefpassTemperaturwertWW1("TiefpassTemperaturwertWW1", FAST, 0x0812);
DPHolder<DPTemp>  TiefpassTemperaturwerWW2("TiefpassTemperaturwerWW2", FAST, 0x0814);
DPHolder<DPTemp>  WW_SolltemperaturAktuell("WW_SolltemperaturAktuell", FAST, 0x6500);
DPHolder<DPTemp>  TiefpassTemperaturwert_VTS("TiefpassTemperaturwert_VTS", FAST, 0x081A);
DPHolder<DPStat>  Speicherladepumpe("Speicherladepumpe", FAST, 0x6513);


ESP8266WebServer server(80);
ESP8266HTTPUpdateServer updateServer;

template<typename T> void printJson(std::stringstream &json, DPHolder<T> &dpHolder) {
  if(dpHolder.isSet()) {
    json << "\"" << dpHolder.getName() << "\": ";
    json << dpHolder.getValue();
    json << "," << std::endl;
  }
}

void handleHttp() {
    std::stringstream json;
    json << "{" << std::endl;

    printJson(json, Solarkollektortemperatur);
    printJson(json, SolarSpeichertemperatur);
    printJson(json, SolarStunden);
    printJson(json, SolarWaerme);
    printJson(json, SolarPumpe);
    printJson(json, SolarNachlade);

    printJson(json, BetriebsstundenBrennerBedienungGWG);
    printJson(json, GWG_Flamme);
    printJson(json, BrennerstartsGWG);
    printJson(json, nvoBoilerState_BLR_value);
    printJson(json, TiefpassTemperaturwert_AGTS);
    printJson(json, TiefpassTemperaturwert_ATS);
    printJson(json, Gemischte_AT);
    printJson(json, TiefpassTemperaturwert_KTS);
    printJson(json, Kesselsoll_eff);
    printJson(json, InternePumpeDrehzahl);
    printJson(json, DigitalAusgang_InternePumpe);
    printJson(json, Zirkulationspumpe);

    printJson(json, TiefpassTemperaturwert_KTS_A1);
    printJson(json, VT_SolltemperaturA1M1);
    printJson(json, HK_PumpenzustandA1M1);
    printJson(json, HKP_A1Drehzahl);

    printJson(json, WW_Status_NR1);
    printJson(json, TiefpassTemperaturwertWW1);
    printJson(json, TiefpassTemperaturwerWW2);
    printJson(json, WW_SolltemperaturAktuell);
    printJson(json, TiefpassTemperaturwert_VTS);
    printJson(json, Speicherladepumpe);
    
    
    json << "\"href\": {" << std::endl;
    json << " \"update\": \"/update\"" << std::endl;
    json << " }" << std::endl;
    json << "}" << std::endl;
    server.send(200, "application/json", json.str().c_str());
}

void setup() {
  // Usually, we could power the IR LED from 3.3V. Issue is that we are connecting it
  // to GPIO15, which means we are pulling it high during boot. This prevents the board
  // from booting.
  // Solution is to power the IR LED from GPIO12, which we only "power up" after he
  // board has been booted. During boot, GPIO15 is low, later is is high.
  pinMode(D6, OUTPUT);
  digitalWrite(D6, HIGH);

  VitoWiFi.setup(&Serial);
  Serial.swap(); // Change pins to  GPIO15 (TX) and GPIO13 (RX) 
  VitoWiFi.setLogger(&debugConsole);
  VitoWiFi.enableLogger();

  VitoWiFi.setGlobalCallback([](const IDatapoint& dp, DPValue value){
    char* buffer = new char[10];
    value.getString(buffer, 10);
    debugConsole.print("UNDEFINED ");
    debugConsole.print(dp.getName());
    debugConsole.print(": ");
    debugConsole.println(buffer);
  });

  debugConsole.begin(9600);

  // Wifi
  debugConsole.println("Starting WiFi");
  
  WiFiManager wifiManager;
  if(!wifiManager.autoConnect(HOSTNAME)) {
    debugConsole.println("Failed to connect to WiFi");
  }
  WiFi.hostname(HOSTNAME);  

  // MDNS
  debugConsole.print("Starting mDNS:");
  debugConsole.println(HOSTNAME);
  if(!MDNS.begin(HOSTNAME)) {
    debugConsole.println("Failed to start mDNS");
  }
  MDNS.addService("http", "tcp", 80);  

  // Webserver
  debugConsole.println("Starting WebServer");
  server.on("/",handleHttp);
  updateServer.setup(&server, "/update");
  server.begin();
  debugConsole.print("http://");
  debugConsole.println(WiFi.localIP());

  debugConsole.println("Waiting before reading values");
  delay(SETUP_WAIT);  
}

unsigned long timeSlow = -READ_INTERVAL_SLOW;
unsigned long timeFast = -READ_INTERVAL_FAST;
void loop() {

  server.handleClient();
  MDNS.update();

  if(millis()- timeSlow > READ_INTERVAL_SLOW) {
    timeSlow = millis();
    debugConsole.println("Reading SLOW");
    VitoWiFi.readGroup(SLOW);
  }
  if(millis()- timeFast > READ_INTERVAL_FAST) {
    timeFast = millis();
    debugConsole.println("Reading FAST");
    VitoWiFi.readGroup(FAST);
  }  
  VitoWiFi.loop();
}
