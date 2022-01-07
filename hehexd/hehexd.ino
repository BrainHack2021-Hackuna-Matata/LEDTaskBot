/*
 Name:		    echoBot.ino
 Created:	    12/21/2017
 Author:	    Stefano Ledda <shurillu@tiscalinet.it>
 Description: a simple example that check for incoming messages
              and reply the sender with the received message
*/
#include <CTBot.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <string>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h> 

CTBot myBot;

String ssid  = "Xiaomi_CAA7"    ; // REPLACE mySSID WITH YOUR WIFI SSID
String pass  = "notgonnatellu"; // REPLACE myPassword YOUR WIFI PASSWORD, IF ANY
String token = "5097903121:AAF2d-VcKmdLbL9baufQD9cecGXo-g5aHbo"   ; // REPLACE myToken WITH YOUR TELEGRAM BOT TOKEN

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

void setup() {
	// initialize the Serial
	Serial.begin(115200);
	Serial.println("Starting TelegramBot...");

	// connect the ESP8266 to the desired access point
	myBot.wifiConnect(ssid, pass);

	// set the telegram bot token
	myBot.setTelegramToken(token);
	
	// check if all things are ok
	if (myBot.testConnection())
		Serial.println("\ntestConnection OK");
	else
		Serial.println("\ntestConnection NOK");

  pinMode(LED_BUILTIN, OUTPUT);   // built in LED
  digitalWrite(LED_BUILTIN, LOW); // active LOW

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
  DynamicJsonDocument doc(4096);
  StaticJsonDocument<200> filter;
  filter["semesterData"][0]["timetable"][0] = true;
  filter["semesterData"][0]["semester"] = true;
  myBot.sendMessage(id, "Enter a Module Code");
  while(myBot.getNewMessage(m) != CTBotMessageText){}
  currmod.modCode = String(m.text);
  myBot.sendMessage(id, "Enter the class code for the module " + currmod.modCode);
  while(myBot.getNewMessage(m) != CTBotMessageText){}
  currmod.classNo = String(m.text);
  myBot.sendMessage(id, "The module " + currmod.modCode + " with class number " + currmod.classNo + " has been added.");
  modList[*length] = currmod;
  *length += 1;
  String ddd = "Your lessons for this module are on: ";
  String json = getTimetable("https://api.nusmods.com/v2/2021-2022/modules/" + currmod.modCode +".json");
  deserializeJson(doc, json, DeserializationOption::Filter(filter));
  int semester = 0;
  Serial.println(String(doc["semesterData"][0]["semester"].as<unsigned int>()));
  if(doc["semesterData"][0]["semester"].as<unsigned int>() == 1){
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
  // String s;
  // serializeJson(doc, s);
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

void loop() {
	// a variable to store telegram message data
	TBMessage msg;
	// if there is an incoming message...
	if (CTBotMessageText == myBot.getNewMessage(msg))
  {
		// ...forward it to the sender
    if(msg.text == "/addmod"){
      addMod(msg.sender.id, modList, &length);
    }
    else if(msg.text == "/listmod"){
      listMods(msg.sender.id, modList);
    }
    else if(msg.text == "/listtt"){
      listTimetable(msg.sender.id, timetable);
    }
    else{
		  myBot.sendMessage(msg.sender.id, msg.text);
    }
  }
	// wait 500 milliseconds
	delay(500);
}
