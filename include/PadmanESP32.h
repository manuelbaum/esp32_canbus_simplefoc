#pragma once

#include "PadmanESP32.h"
#include <SimpleFOC.h>
#include <map>
#include "driver/twai.h"

enum MSG_IDS_REL{
    CMD = 0,
    STATE = 1,
    TARGET_TORQUE = 2,
    TARGET_POSITION = 3,
    STATE_POSITION = 4,
    STATE_VELOCITY = 5,
    STATE_EFFORT = 6};

enum CMD_IDS{
    REQ_STATUS=1,
    INIT_FOC=2,
    FIND_JOINTLIMITS=3,
    CMD_CTRL_TORQUE=4,
    CMD_CTRL_POSITION=5,
    REBOOT=6
};

enum STATES{
    UNINITIALIZED=0,
    INITIALIZED_FOC=1,
    FIND_LIMIT_LOWER=2,
    FIND_LIMIT_UPPER=3,
    INITIALIZED_JOINT=4,
    CTRL_TORQUE=5,
    CTRL_POSITION=6
};



class PadmanESP32{
    private:
    // pins we use for canbus
    int pin_canbus_rx=5;
    int pin_canbus_tx=4;


    std::map<std::array<uint8_t, 6>, uint8_t> map_mac_to_id;
    bool is_canbus_driver_installed=false;

    uint8_t mac8[6];
    uint8_t id;

    float position;
    float lim_lower;
    float lim_upper;
    float gear_ratio;
    float joint_target = 0;
    float tau = 0;
    float kp = 1.0;
    STATES state;

    int n_msg_received=0;
    int n_msg_req_position=0;

    MagneticSensorSPI sensor; // padman v01
    //BLDCMotor motor = BLDCMotor(7, 14.4, 128); //GB2208
    BLDCMotor motor;// = BLDCMotor(7, 15.3, 53); //GB4106

    //BLDCDriver3PWM driver = BLDCDriver3PWM(25, 26, 27, 33); // Motor 1
    //BLDCDriver3PWM driver = BLDCDriver3PWM(17, 16, 32, 33); // Motor 2
    //BLDCDriver3PWM driver = BLDCDriver3PWM(12, 14, 27, 13); // rewired
        BLDCDriver3PWM driver;// = BLDCDriver3PWM(7, 8, 9, 6); // padman
    // BLDCDriver3PWM driver = BLDCDriver3PWM(pwmA, pwmB, pwmC, Enable(optional));

      twai_message_t message_position;

    public:
        PadmanESP32();
        void init();
        void init_simplefoc();
        void init_canbus();
        void send_canbus_position();
        void send_canbus_state();
        void readMacAddress(uint8_t baseMac[6]);

        void update_sensor();
        float get_position();
        float get_id(){return id;};
        STATES get_state(){return state;};
        void canbus_callback();
        void canbus_polling();
        void loop();
        void switch_ctrl_position();
        void print_can_statistic();
        void check_twai_status_and_recover();

};

