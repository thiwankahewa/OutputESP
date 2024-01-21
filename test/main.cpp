#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <FirebaseJson.h>
#include <PubSubClient.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#include "time.h"
#include <EEPROM.h>

#define WIFI_SSID "TiPhone"
#define WIFI_PASSWORD "123123123"
#define API_KEY "AIzaSyCGbG5mypHsC36spPFDaZj6yvBI9u_rx-o"
#define DATABASE_URL "https://fyp-web-app-6f954-default-rtdb.asia-southeast1.firebasedatabase.app/"

#define relay1 19
#define relay2 18
#define relay3 5
#define relay4 17
#define relay5 16
#define relay6 4
#define relay7 2
#define relay8 15
#define relay9 0
#define relay10 12

const char *mqtt_server = "172.20.10.8";
const char *mqtt_client_id = "esp-client";
const char *mqtt_subscribe_topic = "topic/to/subscribe";
const char *mqtt_publish_topic = "topic/to/publish";

WiFiClient espClient;
PubSubClient mqtt_client(espClient);

FirebaseData stream;
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

String parentPath = "/switchState";
const int childPathLength = 17;
String childPath[childPathLength] = {"/fan", "/AcidPump", "/BasePump", "/NutrientPump", "/Solenoid1", "/StirringSystem",
                                     "/growLight1", "/growLight2", "/growLight3", "/waterPump", "/iController",
                                     "/fanOnTime", "/fanOffTime", "/lightOffTime", "/lightOnTime", "/waterPumpOffTime", "/waterPumpOnTime"};
String currentPath;
String switchState;

bool dataChanged = false;

bool signupOK = false;

unsigned long previousMillis = 0;
const int interval = 20000;
int timestamp;
const char *ntpServer = "pool.ntp.org";

bool fan, AcidPump, BasePump, NutrientPump, Solenoid1, StirringSystem, growLight1, growLight2, growLight3, waterPump, iController = true;
bool ifan = false, iAcidPump = false, iBasePump = false, iNutrientPump = false, iSolenoid1 = false, iStirringSystem = false, igrowLight1 = false, igrowLight2 = false, igrowLight3 = false, iwaterPump = false;
int fanOnTime, fanOffTime, lightOffTime, lightOnTime, waterPumpOffTime, waterPumpOnTime;

void streamCallback(MultiPathStream stream);
void streamTimeoutCallback(bool timeout);
void setup_wifi();
void callback(char *topic, byte *payload, unsigned int length);
void reconnect();
void setup_databse();
void saveState();
void getInitialState();
void controlLogic();
unsigned long getTime();

void setup()
{
    Serial.begin(115200);
    setup_wifi();
    setup_databse();
    getInitialState();
    configTime(0, 0, ntpServer);

    mqtt_client.setServer(mqtt_server, 1883);
    mqtt_client.setCallback(callback);

    pinMode(relay1, OUTPUT);
    pinMode(relay2, OUTPUT);
    pinMode(relay3, OUTPUT);
    pinMode(relay4, OUTPUT);
    pinMode(relay5, OUTPUT);
    pinMode(relay6, OUTPUT);
    pinMode(relay7, OUTPUT);
    pinMode(relay8, OUTPUT);
    pinMode(relay9, OUTPUT);
    pinMode(relay10, OUTPUT);
}

void loop()
{
    if (WiFi.status() != WL_CONNECTED)
    {
        setup_wifi();
    }
    else
    {
        unsigned long currentMillis = millis();
        timestamp = getTime();
        if (iController)
        {
            /* if (!mqtt_client.connected())
             {
                 reconnect();
             }
             mqtt_client.loop();*/
        }

        if (!(Firebase.ready() && signupOK))
        {
            setup_databse();
            Serial.println("datatbase reconnect!");
        }
        if (dataChanged)
        {
            dataChanged = false;
            saveState();
        }
        controlLogic();
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void streamCallback(MultiPathStream stream)
{
    for (int i = 0; i < childPathLength; i++)
    {
        if (stream.get(childPath[i]))
        {
            currentPath = stream.dataPath;
            switchState = stream.value;
        }
    }
    Serial.println(currentPath + "/" + switchState);
    dataChanged = true;
}

void streamTimeoutCallback(bool timeout)
{
    if (timeout)
        Serial.println("stream timed out, resuming...\n");

    if (!stream.httpConnected())
        Serial.printf("error code: %d, reason: %s\n\n", stream.httpCode(), stream.errorReason().c_str());
}

void setup_wifi()
{
    delay(10);
    Serial.print("Connecting to ");
    Serial.println(WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(1000);
        Serial.print(".");
    }
    Serial.println("");
    Serial.print("WiFi connected - ESP IP address: ");
    Serial.println(WiFi.localIP());
}

void callback(char *topic, byte *payload, unsigned int length)
{

    Serial.print("Message:");
    String message;
    for (unsigned int i = 0; i < length; i++)
    {
        message += (char)payload[i];
    }
    Serial.println(message);
    ifan = (message == "fanOn") ? true : ((message == "fanOff") ? false : ifan);
    iAcidPump = (message == "AcidPumpOn") ? true : ((message == "AcidPumpOff") ? false : iAcidPump);
    iBasePump = (message == "BasePumpOn") ? true : ((message == "BasePumpOff") ? false : iBasePump);
    iNutrientPump = (message == "NutrientPumpOn") ? true : ((message == "NutrientPumpOff") ? false : iNutrientPump);
    iSolenoid1 = (message == "Solenoid1On") ? true : ((message == "Solenoid1Off") ? false : iSolenoid1);
    iStirringSystem = (message == "StirringSystemOn") ? true : ((message == "StirringSystemOff") ? false : iStirringSystem);
    igrowLight1 = (message == "growLight1On") ? true : ((message == "growLight1Off") ? false : igrowLight1);
    igrowLight2 = (message == "growLight2On") ? true : ((message == "growLight2Off") ? false : igrowLight2);
    igrowLight3 = (message == "growLight3On") ? true : ((message == "growLight3Off") ? false : igrowLight3);
    iwaterPump = (message == "waterPumpOn") ? true : ((message == "waterPumpOff") ? false : iwaterPump);
}

void reconnect()
{
    while (!mqtt_client.connected())
    {
        Serial.print("Attempting MQTT connection...");

        if (mqtt_client.connect(mqtt_client_id))
        {
            Serial.println("connected");
            mqtt_client.subscribe(mqtt_subscribe_topic);
        }
        else
        {
            Serial.print("failed, rc=");
            Serial.print(mqtt_client.state());
            Serial.println(" try again in 5 seconds");
            delay(5000);
        }
    }
}

void setup_databse()
{
    config.api_key = API_KEY;
    config.database_url = DATABASE_URL;

    if (Firebase.signUp(&config, &auth, "", ""))
    {
        Serial.println("Database connected");
        signupOK = true;
    }
    else
    {
        Serial.printf("%s\n", config.signer.signupError.message.c_str());
    }
    config.token_status_callback = tokenStatusCallback;
    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);
    if (!Firebase.RTDB.beginMultiPathStream(&stream, parentPath))
        Serial.printf("stream begin error, %s\n\n", stream.errorReason().c_str());

    Firebase.RTDB.setMultiPathStreamCallback(&stream, streamCallback, streamTimeoutCallback);
}

void saveState()
{
    if (currentPath == "/fan")
    {
        fan = (switchState == "true") ? true : false;
    }
    else if (currentPath == "/AcidPump")
    {
        AcidPump = (switchState == "true") ? true : false;
    }
    else if (currentPath == "/BasePump")
    {
        BasePump = (switchState == "true") ? true : false;
    }
    else if (currentPath == "/NutrientPump")
    {
        NutrientPump = (switchState == "true") ? true : false;
    }
    else if (currentPath == "/Solenoid1")
    {
        Solenoid1 = (switchState == "true") ? true : false;
    }
    else if (currentPath == "/StirringSystem")
    {
        StirringSystem = (switchState == "true") ? true : false;
    }
    else if (currentPath == "/growLight1")
    {
        growLight1 = (switchState == "true") ? true : false;
    }
    else if (currentPath == "/growLight2")
    {
        growLight2 = (switchState == "true") ? true : false;
    }
    else if (currentPath == "/growLight3")
    {
        growLight3 = (switchState == "true") ? true : false;
    }
    else if (currentPath == "/waterPump")
    {
        waterPump = (switchState == "true") ? true : false;
    }
    else if (currentPath == "/iController")
    {
        iController = (switchState == "true") ? true : false;
    }
    else if (currentPath == "/fanOnTime")
    {
        fanOnTime = atoi(switchState.c_str());
    }
    else if (currentPath == "/fanOffTime")
    {
        fanOffTime = atoi(switchState.c_str());
    }
    else if (currentPath == "/lightOffTime")
    {
        lightOffTime = atoi(switchState.c_str());
    }
    else if (currentPath == "/lightOnTime")
    {
        lightOnTime = atoi(switchState.c_str());
    }
    else if (currentPath == "/waterPumpOffTime")
    {
        waterPumpOffTime = atoi(switchState.c_str());
    }
    else if (currentPath == "/waterPumpOnTime")
    {
        waterPumpOnTime = atoi(switchState.c_str());
    }
}

void getInitialState()
{
    Firebase.RTDB.getBool(&fbdo, F("/switchState/fan"), &fan);
    Firebase.RTDB.getBool(&fbdo, F("/switchState/AcidPump"), &AcidPump);
    Firebase.RTDB.getBool(&fbdo, F("/switchState/BasePump"), &BasePump);
    Firebase.RTDB.getBool(&fbdo, F("/switchState/NutrientPump"), &NutrientPump);
    Firebase.RTDB.getBool(&fbdo, F("/switchState/Solenoid1"), &Solenoid1);
    Firebase.RTDB.getBool(&fbdo, F("/switchState/StirringSystem"), &StirringSystem);
    Firebase.RTDB.getBool(&fbdo, F("/switchState/growLight1"), &growLight1);
    Firebase.RTDB.getBool(&fbdo, F("/switchState/growLight2"), &growLight2);
    Firebase.RTDB.getBool(&fbdo, F("/switchState/growLight3"), &growLight3);
    Firebase.RTDB.getBool(&fbdo, F("/switchState/waterPump"), &waterPump);
    Firebase.RTDB.getBool(&fbdo, F("/switchState/iController"), &iController);
    Firebase.RTDB.getInt(&fbdo, F("/switchState/fanOnTime"), &fanOnTime);
    Firebase.RTDB.getInt(&fbdo, F("/switchState/fanOffTime"), &fanOffTime);
    Firebase.RTDB.getInt(&fbdo, F("/switchState/lightOffTime"), &lightOffTime);
    Firebase.RTDB.getInt(&fbdo, F("/switchState/lightOnTime"), &lightOnTime);
    Firebase.RTDB.getInt(&fbdo, F("/switchState/waterPumpOffTime"), &waterPumpOffTime);
    Firebase.RTDB.getInt(&fbdo, F("/switchState/waterPumpOnTime"), &waterPumpOnTime);
}

void controlLogic()
{

    digitalWrite(relay7, (fan ? (iController ? (ifan ? HIGH : LOW) : (fan ? HIGH : LOW)) : LOW));
    digitalWrite(relay2, AcidPump ? HIGH : LOW);
    digitalWrite(relay3, BasePump ? HIGH : LOW);
    digitalWrite(relay4, NutrientPump ? HIGH : LOW);
    digitalWrite(relay5, Solenoid1 ? HIGH : LOW);
    digitalWrite(relay6, StirringSystem ? HIGH : LOW);
    digitalWrite(relay1, growLight1 ? HIGH : LOW);
    digitalWrite(relay8, growLight2 ? HIGH : LOW);
    digitalWrite(relay9, growLight3 ? HIGH : LOW);
    digitalWrite(relay10, waterPump ? HIGH : LOW);
}

unsigned long getTime()
{
    time_t now;
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo))
    {
        return (0);
    }
    time(&now);
    return now;
}