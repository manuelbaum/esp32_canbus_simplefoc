#include <Arduino.h>
#include <PadmanESP32.h>




PadmanESP32 *padman;
int TRANSMIT_RATE_MS=10;
int POLLING_RATE_MS=1000;
int PRINT_RATE_MS = 1000;
unsigned long previousMillis = 0;  // will store last time a message was send

void setup() {
    // Start Serial:
  delay(0.1);
  Serial.begin(9600);
  delay(0.1);
  printf("Setup\n");
  padman = new PadmanESP32();
}

void loop() {
  // Send message
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= PRINT_RATE_MS) {
    //printf(" --- loop ---\n");
    //padman->print_can_statistic();
    
    padman->check_twai_status_and_recover();

    previousMillis = currentMillis;
    //send_message();
  
      
    // padman->send_canbus_jointstate();
    //printf("%f %u",padman->get_position(), padman->get_id());
  }

  if(padman->get_state()==STATES::UNINITIALIZED){return;}
  

  //
  padman->loop();



  //padman->send_canbus_position();
    
  
}