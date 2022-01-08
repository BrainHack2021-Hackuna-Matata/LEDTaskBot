#include <ArduinoJson.h>
#include <CTBot.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <FastLED.h>
#include <NTPClient.h>
#include <WiFiClientSecure.h>
#include <WiFiUDP.h>

#define MAX_LENGTH 30

#define LED_PIN 5
#define NUM_LEDS 60
#define MAX_NUM_TASKS 30

CRGB leds[NUM_LEDS];

DynamicJsonDocument doc(4096);
StaticJsonDocument<200> filter;

byte LOW_PRIO = 0;
byte MED_PRIO = 1;
byte HIGH_PRIO = 2;

const char *TELE_BOT_TOKEN = "5097903121:AAF2d-VcKmdLbL9baufQD9cecGXo-g5aHbo";
const char *WIFI_SSID = "Xiaomi_CAA7";
const char *WIFI_PWD = "notgonnatellu";

int USER = 0;

/*
Time pooling parameters
*/
const long utcOffsetInSeconds = 8 * 60 * 60; // UTC+8 offset in seconds
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);

/*
Telegram bot parameters
*/
CTBot myBot;
const unsigned long BOT_MIN_REFRESH_DELAY = 50;
unsigned long botLastRefreshed;

int aHour[10];
int aMin[10];
byte aTypeL[10];
byte aLength = 0;

/*
Task list parameters
*/
struct Task
{
  String desc;
  byte prio;
};

struct times {
  String modCode;
  String classNo;
  String startTime;
  String day;
};

struct mod {
  String modCode;
  String classNo;
};

mod modList[10];
times timetable[100];

int length = 0;
unsigned int numTimes = 0;

struct Task tasks[MAX_LENGTH];
unsigned int currLength = 0;
const String PRIORITY_STRING_MAP[3] = {"Low", "Med", "High"};


// CHANGE THIS
const unsigned long MIN_DELAY_BEFORE_ALERT = 60000*2; // 2 minutes
unsigned long lastAlertTime = millis();

const String DAYS_OF_WEEK[7] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

void setup()
{
  /*
  Set up WiFi
  */
  Serial.begin(115200);

  //set up ledstrip
  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setMaxPowerInVoltsAndMilliamps(3, 120);
  FastLED.clear();
  FastLED.show();
  Serial.println("LED Setup");
  alertLight(0);

  // blocks until connects
  myBot.wifiConnect(WIFI_SSID, WIFI_PWD);

    /*
  Setup Telegram bot
  */
  myBot.setTelegramToken(TELE_BOT_TOKEN);

  if (myBot.testConnection())
  {
    Serial.print("Connected to IP address: ");
    Serial.println(WiFi.localIP());
  }
  else
  {
    Serial.println("Not connected!");
  }

  /*
  Setup clock
  */
  timeClient.begin();
  // get current time
  timeClient.update();
  botLastRefreshed = millis();

  /*
  Initialise built in LED
  */
  pinMode(LED_BUILTIN, OUTPUT);   // built in LED
  digitalWrite(LED_BUILTIN, LOW); // active LOW


}

void loop()
{
  /*
  Receive updates from bot
  */
  TBMessage msg;
  if (millis() - botLastRefreshed > BOT_MIN_REFRESH_DELAY)
  {

    if (myBot.getNewMessage(msg))
    {
      processNewMessage(msg);
    }

    botLastRefreshed = millis();
  }

  taskLight(tasks, currLength);
  delay(100);
  //alertNextClass();
  //delay(100);

  int hours[2] = {14};
  int mins[2] = {32};
  byte typel[2] = {2}; //exercise is 1, water is 2
  

  alertIfWithinFiveMinutes(hours, mins, 1, typel);
  delay(100);
}



String getTimetable(String link){
  HTTPClient http;
  WiFiClientSecure wifiClient;
  wifiClient.setInsecure();   // don't do this! but who cares?
  String payload = "";
  http.begin(wifiClient, link.c_str());
  int httpResponseCode = http.GET();
  if(httpResponseCode > 0){
    payload = http.getString();
  }
  http.end();
  return payload;
}

void addMod(int id, mod modList[], int* length){
  TBMessage m;
  mod currmod;
  times time;

  filter["semesterData"][0]["timetable"][0] = true;
  filter["semesterData"][0]["semester"] = true;
  myBot.sendMessage(id, "Enter a Module Code");
  //delay(BOT_MIN_REFRESH_DELAY);
  while(myBot.getNewMessage(m) != CTBotMessageText){
    delay(BOT_MIN_REFRESH_DELAY);
  }
  currmod.modCode = String(m.text);
  //delay(BOT_MIN_REFRESH_DELAY);
  myBot.sendMessage(id, "Enter the class code for the module " + currmod.modCode);
  //delay(BOT_MIN_REFRESH_DELAY);
  while(myBot.getNewMessage(m) != CTBotMessageText){
    delay(BOT_MIN_REFRESH_DELAY);
  }
  currmod.classNo = String(m.text);
  myBot.sendMessage(id, "The module " + currmod.modCode + " with class number " + currmod.classNo + " has been added.");
  modList[*length] = currmod;
  *length += 1;
  String ddd = "Your lessons for this module are on: ";
  String json = getTimetable("https://api.nusmods.com/v2/2021-2022/modules/" + currmod.modCode +".json");
  deserializeJson(doc, json, DeserializationOption::Filter(filter));
  int semester = 0;
  Serial.println(String(doc["semesterData"][0]["semester"].as<unsigned int>()));
  if (doc["semesterData"][0]["semester"].as<String>().toInt() == 1)
  {
    semester = 1;
  }
  for(int i = 0; i < doc["semesterData"][semester]["timetable"].as<JsonArray>().size(); i++){
    if(currmod.classNo == doc["semesterData"][semester]["timetable"][i]["classNo"].as<String>()){
      timetable[numTimes].modCode = currmod.modCode;
      timetable[numTimes].classNo = currmod.classNo;
      timetable[numTimes].startTime = doc["semesterData"][semester]["timetable"][i]["startTime"].as<String>();
      timetable[numTimes].day = doc["semesterData"][semester]["timetable"][i]["day"].as<String>();
      ddd += "\n\t" + currmod.modCode + " " + currmod.classNo + " " + String(timetable[numTimes].startTime) + " " + String(timetable[numTimes].day);
      numTimes++;
    }
  }

  myBot.sendMessage(id, ddd);
}

void listMods(int id, mod modList[]){
  String ddd = "";
  if(length == 0){
    myBot.sendMessage(id, "There are no mods in your timetable.");
  }
  else{
    ddd += "Here are your mods:";    
    for(int i = 0; i < length; i++){
      ddd += "\n" + String(i + 1) + ". \n\tModule Code: " + modList[i].modCode + "\n\tClass Number: " + modList[i].classNo;
    }
    myBot.sendMessage(id, ddd);
  }
}

void listTimetable(int id, times timetable[]){
  String ddd = "";
  if(numTimes == 0){
    myBot.sendMessage(id, "There are no mods in your timetable.");
  }
  else{
    ddd += "Here are your mods:";
    for(int i = 0; i < numTimes; i++){
      ddd += "\n" + String(i + 1) + ". \n\tModule Code: " + timetable[i].modCode + "\n\tClass Number: " + timetable[i].classNo + "\n\tDay: " + timetable[i].day + "\n\tStart Time: " + timetable[i].startTime;
    }
    myBot.sendMessage(id, ddd);
  }
}

void processNewMessage(TBMessage msg)
{
  int chatId = msg.sender.id;
  USER = msg.sender.id;
  String fromUserName = msg.sender.username;
  String text = msg.text;

  Serial.print("Message from: ");
  Serial.println(fromUserName);
  Serial.println(text);

  if (text == "/list")
  {
    String taskStr = "Here are your tasks:\n" + getTasksString();
    myBot.sendMessage(chatId, taskStr);
  }
  else if (text == "/newtask")
  {
    processNewTask(chatId);
  }
  else if (text == "/donetask")
  {
    processDoneTask(chatId);
  }

  else if (text == "/exercise")
  {
    processNewAlert(chatId, 1);
  }

  else if (text == "/water")
  {
    processNewAlert(chatId, 2);
  }

  else if (text == "/alertlist")
  {
    String alertStr = "Here are your alerts:\n" + getAlertsString();
    myBot.sendMessage(chatId, alertStr);
  }

  else if(text == "/addmod"){
    addMod(msg.sender.id, modList, &length);
  }
  else if(text == "/listmod"){
    listMods(msg.sender.id, modList);
  }
  else if(text == "/listtt"){
    listTimetable(msg.sender.id, timetable);
  }

  else if (text == "/help" || text == "/start")
  {
    myBot.sendMessage(chatId, getHelpString());
  }
  else
  {
    myBot.sendMessage(chatId, "Unknown command. Type /help for help.");
  }
}

void processNewAlert(unsigned int chatId, byte aType)
{
  TBMessage msg;
  int num;
  myBot.sendMessage(chatId, "Alerts will be randomised between 0900 to 1700.\nEnter number of alert [1 (default) to 5]:");
  while (myBot.getNewMessage(msg) != CTBotMessageText)
  {
    delay(BOT_MIN_REFRESH_DELAY);
  }
  if (msg.text.length() > 1)
  {
    num = 1;
  }
  else
  {
    (msg.text.toInt() == 0) ? num = 1 : num = msg.text.toInt();
  }

  unsigned int exitCode = addAlert(num, aType);

  if (exitCode)
  {
    myBot.sendMessage(chatId, "Failed to add alert");
  }
  else
  {
    myBot.sendMessage(chatId, "Alert added successfully");
  }
}

unsigned int addAlert(int num, byte aType)
{
  if (aLength + num < 10)
  {
    for (int i = 0; i < num; i++)
    {
      aHour[aLength] = random(9, 17);
      aMin[aLength] = random(0, 60);
      aTypeL[aLength] = aType;
      aLength++;
    }
    return 0;
  }
  return 1;
}

void processNewTask(unsigned int chatId)
{
  String desc;
  unsigned int prio;
  TBMessage msg;
  myBot.sendMessage(chatId, "Okay, send me your task description");
  while (myBot.getNewMessage(msg) != CTBotMessageText)
  {
    delay(BOT_MIN_REFRESH_DELAY);
  }
  desc = msg.text;

  myBot.sendMessage(chatId, "Select its priority level\n[0 - Low,  1 - Med,  2 - High]");

  while (!myBot.getNewMessage(msg))
  {
    delay(BOT_MIN_REFRESH_DELAY);
  }

  if (msg.text.equals("0"))
  {
    prio = LOW_PRIO;
  }
  else if (msg.text.equals("1"))
  {
    prio = MED_PRIO;
  }
  else if (msg.text.equals("2"))
  {
    prio = HIGH_PRIO;
  }
  else
  {
    prio = LOW_PRIO;
  }

  unsigned int exitCode = addTask(desc, prio, chatId);

  if (exitCode)
  {
    myBot.sendMessage(chatId, "Failed to add task");
  }
  else
  {
    myBot.sendMessage(chatId, "Task added successfully");
  }
}

void processDoneTask(unsigned int chatId)
{
  String tasksStr = getTasksString();
  myBot.sendMessage(chatId, "Select index of task to be marked as done\n" + tasksStr);

  TBMessage msg;
  while (!myBot.getNewMessage(msg))
  {
    delay(BOT_MIN_REFRESH_DELAY);
  }

  long selectedIdx = msg.text.toInt();
  while (selectedIdx < 1 || selectedIdx > currLength)
  {
    myBot.sendMessage(chatId, "Invalid index, please send again");
    while (!myBot.getNewMessage(msg))
    {
      delay(BOT_MIN_REFRESH_DELAY);
    }
    selectedIdx = msg.text.toInt();
  }

  unsigned int exitCode = markTaskDone(selectedIdx - 1);

  if (exitCode)
  {
    myBot.sendMessage(chatId, "Failed to mark task as done");
  }
  else
  {
    myBot.sendMessage(chatId, "Task completed, good job!");
  }
}

unsigned int addTask(String description, unsigned int priority, unsigned int chatId)
{
  if (currLength < MAX_LENGTH)
  {
    tasks[currLength].desc = description;
    tasks[currLength].prio = priority;
    currLength++;

    return 0;
  }

  return 1;
}

unsigned int markTaskDone(unsigned int idx)
{
  if (idx < currLength)
  {
    for (int i = idx; i < currLength - 1; i++)
    {
      tasks[i].desc = tasks[i + 1].desc;
      tasks[i].prio = tasks[i + 1].prio;
    }

    currLength--;

    return 0;
  }

  return 1;
}

String getAlertsString()
{
  if (aLength == 0)
  {
    return "No alerts yet!";
  }

  quickSort(aHour, aMin, aTypeL, 0, aLength - 1);

  String alertStr = "";

  for (int i = 0; i < aLength; i++)
  {
    String type = (aTypeL[i] == 2) ? "Water" : "Exercise";
    String hour = (aHour[i] < 10) ? "0" + String(aHour[i]) : String(aHour[i]);
    String min = (aMin[i] < 10) ? "0" + String(aMin[i]): String(aMin[i]);

    alertStr += String(i + 1) + ": ";

    alertStr += hour + ":" + min + " " + " [" + type + "]\n";
  }

  return alertStr;
}

String getTasksString()
{
  if (currLength == 0)
  {
    return "No tasks yet!";
  }

  String taskStr = "";

  for (int i = 0; i < currLength; i++)
  {
    taskStr.concat(String(i + 1) + ": [" + PRIORITY_STRING_MAP[tasks[i].prio] + "] " + tasks[i].desc + "\n");
  }

  return taskStr;
}

String getHelpString()
{
  String helpStr = "Here are the available commands:\n/list to list all tasks\n/newtask to add a new task\n/donetask to mark a task as done\n";
  helpStr += "/exercise to create exercise alerts\n/water to create water alerts\n/alertlist to get your list of alerts";
  return helpStr;
}

void printCurrentTime()
{
  Serial.print(timeClient.getHours());
  Serial.print(":");
  Serial.print(timeClient.getMinutes());
  Serial.print(":");
  Serial.println(timeClient.getSeconds());
}

void alertLight(byte aType)
{

  for (int j = 0; j < 5; j++)
  {
    for (int i = 0; i < NUM_LEDS; i++)
    {
      // alert for lesson
      if (aType == 0)
      {
        leds[i] = 0xFF0000;
      }
      // alert for water
      else if (aType == 1)
      {
        leds[i] = 0x0000FF;
      }
      // alert for exercise
      else
      {
        leds[i] = 0x00FF00;
      }
      FastLED.setBrightness(10);
    }
    FastLED.show();
    delay(100);
    FastLED.clear();
    FastLED.show();
    delay(100);// clear all pixel data
  }
}

void taskLight(Task tasks[], unsigned int numTask)
{
  FastLED.clear();
  byte high, med, low = 0;
  for (int i = 0; i < numTask; i++)
  {
    switch (tasks[i].prio)
    {
    case 0:
      low++;
      break;
    case 1:
      med++;
      break;
    case 2:
      high++;
      break;
    }
  }
  for (int i = 0; i < high; i++)
  {
    leds[i] = 0xFF0000;
    FastLED.setBrightness(30);
  }
  for (int i = high; i < med + high; i++)
  {
    leds[i] = 0xFF8000;
    FastLED.setBrightness(30);
  }
  for (int i = med + high; i < low + med + high; i++)
  {
    leds[i] = 0x00FFFF;
    FastLED.setBrightness(30);
  }
  FastLED.show();
  FastLED.show();
}

void swap(int *a, int *b)
{
  int temp = *a;
  *a = *b;
  *b = temp;
}

void swap(byte *a, byte *b)
{
  byte temp = *a;
  *a = *b;
  *b = temp;
}

int partition(int hour[], int min[], byte type[], int low, int high)
{
  int hrPivot = hour[high];
  int minPivot = min[high];

  int i = (low - 1);
  for (int j = low; j <= high - 1; j++)
  {
    if ((hour[j] < hrPivot) || (hour[j] == hrPivot && min[j] <= minPivot))
    {
      i++;
      swap(&hour[i], &hour[j]);
      swap(&min[i], &min[j]);
      swap(&type[i], &type[j]);
    }
  }
  swap(&hour[i + 1], &hour[high]);
  swap(&min[i + 1], &min[high]);
  swap(&type[i + 1], &type[high]);
  return (i + 1);
}

void quickSort(int hour[], int min[], byte type[], int low, int high)
{
  if (low < high)
  {
    int pi = partition(hour, min, type, low, high);
    quickSort(hour, min, type, low, pi - 1);
    quickSort(hour, min, type, pi + 1, high);
  }
}

void blink(int color){
  // 0 is red, 1 is green, 2 is blue
  for (int j = 0; j < 10; j++){
    fill_solid(leds, NUM_LEDS, color == 1 ? 0x008000 : color == 2 ? 0x003C5F : 0xFF0000);
    FastLED.show();
    FastLED.show();
    FastLED.show();
    FastLED.show();
    delay(500);
    fill_solid(leds, NUM_LEDS, 0x000000);
    FastLED.show();
    FastLED.show();
    FastLED.show();
    FastLED.show();
    delay(500);
  }
  fill_solid(leds, NUM_LEDS, 0x000000);
  FastLED.show();
}

void alertNextClass()
{
//    String today = DAYS_OF_WEEK[timeClient.getDay()];
//    unsigned int currHr = timeClient.getHours();
//    unsigned int currMin = timeClient.getMinutes();

    // Change this to hard code time for testing
    String today = "Monday";
    unsigned int currHr = 7;
    unsigned int currMin = 55;

    if (millis() - lastAlertTime > MIN_DELAY_BEFORE_ALERT)
    {
        for (int i = 0; i < numTimes; i++)
        {
            unsigned int lessonHr = timetable[i].startTime.substring(0, 2).toInt();
            unsigned int lessonMin = timetable[i].startTime.substring(2, 4).toInt();

            bool isWithinFiveMin = (currHr == lessonHr && (lessonMin - currMin) <= 5) || (currHr == (lessonHr - 1) && lessonMin + 60 - currMin <= 5);

            if (timetable[i].day == today && isWithinFiveMin)
            {
                blink(0);
                myBot.sendMessage(USER, "ALERT! LESSON STARTING");
                
                lastAlertTime = millis();
            }
        }
    }
}

void alertIfWithinFiveMinutes(int hours[], int mins[], int length, byte type[])
{
    unsigned int currHr = timeClient.getHours();
    unsigned int currMin = timeClient.getMinutes();
    Serial.println(currHr);
    Serial.println(currMin);

    if (millis() - lastAlertTime > MIN_DELAY_BEFORE_ALERT)
    {
        for (int i = 0; i < length; i++)
        {
            if ((hours[i] == currHr && mins[i] - currMin <= 2) || (hours[i] == currHr + 1 && mins[i] + 60 - currMin <= 2))
            {
 
                if (type[i] == 1) {
                  blink(1);
                }
                else {
                  blink(2);
                }


              
              if (type[i] == 1) {
                myBot.sendMessage(USER, "ALERT! TIME FOR PHYSICAL ACTIVITY");
              }
              else {
                myBot.sendMessage(USER, "ALERT! TIME FOR HYDRATION");
              }
                
                
              lastAlertTime = millis();
            }
        }
    }
}
