#include "PadmanESP32.h"

#include "esp_mac.h"
#include "esp_wifi.h"
#include <WiFi.h>
#include "driver/twai.h"



// Helper function to clamp torque, coulndt import std::clamp for some reason
float clamp(float n, float lower, float upper) {
  return std::max(lower, std::min(n, upper));
}

PadmanESP32::PadmanESP32():sensor(AS5048_SPI, 10),
                          state(STATES::UNINITIALIZED),
                          motor(7, 15.3, 53),
                          driver(7, 8, 9, 6),
                          message_position()
                          {


  map_mac_to_id = {
    { std::array<uint8_t, 6>({0x34, 0xb7, 0xda, 0x5a, 0x48, 0xd0}), 0 },
    { std::array<uint8_t, 6>({0x34, 0xb7, 0xda, 0x5a, 0x48, 0xbc}), 1 },
    { std::array<uint8_t, 6>({0x34, 0xb7, 0xda, 0x5a, 0x48, 0xb8}), 2 },
    { std::array<uint8_t, 6>({0x34, 0xb7, 0xda, 0x5a, 0x48, 0xc0}), 3 },
  }; // std::map<std::array<uint8_t, 6>, uint8_t> 
   // padman v01
  
  delay(1000);
  init_canbus();

  // delay(100);
  // init_simplefoc();


  delay(1000);

  // Figure out our ID from the mac address
  readMacAddress(mac8);
  // printf("MAC address:");
  // printf("%02:%02:%02:%02:%02:%02\n", mac8[0], mac8[1], mac8[2], mac8[3], mac8[4], mac8[5]);

  std::array<uint8_t, 6> mac_array;
  std::copy(std::begin(mac8), std::end(mac8), mac_array.begin());
  id = map_mac_to_id[mac_array];
  printf("ID:%u\n", id);


  std::map<uint8_t, float> map_gear_ratio = {
    { 0, 4.0},
    { 1, 4.0},
    { 2, 1.0},
    { 3, 4.0},
  }; 
  gear_ratio = map_gear_ratio[id];

  std::map<uint8_t, float> map_joint_offset = {
    { 0, -3.141/6.0 + 3.141},
    { 1, 0.0},
    { 2, 0.0},
    { 3, 0.0},
  }; 
  joint_offset = map_joint_offset[id];

  std::map<uint8_t, float> map_sign = {
    { 0, 1.0},
    { 1, 1.0},
    { 2, 1.0},
    { 3, 1.0},
  }; 
  sign = map_sign[id];

  // prepare position msg
  message_position.identifier = MSG_IDS_REL::STATE_POSITION + ((id+1) *100);
  message_position.data_length_code = sizeof(position);
  message_position.rtr = false;
  memcpy(message_position.data, &position, sizeof(position));

}


void PadmanESP32::readMacAddress(uint8_t baseMac[6]) {
    WiFi.STA.begin();
    //uint8_t baseMac[6];
    esp_err_t ret = esp_wifi_get_mac(WIFI_IF_STA, baseMac);
    if (ret == ESP_OK) {
        printf("MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
                      baseMac[0], baseMac[1], baseMac[2],
                      baseMac[3], baseMac[4], baseMac[5]);
    } else {
        printf("Failed to read MAC address\n");
    }
}


void PadmanESP32::init_simplefoc(){
    printf("Initializing SimpleFOC\n");
    sensor.init();
    // link the motor to the sensor
    motor.linkSensor(&sensor);

    // power supply voltage
    driver.voltage_power_supply = 14.8;
    driver.init(); // <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< THIS ONE SEEMS TO FUCK UP CAN BUS... WHY
    motor.linkDriver(&driver);

    // aligning voltage 
    motor.voltage_sensor_align = 10;

    ////// generated by simplefocstudio
    //
    //// control loop type and torque mode 
    motor.torque_controller = TorqueControlType::voltage;
    motor.controller = MotionControlType::torque;
    
    motor.motion_downsample = 10.0;

    //// velocity loop PID
    motor.PID_velocity.P = 2.1;
    motor.PID_velocity.I = 0.0;
    motor.PID_velocity.D = 0.1;
    motor.PID_velocity.output_ramp = 1.0;
    //motor.PID_velocity.limit = 2.0;
    //// Low pass filtering time constant 
    motor.LPF_velocity.Tf = 0.01;

        // initialize motor
    motor.init();
    // align sensor and start FOC
    motor.initFOC();

    printf("Initialized SimpleFOC\n");
    state = STATES::INITIALIZED_FOC;

}
void recover_from_bus_off() {
    twai_status_info_t status_info;
    twai_get_status_info(&status_info);

    // Check if we are in bus-off state
    if (status_info.state == TWAI_STATE_BUS_OFF) {
        twai_stop();                  // Stop TWAI driver
        twai_start();                 // Restart TWAI driver
        printf("TWAI bus-off detected, attempting recovery...\n");
    }
}

void PadmanESP32::init_canbus(){
  printf("Initializing TWAI\n");

  recover_from_bus_off();
  // Initialize configuration structures using macro initializers
  twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)pin_canbus_tx, (gpio_num_t)pin_canbus_rx, TWAI_MODE_NORMAL);
  //g_config.bus_off_recovery = true;
  twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();  //Look in the api-reference for other speed sets.
  twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

  // Install TWAI driver
  if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK) {
    printf("Driver installed\n");
  } else {
    printf("Failed to install driver\n");
    return;
  }

  // Start TWAI driver
  if (twai_start() == ESP_OK) {
    printf("Driver started\n");
  } else {
    printf("Failed to start driver\n");
    return;
  }

  // Reconfigure alerts to detect TX alerts and Bus-Off errors
  uint32_t alerts_to_enable = TWAI_ALERT_TX_IDLE | TWAI_ALERT_TX_SUCCESS | TWAI_ALERT_TX_FAILED | TWAI_ALERT_ERR_PASS | TWAI_ALERT_BUS_ERROR;
  if (twai_reconfigure_alerts(alerts_to_enable, NULL) == ESP_OK) {
    printf("CAN Alerts reconfigured\n");
  } else {
    printf("Failed to reconfigure alerts\n");
    return;
  }

  // TWAI driver is now successfully installed and started
  is_canbus_driver_installed = true;



    xTaskCreatePinnedToCore([](void* obj) {
      PadmanESP32* instance = static_cast<PadmanESP32*>(obj);
      instance->canbus_polling();
    }, "TWAI Receive Task", 4096, this, 1, NULL, 1);

    // xTaskCreatePinnedToCore(
    //     myTask,                  // Task function
    //     "MyTask",                // Task name
    //     2048,                    // Stack size (bytes)
    //     NULL,                    // Parameter to pass
    //     1,                       // Task priority
    //     &myTaskHandle,           // Task handle
    //     1                        // Core to run on (1 = Core 1, 0 = Core 0)
    // );

    
}

void PadmanESP32::canbus_polling(){
    unsigned long t_prev = millis();
    int CANBUS_STATE_RATE_MS = 1000;
    while (1) {
      canbus_callback();  // Check for received messages in a loop
      //send_canbus_position();
      delay(0.0001);   // Small delay to avoid hogging the CPU

      unsigned long t_now = millis();
      if (t_now - t_prev >= CANBUS_STATE_RATE_MS) {
          send_canbus_state();
          t_prev = t_now;
      }
    }

}

// Callback function for message reception
void PadmanESP32::canbus_callback() {
  
  twai_message_t message;
  if (twai_receive(&message, pdMS_TO_TICKS(10)) == ESP_OK) {
    n_msg_received++;
    // Print the received message ID and data
    // Serial.print("Received message with ID: ");
    // Serial.println(message.identifier);

    // Serial.print("Data: ");
    // for (int i = 0; i < message.data_length_code; i++) {
    //   Serial.print(message.data[i], HEX);
    //   Serial.print(" ");
    // }
    // Serial.println();

    if((message.identifier/100)-1 == id){ // is this for this joint?
      
      switch(message.identifier%100){
        case MSG_IDS_REL::CMD: // is it a command?
          printf("COMMAND: %u",(uint8_t)*message.data);
          switch((uint8_t)*message.data){
            case CMD_IDS::REQ_STATUS:
              send_canbus_state();
              break;
            case CMD_IDS::INIT_FOC: // Received Command to init FOC
              printf("Initializing FOC.");
              init_simplefoc();
              break;
            case FIND_JOINTLIMITS: // Received Command to explore joint limits
              printf("Find Joint Limits.");
              motor.controller = MotionControlType::velocity;
              state=FIND_LIMIT_LOWER;
              break;
            case REBOOT:
              printf("Received REBOOT command. Rebooting!\n");
              twai_stop();
              twai_driver_uninstall();
              ESP.restart();
            case CMD_CTRL_POSITION:
              printf("Received CMD_CTRL_POSITION!\n");
              motor.controller = MotionControlType::torque;
              set_desired_position(0);
              state=CTRL_POSITION;
            case CMD_CTRL_TORQUE:
              printf("Received CMD_CTRL_TORQUE!\n");
              motor.controller = MotionControlType::torque;
              set_desired_position(0);
              state=CTRL_POSITION;
          }
        
        case MSG_IDS_REL::TARGET_POSITION:
          
          //joint_target = (float_t)*message.data;
          float new_target;
          memcpy(&new_target, message.data, sizeof(new_target));
          set_desired_position(new_target);
          // printf("Received a new joint target %f", joint_target);

      } 
    }
    else if ((message.identifier/100) == id){ // is it for the previous joint? (we check because we only send our position if the previous joint sent its position)
      switch(message.identifier%100){
        case MSG_IDS_REL::STATE_POSITION:
          n_msg_req_position++;
          send_canbus_position();
          break;
      }
    }

  } else {
    //Serial.println("Failed to receive message");
  }
}

void PadmanESP32::send_canbus_state(){
  twai_message_t message_state;
  message_state.identifier = MSG_IDS_REL::STATE + ((id+1) *100);
  message_state.data_length_code = sizeof(state);
  message_state.rtr = false;
  memcpy(message_state.data, &state, sizeof(state));

  // Queue message for transmission
  if (twai_transmit(&message_state, pdMS_TO_TICKS(1000)) == ESP_OK) {
    //printf("Message queued for transmission -- State\n");
  } else {
    printf("Failed to queue message for transmission -- State\n");
  }
}

void PadmanESP32::send_canbus_position() {
float position=joint_position();
if(position != position){
    printf("NAN!!!!");
    printf("Joint %i Sending Position: %f %f %f\n", id, position, motor.shaftAngle(), sensor.getAngle());
}

memcpy(message_position.data, &position, sizeof(position));
  // Queue message for transmission
  if (twai_transmit(&message_position, pdMS_TO_TICKS(10)) == ESP_OK) {
    //printf("Message queued for transmission from id %d\n",id);
  } else {
    //printf("Failed to queue message for transmission from id %d\n",id);
  }


  // twai_message_t message_state;
  // message_state.identifier = MSG_IDS_REL::STATE + ((id+1) *100);
  // message_state.data_length_code = sizeof(state);
  // message_state.rtr = false;
  // memcpy(message_state.data, &state, sizeof(state));

  // // Queue message for transmission
  // if (twai_transmit(&message_state, pdMS_TO_TICKS(1000)) == ESP_OK) {
  //   printf("Message queued for transmission\n");
  // } else {
  //   printf("Failed to queue message for transmission\n");
  // }

  // twai_message_t message_gear_ratio;
  // message_gear_ratio.identifier = 12 + (id *100);
  // message_gear_ratio.data_length_code = sizeof(gear_ratio);
  // message_gear_ratio.rtr = false;
  // memcpy(message_gear_ratio.data, &gear_ratio, sizeof(gear_ratio));

  // // Queue message for transmission
  // if (twai_transmit(&message_gear_ratio, pdMS_TO_TICKS(1000)) == ESP_OK) {
  //   printf("Message queued for transmission\n");
  // } else {
  //   printf("Failed to queue message for transmission\n");
  // }

}

void PadmanESP32::print_can_statistic(){
  printf("Messages received since last print: %d %d\n",n_msg_received, n_msg_req_position);
  n_msg_received=0;
  n_msg_req_position=0;
}

#define ERROR_PASSIVE_THRESHOLD 127

void PadmanESP32::check_twai_status_and_recover() {
    twai_status_info_t status_info;

    // Get the current TWAI status
    twai_get_status_info(&status_info);

    printf("TWAI Status:\n");
    printf("State: %s\n", 
        status_info.state == TWAI_STATE_RUNNING ? "Running" :
        status_info.state == TWAI_STATE_STOPPED ? "Stopped" :
        status_info.state == TWAI_STATE_BUS_OFF ? "Bus-Off" : "Unknown");
    printf("Tx Error Counter: %d\n", status_info.tx_error_counter);
    printf("Rx Error Counter: %d\n", status_info.rx_error_counter);
    printf("Messages Pending Tx: %d\n", status_info.msgs_to_tx);
    printf("Messages Pending Rx: %d\n", status_info.msgs_to_rx);

    // Check for Bus-Off state and initiate recovery
    if (status_info.state == TWAI_STATE_BUS_OFF) {
        printf("Node is in Bus-Off state. Initiating recovery...\n");
        
        // Stop TWAI driver to allow recovery
        twai_stop();
        
        // Initiate recovery
        esp_err_t recovery_err = twai_initiate_recovery();
        if (recovery_err == ESP_OK) {
            printf("Recovery initiated successfully. Waiting for completion...\n");
            
            // Wait for a while to ensure recovery is complete
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            
            // Restart TWAI driver
            twai_start();
            printf("TWAI driver restarted after recovery.\n");
        } else {
            printf("Failed to initiate recovery. Error: %s\n", esp_err_to_name(recovery_err));
        }
    }
    // Check for Error-Passive state by inspecting error counters
    else if (status_info.tx_error_counter > ERROR_PASSIVE_THRESHOLD || status_info.rx_error_counter > ERROR_PASSIVE_THRESHOLD) {
        printf("Node is in Error-Passive state (Tx Error: %d, Rx Error: %d). Consider taking action to recover.\n",
               status_info.tx_error_counter, status_info.rx_error_counter);
        // Optional: Add recovery logic if desired (e.g., stop/start driver)
    } else {
        printf("TWAI bus is in a healthy state.\n");
    }
}

void PadmanESP32::switch_ctrl_position(){
  motor.controller = MotionControlType::torque;
  state = CTRL_POSITION;
}

void PadmanESP32::loop(){
  update_sensor(); 
  motor.loopFOC();
  motor.move();

  float d_vel_findjointlimit = 1.0;

  switch(state){
  case FIND_LIMIT_LOWER:
        motor.target = d_vel_findjointlimit*gear_ratio;
        if(motor.voltage.q>6){ // we found the limit
          lim_lower = motor.shaftAngle();
          state = FIND_LIMIT_UPPER;
          Serial.println(F("Switching to find_limit_upper"));
        }   
        break;
  case FIND_LIMIT_UPPER:
        motor.target = -d_vel_findjointlimit*gear_ratio;
        if(motor.voltage.q<-6){ // we found the limit
          lim_upper = motor.shaftAngle();
          //switch_ctrl_position();
          motor.controller = MotionControlType::torque;
          motor.target = 0.0;
          state = INITIALIZED_JOINT;

          // StartTime = millis();

        }
        break;
  case CTRL_POSITION :
        motor.controller = MotionControlType::torque;
        tau = kp*(get_desired_position() - joint_position());
        motor.target = tau;
        //motor.target = clamp(tau,-0.5, 0.5);
        break;

    // case oscillate_up :
    //     motor.controller = MotionControlType::torque;
    //     motor_position = (motor.shaftAngle() - jc.lim_lower) - (jc.lim_upper - jc.lim_lower)/2. ;
    //     joint_position = motor_position / jc.gear_ratio;

    //     joint_target = 3.141/4;
        
    //     torque = kp*(joint_target - joint_position);
        
    //     motor.target = clamp(torque,-10.5, 10.5);

    //     if (millis() - StartTime > 1000){
    //       state = centering;
    //       StartTime = millis();
    //     }
    //     break;
  }

}

float PadmanESP32::joint_position(){
  float motor_position = (motor.shaftAngle() - lim_lower) - (lim_upper - lim_lower)/2. ; // compute off-zero position in motor space (temporarily assuming zero is in the middle of joint limits in motor space)
  float joint_position = sign * (motor_position / gear_ratio - joint_offset); // transfer to joint space, first using gear ratio, then offsetting joint_offset which is specified in joint-space
  return joint_position;
}

void PadmanESP32::update_sensor(){

  sensor.update();
  position = sensor.getAngle();
}

float PadmanESP32::get_position(){
  return position;
}