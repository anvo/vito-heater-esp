# vito-header-esp
PlatformIO/Arduino project to read out values from a Viessmann Vitocrossal 300 (Vitotronic 200 KW6B) using an ESP8266 and a custom IR reading head.

**References**

* https://github.com/openv/openv
* https://github.com/bertmelis/VitoWifi

## Hardware

* ESP8266 (NodeMCU)
* 1xSFH309FA IR
* 1x10K resistor
* 1xL-7104SF4BT KB (880nm)
* 1x220 resistor
* 3d printed reading head (https://www.printables.com/model/428029)

Schematic see: https://github.com/openv/openv/wiki/Bauanleitung-ESP8266

Q1:
Green - Short
Black - Long

D1:
Blue - Short
Red - Long

## Software

### Setup

1. Open project using [PlatformIO](https://platformio.org)
1. Make sure to have the following libraries installed
   1. WifiManager
   2. VitoWiFi
1. Adjust constants in `main.cpp` according to your needs
   * `HOSTNAME` is the name of the device within your network
1. Upload everything to your device

### First start

Upon first start, the device will create a new access point named after the contents of `HOSTNAME`. Connect to this WiFi network and open the IP `192.168.4.1` in your browser. A web page allows to configure the connection to any available network.

Once the device is connected to a network, it will start publishing the values via JSON using the embedded webserver.
Connect your browser to `http://<IP of device>` to receive the following output:

```
{
"Solarkollektortemperatur": 12.1,
"SolarSpeichertemperatur": 22.3,
"SolarStunden": 10495,
"SolarWaerme": 24520,
"SolarPumpe": false,
"SolarNachlade": true,
"BetriebsstundenBrennerBedienungGWG": 65215276,
"GWG_Flamme": true,
"BrennerstartsGWG": 127794,
"nvoBoilerState_BLR_value": 296,
"TiefpassTemperaturwert_AGTS": 27.6,
"TiefpassTemperaturwert_ATS": 11.7,
"Gemischte_AT": 11.4,
"TiefpassTemperaturwert_KTS": 44.3,
"Kesselsoll_eff": 34.8,
"InternePumpeDrehzahl": 8961,
"DigitalAusgang_InternePumpe": true,
"Zirkulationspumpe": true,
"TiefpassTemperaturwert_KTS_A1": 44.3,
"VT_SolltemperaturA1M1": 41.9,
"HK_PumpenzustandA1M1": true,
"HKP_A1Drehzahl": 8961,
"WW_Status_NR1": false,
"TiefpassTemperaturwertWW1": 46.0,
"TiefpassTemperaturwerWW2": 20.0,
"WW_SolltemperaturAktuell": 46.0,
"TiefpassTemperaturwert_VTS": 20.0,
"Speicherladepumpe": false,
"href": {
 "update": "/update"
 }
}
```

### OTA Updates
The device supports OTA updates via the `ESP8266HTTPUpdateServer`. To upload a new firmware, visit `http://<IP of device>/update`.