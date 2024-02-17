// Send Blynk user messages to the hardware serial port…
#define BLYNK_PRINT Serial

/* Fill in information from Blynk Device Info here */
#define BLYNK_TEMPLATE_ID "TMPL6PY34V62K"
#define BLYNK_TEMPLATE_NAME "Prepaid Electric Meter"
#define BLYNK_AUTH_TOKEN "sE1Cs8phdO0_jWcmPrkppTO51yRFmFgQ"

// Hardware Serial on Mega
#define EspSerial Serial1

// Your ESP8266 baud rate:
#define ESP8266_BAUD 9600

#include <ESP8266_Lib.h>
#include <BlynkSimpleShieldEsp8266.h>
#include "ACS712.h"
#include <EEPROM.h>

// Your WiFi credentials.
char ssid[] = "PLDT_Home_412F7"; // Your WiFi SSID
char pass[] = "Sarcia24151219";  // Your WiFi password

// Pin Definitions
const int relayPin = 8;
const int sensorPin = A0;

// Constants
const int eepromAddress = 0;
const int mVperAmp = 66;                     // ACS712 sensitivity for 30A variant
const float voltage = 220;                   // Voltage assumption in Volts
const float electricityRate = 11.2584;       // Electricity rate in pesos per Wh
const float initialCredits = 100.0;          // Initial credits in pesos
const unsigned long samplingInterval = 1000; // Sampling interval in milliseconds

// Global Variables
ACS712 ACS(sensorPin, 5.0, 1023, mVperAmp);
float remainingCredits = initialCredits;
float totalEnergyConsumed = 0.0;
float remainingkWh = remainingCredits / electricityRate;
unsigned long lastSamplingTime = 0;

BlynkTimer timer;

// Tell the Blynk library to use the  Hardware Serial port 1 for WiFi..
ESP8266 wifi(&EspSerial);

void setup()
{
    // Debug console
    Serial.begin(9600);
    // Set ESP8266 baud rate
    EspSerial.begin(ESP8266_BAUD);

    while (!Serial)
        ;
    Serial.println("ACS712 Current Sensor");

    ACS.autoMidPoint(); // calibrate
    pinMode(relayPin, OUTPUT);
    updateRelayState();
    EEPROM.get(eepromAddress, remainingCredits);
    if (isnan(remainingCredits))
    {
        remainingCredits = 0.0; // Set to 0 if no data saved
    }
    EEPROM.get(eepromAddress + sizeof(float), totalEnergyConsumed);
    if (isnan(totalEnergyConsumed))
    {
        totalEnergyConsumed = 0.0; // Set to 0 if no data saved
    }

    Blynk.begin(BLYNK_AUTH_TOKEN, wifi, ssid, pass);

    timer.setInterval(1000L, sendEnergyDataToBlynk);

    // A small delay for system to stabilize
    delay(1000);
}

void updateRelayState()
{
    digitalWrite(relayPin, remainingCredits > 0.0 ? HIGH : LOW);
}

void sendEnergyDataToBlynk()
{
    unsigned long currentTime = millis();
    if (currentTime - lastSamplingTime >= samplingInterval)
    {
        lastSamplingTime = currentTime;

        float mA = ACS.mA_AC(60); // get current reading from sensor. Specify 60Hz
        float watts = calculatePower(mA);
        float energy = (watts / 1000) * (samplingInterval / 3600000.0); // Convert ms to hours

        totalEnergyConsumed += energy;
        // totalEnergyConsumed = 0; // for reset
        remainingCredits -= energy * electricityRate;
        // remainingCredits = 2;
        remainingkWh = remainingCredits / electricityRate;

        if (remainingCredits <= 0.0)
        {
            remainingCredits = 0.0;
            updateRelayState();
        }
        if (remainingkWh <= 0.0)
        {
            remainingkWh = 0.0;
        }

        // Save data to EEPROM
        EEPROM.put(eepromAddress, remainingCredits);
        EEPROM.put(eepromAddress + sizeof(float), totalEnergyConsumed);
    }

    // Serial.println("totalEnergyConsumed: " + String(totalEnergyConsumed, 4));
    // Serial.println("remainingCredits: " + String(remainingCredits));
    // Serial.println("remainingkWh: " + String(remainingkWh));
    Serial.println(totalEnergyConsumed, 4);

    Blynk.virtualWrite(V1, totalEnergyConsumed);
    Blynk.virtualWrite(V2, remainingCredits);
    Blynk.virtualWrite(V3, remainingkWh);
}

float calculatePower(float mA)
{
    return (mA / 1000.0) * voltage;
}

BLYNK_WRITE(V4)
{
    int pinValue = param.asInt(); // assigning incoming value from pin V4 to a variable
    remainingCredits += pinValue;
    updateRelayState();
    Serial.println("Reload Success! Your new balance is " + String(remainingCredits));
}

void loop()
{
    Blynk.run();
    timer.run(); // Initiates BlynkTimer
}
