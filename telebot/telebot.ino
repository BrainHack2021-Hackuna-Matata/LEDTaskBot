#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiUDP.h>
#include <NTPClient.h>
#include <CTBot.h>
#include <ArduinoJson.h>
#include <FastLED.h>

#define MAX_LENGTH 30

#define LED_PIN 5
#define NUM_LEDS 60
#define MAX_NUM_TASKS 30

CRGB leds[NUM_LEDS];

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

/*
Task list parameters
*/
struct Task
{
  String desc;
  byte prio;
};

struct Task tasks[MAX_LENGTH];
unsigned int currLength = 0;
const String PRIORITY_STRING_MAP[3] = {"Low", "Med", "High"};

void setup()
{
  /*
  Set up WiFi
  */
  Serial.begin(115200);

 //set up ledstrip
  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, 500);
  FastLED.clear();
  FastLED.show();
  Serial.println("LED Setup");
  alertLight(0);

  // blocks until connects
  myBot.wifiConnect(WIFI_SSID, WIFI_PWD);

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


  /*
  Setup Telegram bot
  */
  myBot.setTelegramToken(TELE_BOT_TOKEN);
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

  if (text == "/on")
  {
    myBot.sendMessage(chatId, "LED is now ON!");
    digitalWrite(LED_BUILTIN, LOW);
  }
  else if (text == "/off")
  {
    myBot.sendMessage(chatId, "LED is now OFF!");
    digitalWrite(LED_BUILTIN, HIGH);
  }
  else if (text == "/list")
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
  else if (text == "/help")
  {
    myBot.sendMessage(chatId, getHelpString());
  }
  else
  {
    myBot.sendMessage(chatId, "Unknown command. Type /help for help.");
  }
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
  return "Here are the available commands:\n/list to list all tasks\n/newtask to add a new task\n/donetask to mark a task as done\n";
}

void printCurrentTime()
{
  Serial.print(timeClient.getHours());
  Serial.print(":");
  Serial.print(timeClient.getMinutes());
  Serial.print(":");
  Serial.println(timeClient.getSeconds());
}


void alertLight(byte aType) {

  for (int i=0; i<NUM_LEDS; i++ )
  {
    // alert for lesson
    if (aType == 0) {
      leds[i] = 0xFF0000;
    }
    // alert for water
    else if (aType == 1) {
      leds[i] = 0x0000FF;
    }
    // alert for exercise
    else {
      leds[i] = 0x00FF00;
    }
    FastLED.setBrightness(10);
  } 

  if (USER != 0) {
    switch (aType) {
      case 0:
        myBot.sendMessage(USER, "ALERT! LESSON STARTING");
        break ;
      case 1:
        myBot.sendMessage(USER, "ALERT! DRINK WATER");
        break;
      case 2:
        myBot.sendMessage(USER, "ALERT! PHYSICAL ACTIVITY TIME");
        break;
    } 
  }



  for (int i=0; i<10; i++) {
    FastLED.show();
    delay (500);
    FastLED.clear();  // clear all pixel data
  }

}

void taskLight(Task tasks[], unsigned int numTask) {
  byte high,med,low = 0;
  for (int i=0; i<numTask; i++){
    switch (tasks[i].prio) {
      case 0:
        low++;
        break ;
      case 1:
        med++;
        break;
      case 2:
        high++;
        break;
    }
  }
  for(int i=0; i<high; i++){
      leds[i] = 0xFF0000;
      FastLED.setBrightness(30);    
  }
  for(int i=high; i<med+high; i++){
      leds[i] = 0xFF8000;
      FastLED.setBrightness(30);    
  }
  for(int i=med+high; i<low+med+high; i++){
      leds[i] = 0x00FFFF;
      FastLED.setBrightness(30);    
  }
  FastLED.show(); 
}

