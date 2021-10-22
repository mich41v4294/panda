#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

typedef struct
{
  uint32_t TIR;  /*!< CAN TX mailbox identifier register */
  uint32_t TDTR; /*!< CAN mailbox data length control and time stamp register */
  uint32_t TDLR; /*!< CAN mailbox data low register */
  uint32_t TDHR; /*!< CAN mailbox data high register */
} CAN_TxMailBox_TypeDef;

#define DATA_SIZE_MAX 8
typedef struct __attribute__((packed)) {
  unsigned char reserved2 : 1;
  unsigned char returned : 1;
  unsigned char extended : 1;  
  unsigned int addr : 29;
  unsigned int bus_time : 24;
  unsigned char bus : 2;
  unsigned char len : 6;
  unsigned char data[DATA_SIZE_MAX];
} CANPacket_t;

typedef struct
{
  uint32_t CNT;
} TIM_TypeDef;

struct sample_t torque_meas;
struct sample_t torque_driver;

TIM_TypeDef timer;
TIM_TypeDef *MICROSECOND_TIMER = &timer;
uint32_t microsecond_timer_get(void);

// from board_declarations.h
#define HW_TYPE_UNKNOWN 0U
#define HW_TYPE_WHITE_PANDA 1U
#define HW_TYPE_GREY_PANDA 2U
#define HW_TYPE_BLACK_PANDA 3U
#define HW_TYPE_PEDAL 4U
#define HW_TYPE_UNO 5U
#define HW_TYPE_DOS 6U

#define ALLOW_DEBUG

// from main_declarations.h
uint8_t hw_type = HW_TYPE_UNKNOWN;

// from config.h
#define MIN(a,b)                                \
  ({ __typeof__ (a) _a = (a);                   \
    __typeof__ (b) _b = (b);                    \
    _a < _b ? _a : _b; })

#define MAX(a,b)                                \
  ({ __typeof__ (a) _a = (a);                   \
    __typeof__ (b) _b = (b);                    \
    _a > _b ? _a : _b; })

#define ABS(a)                                  \
 ({ __typeof__ (a) _a = (a);                    \
   (_a > 0) ? _a : (-_a); })

// from faults.h
#define FAULT_RELAY_MALFUNCTION         (1U << 0)
void fault_occurred(uint32_t fault) {
}
void fault_recovered(uint32_t fault) {
}

// from config.h
#define GET_BUS(msg) ((msg)->bus)
#define GET_LEN(msg) ((msg)->len)
#define GET_ADDR(msg) ((msg)->addr)
#define GET_BYTE(msg, b) ((msg)->data[(int)(b)])
#define GET_BYTES_04(msg) ((msg)->data[0] | ((msg)->data[1] << 8) | ((msg)->data[2] << 16) | ((msg)->data[3] << 24))
#define GET_BYTES_48(msg) ((msg)->data[4] | ((msg)->data[5] << 8) | ((msg)->data[6] << 16) | ((msg)->data[7] << 24))
#define GET_FLAG(value, mask) (((__typeof__(mask))(value) & (mask)) == (mask))

// Flasher and pedal with raw mailbox access
#define GET_MAILBOX_BYTE(msg, b) (((int)(b) > 3) ? (((msg)->RDHR >> (8U * ((unsigned int)(b) % 4U))) & 0xFFU) : (((msg)->RDLR >> (8U * (unsigned int)(b))) & 0xFFU))
#define GET_MAILBOX_BYTES_04(msg) ((msg)->RDLR)
#define GET_MAILBOX_BYTES_48(msg) ((msg)->RDHR)

#define UNUSED(x) (void)(x)

#ifndef PANDA
#define PANDA
#endif
#define NULL ((void*)0)
#define static
#include "safety.h"

uint32_t microsecond_timer_get(void) {
  return MICROSECOND_TIMER->CNT;
}

void set_controls_allowed(bool c){
  controls_allowed = c;
}

void set_unsafe_mode(int mode){
  unsafe_mode = mode;
}

void set_relay_malfunction(bool c){
  relay_malfunction = c;
}

void set_gas_interceptor_detected(bool c){
  gas_interceptor_detected = c;
}

bool get_controls_allowed(void){
  return controls_allowed;
}

int get_unsafe_mode(void){
  return unsafe_mode;
}

bool get_relay_malfunction(void){
  return relay_malfunction;
}

bool get_gas_interceptor_detected(void){
  return gas_interceptor_detected;
}

int get_gas_interceptor_prev(void){
  return gas_interceptor_prev;
}

bool get_gas_pressed_prev(void){
  return gas_pressed_prev;
}

bool get_brake_pressed_prev(void){
  return brake_pressed_prev;
}

bool get_cruise_engaged_prev(void){
  return cruise_engaged_prev;
}

bool get_vehicle_moving(void){
  return vehicle_moving;
}

int get_hw_type(void){
  return hw_type;
}

void set_timer(uint32_t t){
  timer.CNT = t;
}

void set_torque_meas(int min, int max){
  torque_meas.min = min;
  torque_meas.max = max;
}

int get_torque_meas_min(void){
  return torque_meas.min;
}

int get_torque_meas_max(void){
  return torque_meas.max;
}

void set_torque_driver(int min, int max){
  torque_driver.min = min;
  torque_driver.max = max;
}

int get_torque_driver_min(void){
  return torque_driver.min;
}

int get_torque_driver_max(void){
  return torque_driver.max;
}

void set_rt_torque_last(int t){
  rt_torque_last = t;
}

void set_desired_torque_last(int t){
  desired_torque_last = t;
}

void set_desired_angle_last(int t){
  desired_angle_last = t;
}

void set_honda_alt_brake_msg(bool c){
  honda_alt_brake_msg = c;
}

void set_honda_bosch_long(bool c){
  honda_bosch_long = c;
}

int get_honda_hw(void) {
  return honda_hw;
}

void set_honda_fwd_brake(bool c){
  honda_fwd_brake = c;
}

void init_tests(void){
  // get HW_TYPE from env variable set in test.sh
  hw_type = atoi(getenv("HW_TYPE"));
  safety_mode_cnt = 2U;  // avoid ignoring relay_malfunction logic
  unsafe_mode = 0;
  set_timer(0);
}

void init_tests_honda(void){
  init_tests();
  honda_fwd_brake = false;
}

void set_gmlan_digital_output(int to_set){
}

void reset_gmlan_switch_timeout(void){
}

void gmlan_switch_init(int timeout_enable){
}

