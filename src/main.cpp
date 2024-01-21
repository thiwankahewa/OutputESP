#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <FirebaseJson.h>
#include <PubSubClient.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#include "time.h"
#include <EEPROM.h>

#define WIFI_SSID "TiLap"
#define WIFI_PASSWORD "123123123"
#define API_KEY "AIzaSyCGbG5mypHsC36spPFDaZj6yvBI9u_rx-o"
#define DATABASE_URL "https://fyp-web-app-6f954-default-rtdb.asia-southeast1.firebasedatabase.app/"

#define relay1 25  // light1
#define relay2 26  // light2
#define relay3 14  // light3
#define relay4 27  // acid pump
#define relay5 15  // base pump
#define relay6 2   // nutrient pump
#define relay7 4   // stirring
#define relay8 16  // extra
#define relay9 17  // solenoid1
#define relay10 5  // solenoid2
#define relay11 18 // water pump
#define relay12 19 // fan

WiFiClient espClient;

FirebaseData stream;
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

String parentPath = "/switchState";
const int childPathLength = 23;
String childPath[childPathLength] = {"/fan", "/AcidPump", "/BasePump", "/NutrientPump", "/Solenoid1", "/Solenoid2", "/StirringSystem",
                                     "/growLight1", "/growLight2", "/growLight3", "/waterPump", "/iController",
                                     "/fanOnTime", "/fanOffTime", "/lightOffTime", "/lightOnTime", "/waterPumpOffTime", "/waterPumpOnTime",
                                     "/acidPump", "/basePump", "/drainPump", "/nutrientPump", "/waterPump"};
String currentPath;
String switchState;

bool dataChanged = false, autoFlag = false;

bool signupOK = false;

unsigned long previousMillis = 0;
const int interval = 20000;
int timestamp;
const char *ntpServer = "pool.ntp.org";

bool fan, AcidPump, BasePump, NutrientPump, Solenoid1, Solenoid2, StirringSystem, growLight1, growLight2, growLight3, waterPump, iController = true;
int fanOnTime, fanOffTime, lightOffTime, lightOnTime, waterPumpOffTime, waterPumpOnTime;
float acidValue, baseValue, drainValue, nutrientValue, waterValue;

void streamCallback(MultiPathStream stream);
void streamTimeoutCallback(bool timeout);
void setup_wifi();
void reconnect();
void setup_databse();
void saveState();
void getInitialState();
void manualLogic();
void autoLogic();
unsigned long getTime();

void setup()
{
  Serial.begin(115200);
  setup_wifi();
  setup_databse();
  getInitialState();
  configTime(0, 0, ntpServer);

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
  pinMode(relay11, OUTPUT);
  pinMode(relay12, OUTPUT);
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
    if (!iController)
    {
      manualLogic();
    }
    else if (iController && autoFlag)
    {
      autoLogic();
      autoFlag = false;
    }
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
      if (currentPath == "/acidPump")
      {
        acidValue = atoi(switchState.c_str());
        autoFlag = true;
      }
      else if (currentPath == "/basePump")
      {
        baseValue = atoi(switchState.c_str());
      }
      else if (currentPath == "/drainPump")
      {
        drainValue = atoi(switchState.c_str());
      }
      else if (currentPath == "/nutrientPump")
      {
        nutrientValue = atoi(switchState.c_str());
      }
      else if (currentPath == "/waterPump")
      {
        waterValue = atoi(switchState.c_str());
      }
      Serial.println(currentPath + "/" + switchState);
    }
  }

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
  else if (currentPath == "/Solenoid2")
  {
    Solenoid2 = (switchState == "true") ? true : false;
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
  Firebase.RTDB.getBool(&fbdo, F("/switchState/Solenoid2"), &Solenoid2);
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

void manualLogic()
{

  digitalWrite(relay12, fan ? LOW : HIGH);
  digitalWrite(relay4, AcidPump ? LOW : HIGH);
  digitalWrite(relay5, BasePump ? LOW : HIGH);
  digitalWrite(relay6, NutrientPump ? LOW : HIGH);
  digitalWrite(relay9, Solenoid1 ? LOW : HIGH);
  digitalWrite(relay10, Solenoid2 ? LOW : HIGH);
  digitalWrite(relay7, StirringSystem ? LOW : HIGH);
  digitalWrite(relay1, growLight1 ? LOW : HIGH);
  digitalWrite(relay2, growLight2 ? LOW : HIGH);
  digitalWrite(relay3, growLight3 ? LOW : HIGH);
  digitalWrite(relay11, waterPump ? LOW : HIGH);
}

void autoLogic()
{
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