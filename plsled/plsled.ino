#include <FastLED.h>
#define LED_PIN 5
#define NUM_LEDS 60
#define MAX_NUM_TASKS 30

struct Task {
  String desc;
  byte prio;
};

Task tasks[MAX_NUM_TASKS];

CRGB leds[NUM_LEDS];
 
void setup() {
  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, 500);
  FastLED.clear();
  FastLED.show();
  Serial.begin(9600);
  Serial.println("Setup");
  alert();
}


void alert() {
  for (int i=0; i<NUM_LEDS; i++ )
  {
      leds[i] = 0xFF0000;
      FastLED.setBrightness(10);
  } 
  FastLED.show();
  delay (500);
  FastLED.clear();  // clear all pixel data
  FastLED.show();
  delay (500);
  FastLED.clear();
}

void taskLight(Task tasks[], byte numTask) {
  byte high,med,low = 0;
  for (int i=0; i<numTask; i++){
    Serial.print(tasks[i].prio);
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
  Serial.print("Low: ");
  Serial.println(low);
    Serial.print("Med: ");
  Serial.println(med);
    Serial.print("High: ");
  Serial.println(high); 
  for(int i=0; i<high; i++){
      leds[i] = 0xFF0000;
      FastLED.setBrightness(110);    
  }
  for(int i=high; i<med+high; i++){
      leds[i] = 0xFF8000;
      FastLED.setBrightness(110);    
  }
  for(int i=med+high; i<low+med+high; i++){
      leds[i] = 0xFFFF00;
      FastLED.setBrightness(110);    
  }
  FastLED.show(); 
  Serial.println("LIGHT UP");
}


void loop() {
  Task a = {"erwr",1};
  Task b = {"22", 0};
  tasks[0] = a;
  tasks[1] = b;
  taskLight(tasks,2);

}
