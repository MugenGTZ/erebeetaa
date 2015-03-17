// Wrapper for libComedi Elevator control.
// These functions provides an interface to the elevators in the real time lab
//
// 2007, Martin Korsgaard


#include <stdlib.h>
#include "../elev.h"
#include "../channels.h"


// Number of signals and lamps on a per-floor basis (excl sensor)
#define N_BUTTONS 3

static const int lamp_channel_matrix[N_FLOORS][N_BUTTONS] = {
    {LIGHT_UP1, LIGHT_DOWN1, LIGHT_COMMAND1},
    {LIGHT_UP2, LIGHT_DOWN2, LIGHT_COMMAND2},
    {LIGHT_UP3, LIGHT_DOWN3, LIGHT_COMMAND3},
    {LIGHT_UP4, LIGHT_DOWN4, LIGHT_COMMAND4},
};


static const int button_channel_matrix[N_FLOORS][N_BUTTONS] = {
    {BUTTON_UP1, BUTTON_DOWN1, BUTTON_COMMAND1},
    {BUTTON_UP2, BUTTON_DOWN2, BUTTON_COMMAND2},
    {BUTTON_UP3, BUTTON_DOWN3, BUTTON_COMMAND3},
    {BUTTON_UP4, BUTTON_DOWN4, BUTTON_COMMAND4},
};

int elev_init(void) {
    
    return 1;
}

void elev_set_motor_direction(elev_motor_direction_t dirn) {
    
}

void elev_set_door_open_lamp(int value) {
    }

int elev_get_obstruction_signal(void) {
	return 1;
   
}

int elev_get_stop_signal(void) {
	return 1;
    
}

void elev_set_stop_lamp(int value) {
 
}

int elev_get_floor_sensor_signal(void) {
	return 1;
   
}

void elev_set_floor_indicator(int floor) {
          
}

int elev_get_button_signal(elev_button_type_t button, int floor) {
	return 1;
   
}

void elev_set_button_lamp(elev_button_type_t button, int floor, int value) {
    
}

