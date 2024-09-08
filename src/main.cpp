#include <Arduino.h>
#include <PadmanESP32.h>




PadmanESP32 *padman;
int TRANSMIT_RATE_MS=1000;
int POLLING_RATE_MS=1000;
unsigned long previousMillis = 0;  // will store last time a message was send

void setup() {
    // Start Serial:
  Serial.begin(9600);
  printf("Setup\n");
  padman = new PadmanESP32();
}

void loop() {
  if(padman->get_state()==STATES::UNINITIALIZED){return;}
  

  //
  padman->loop();

  // Send message
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= TRANSMIT_RATE_MS) {
    printf(" --- loop ---\n");
    previousMillis = currentMillis;
    //send_message();
  
      
    padman->send_canbus_jointstate();
    printf("%f %u",padman->get_position(), padman->get_id());


      

    }


    
  
}