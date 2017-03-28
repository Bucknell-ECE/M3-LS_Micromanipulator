/*
M3LS.cpp - An Arduino library for translating high level commands to M3-LS API
           calls over SPI
Created by Matthew Hawkins
Copyright info?
*/

#include "M3LS.h"

#define DEBUG   //If you comment this line, the DPRINT & DPRINTLN lines are defined as blank.
#ifdef DEBUG    //Macros are usually in all capital letters.
  #define DPRINT(...)    Serial.print(__VA_ARGS__)     //DPRINT is a macro, debug print
  #define DPRINTLN(...)  Serial.println(__VA_ARGS__)   //DPRINTLN is a macro, debug print with new line
#else
  #define DPRINT(...)     //now defines a blank line
  #define DPRINTLN(...)   //now defines a blank line
#endif

// Constructors
// Class constructor for a one axis M3LS micromanipulator setup
M3LS::M3LS(int X_SS){
    // Initialize variables
    numAxes = 1;
    pins[0] = X_SS;
    initialize();
}

// Class constructor for a two axis M3LS micromanipulator setup
M3LS::M3LS(int X_SS, int Y_SS){
    // Initialize variables
    numAxes = 2;
    pins[0] = X_SS;
    pins[1] = Y_SS;
    initialize();
}

// Class constructor for a three axis M3LS micromanipulator setup
M3LS::M3LS(int X_SS, int Y_SS, int Z_SS){
    // Initialize variables
    numAxes = 3;
    pins[0] = X_SS;
    pins[1] = Y_SS;
    pins[2] = Z_SS;
    initialize();
}

// Public functions
// Calibrate the stages
void M3LS::calibrate(){
    for (int axis = 0; axis < numAxes; axis++){
            // Build command and send it to SPI
            memcpy(sendChars, "<87 4>\r", 7);
            sendSPICommand(axis, 7);
    }
}

// Sets the current control mode to the new mode
void M3LS::setControlMode(ControlMode newMode){
    if (newMode == open && currentControlMode != open){
        memcpy(sendChars, "<20 0>\r", 7);
        for (int axis = 0; axis < numAxes; axis++){
            sendSPICommand(pins[axis], 7);
        }
    } else if(newMode != open && currentControlMode == open){
        memcpy(sendChars, "<20 1>\r", 7);
        for (int axis = 0; axis < numAxes; axis++){
            sendSPICommand(pins[axis], 7);
        }
    } else if(newMode == position && currentControlMode != position){
        // This is where re-centering has to occur.
        // get current position, re-center bounds around that.
        getCurrentPosition(); // BLOCKING
        recenter(currentPosition[0], currentPosition[1], currentPosition[2]);
    }
    currentControlMode = newMode;
}

// Default method for updating the needle's position
void M3LS::updatePosition(int inp0, int inp1, int inp2){
    updatePosition(inp0, inp1, inp2, XYZ, false);
}

// Default method for updating the needle's position with a trigger arg
void M3LS::updatePosition(int inp0, int inp1, int inp2, bool isActive){
    updatePosition(inp0, inp1, inp2, XYZ, isActive);
}

// Default method for updating the needle's position with an axis arg
void M3LS::updatePosition(int inp0, int inp1, int inp2, Axes axis){
    updatePosition(inp0, inp1, inp2, axis, false);
}

// Update the needle's position based upon current mode and joystick inputs
void M3LS::updatePosition(int inp0, int inp1, int inp2, Axes axis, bool isActive){
    // Handle inputs based on the current control mode
    switch(currentControlMode)
    {
        case hold     : if (isActive){
                            moveToTargetPosition(inp0, inp1, inp2, axis);
                        }
                        break;
        case position : // Map the inputs based on the current bounds
                        // Joystick reports 0-255
                        inp0 = map(inp0, 0, 255, center[0]-radius, center[0]+radius);
                        inp1 = map(inp1, 0, 255, center[1]-radius, center[1]+radius);
                        inp2 = map(inp2, 0, 255, center[2]-radius, center[2]+radius);
                        moveToTargetPosition(inp0, inp1, inp2, axis);
                        break;

        case velocity : // Set the speed and target positions based on
                        // displacement, divided between 7 zones
                        // This should result in zone 0 being a "dead zone."
                        int numZones = 7;
                        int scaleFactor = 10;
                        inp0 = map(inp0, 0, 255, -((numZones - 1) / 2), 
                                    ((numZones - 1) / 2)) * scaleFactor;
                        inp1 = map(inp1, 0, 255, -((numZones - 1) / 2), 
                                    ((numZones - 1) / 2)) * scaleFactor;
                        inp2 = map(inp2, 0, 255, -((numZones - 1) / 2), 
                                    ((numZones - 1) / 2)) * scaleFactor;
                        // setMotorSpeed(abs(inp0), abs(inp1), abs(inp2));
                        advanceMotor(inp0, inp1, inp2);
                        break;
    }
}

// Store the current position as the home position
void M3LS::setHome(){
    getCurrentPosition();
    memcpy(homePosition, currentPosition, numAxes * sizeof(int));
    DPRINT("Setting home to ");
    DPRINT(homePosition[0]); DPRINT(" ");
    DPRINT(homePosition[1]); DPRINT(" ");
    DPRINTLN(homePosition[2]);
}

// Return to the stored home position
void M3LS::returnHome(){
    // Store current mode and switch to position mode
    ControlMode previousMode = currentControlMode;
    setControlMode(position);
    DPRINTLN("Returning home");
    DPRINT(homePosition[0]); DPRINT(" ");
    DPRINTLN(homePosition[1]);

    // Raise Z axis
    if (numAxes > 2){
        getCurrentPosition();
        // TODO: Figure out appropriate offset
        moveToTargetPosition(currentPosition[2] + 10, Z);
    }

    // Move X and Y to home position
    moveToTargetPosition(homePosition[0], homePosition[1], XY);
    recenter(homePosition[0], homePosition[1], homePosition[2]);

    // Restored previous mode
    setControlMode(previousMode);
}

// Gets and stores the current position of each stage
void M3LS::getCurrentPosition(){
    for (int axis = 0; axis < numAxes; axis++){
        currentPosition[axis] = getAxisPosition(pins[axis]);
    }
}

// Adjust the internal bounds based on a given number of encoder counts
void M3LS::setBounds(int raw){
    radius = map(raw, 0, 255, 10, 5500);
}

// Private Functions
// Initialize starting parameters and SPI settings
void M3LS::initialize(){
    // Set the default control mode
    currentControlMode = position;

    // Initialize all pins as unselected outputs
    for (int pin = 0; pin < numAxes; pin++){
        pinMode(pins[pin], OUTPUT);
        digitalWrite(pins[pin], HIGH);
    }

    // Set the default internal bounds
    center[0]=6000; center[1]=6000; center[2]=6000;
    radius = 5500;

#ifndef MOCK
    // Initialize SPI
    SPI.begin();
    SPI.beginTransaction(SPISettings(2000000, MSBFIRST, SPI_MODE1));

    // Calibrate the stages
    calibrate();

    // Ensure the system is in position mode
    setControlMode(M3LS::open);
    setControlMode(M3LS::position);
#endif
}

// Get the current position of a single stage
int M3LS::getAxisPosition(int pin){
    /*
    Send to controller:
        <10>\r
    Receive from controller:
        <10 SSSSSS PPPPPPPP EEEEEEEE>\r
        length: 30 bytes
    */

    // Build command and send it to SPI
    memcpy(sendChars, "<10>\r", 5);
    sendSPICommand(pin, 5);

    // Allocate space for and generate position value
    char position[11] = "0x";
    memcpy(position+2, recvChars + 11, 8);
    position[10] = 0;
    return strtol(position, NULL, 0);
}

// Default single axis move command
void M3LS::moveToTargetPosition(int target0){
    moveToTargetPosition(target0, X);
}

// Move the specified axis to the target position
void M3LS::moveToTargetPosition(int target0, Axes axis){
    setTargetPosition(target0);
    sendSPICommand(pins[axis], 14);
}

// Default two axis move command
void M3LS::moveToTargetPosition(int target0, int target1){
    moveToTargetPosition(target0, target1, XY);
}

// Move the specified axes to the target positions
void M3LS::moveToTargetPosition(int target0, int target1, Axes axis){
    switch(axis)
    {
        case XY  :  setTargetPosition(target0);
                    sendSPICommand(pins[0], 14);
                    setTargetPosition(target1);
                    sendSPICommand(pins[1], 14);
                    break;
        case XZ  :  setTargetPosition(target0);
                    sendSPICommand(pins[0], 14);
                    setTargetPosition(target1);
                    sendSPICommand(pins[2], 14);
                    break;
        case YZ  :  setTargetPosition(target0);
                    sendSPICommand(pins[1], 14);
                    setTargetPosition(target1);
                    sendSPICommand(pins[2], 14);
                    break;
    }
}

// Default three axis move command
void M3LS::moveToTargetPosition(int target0, int target1, int target2){
    moveToTargetPosition(target0, target1, target2, XYZ);
}

// Move the specified axes to the target positions
void M3LS::moveToTargetPosition(int target0, int target1, int target2, Axes axis){


    switch(axis)
    {
        case X   :  setTargetPosition(target0);
                    sendSPICommand(pins[0], 14);
                    break;
        case Y   :  setTargetPosition(target1);
                    sendSPICommand(pins[1], 14);
                    break;
        case Z   :  setTargetPosition(target2);
                    sendSPICommand(pins[2], 14);
                    break;
        case XY  :  setTargetPosition(target0);
                    sendSPICommand(pins[0], 14);
                    setTargetPosition(target1);
                    sendSPICommand(pins[1], 14);
                    break;
        case XZ  :  setTargetPosition(target0);
                    sendSPICommand(pins[0], 14);
                    setTargetPosition(target2);
                    sendSPICommand(pins[2], 14);
                    break;
        case YZ  :  setTargetPosition(target1);
                    sendSPICommand(pins[1], 14);
                    setTargetPosition(target2);
                    sendSPICommand(pins[2], 14);
                    break;
        case XYZ :  setTargetPosition(target0);
                    sendSPICommand(pins[0], 14);
                    setTargetPosition(target1);
                    sendSPICommand(pins[1], 14);
                    setTargetPosition(target2);
                    sendSPICommand(pins[2], 14);
                    break;
    }
}

// Set the target position to move to
void M3LS::setTargetPosition(int target){
    /*
    Send to controller:
        <08>\r
    Receive from controller:
        <10 SSSSSS PPPPPPPP EEEEEEEE>\r
        length: 30 bytes
    */

    // Build command and send it to SPI
    memcpy(sendChars, "<08 ", 4);
    sprintf(sendChars + 4, "%08x", target);
    memcpy(sendChars + 12, ">\r", 2);
}

// Set the controller's sensitivity to a new value
void M3LS::setMotorSpeed(int inp0, int inp1, int inp2){
    /*
    Send to controller:
        <40 SSSSSS CCCCCC AAAAAA IIII>
    Receive from controller:
        <40>
    */

    // Build commands and send them to SPI
    memcpy(sendChars, "<40 ", 4);
    sprintf(sendChars + 4, "%06x", inp0);
    memcpy(sendChars + 10, " 000033 0000CD 0001>\r", 20);
    sendSPICommand(pins[0], 30);

    memcpy(sendChars, "<40 ", 4);
    sprintf(sendChars + 4, "%06x", inp1);
    memcpy(sendChars + 10, " 000033 0000CD 0001>\r", 20);
    sendSPICommand(pins[1], 30);

    memcpy(sendChars, "<40 ", 4);
    sprintf(sendChars + 4, "%06x", inp2);
    memcpy(sendChars + 10, " 000033 0000CD 0001>\r", 20);
    sendSPICommand(pins[2], 30);
}

// Move the needle a short distance based on each axis's current zone
void M3LS::advanceMotor(int inp0, int inp1, int inp2){
    memcpy(sendChars, "<06 ", 4);
    sprintf(sendChars + 4, "%01d", inp0 < 0);
    memcpy(sendChars + 5, " ", 1);
    sprintf(sendChars + 6, "%08x", inp0);
    memcpy(sendChars + 14, ">\r", 2);
    sendSPICommand(pins[0], 16);

    memcpy(sendChars, "<06 ", 4);
    sprintf(sendChars + 4, "%01d", inp1 < 0);
    memcpy(sendChars + 5, " ", 1);
    sprintf(sendChars + 6, "%08x", inp1);
    memcpy(sendChars + 14, ">\r", 2);
    sendSPICommand(pins[1], 16);

    memcpy(sendChars, "<06 ", 4);
    sprintf(sendChars + 4, "%01d", inp2 < 0);
    memcpy(sendChars + 5, " ", 1);
    sprintf(sendChars + 6, "%08x", inp2);
    memcpy(sendChars + 14, ">\r", 2);
    sendSPICommand(pins[2], 16);
}

// Set the needle's current position as the new center by generating new bounds
void M3LS::recenter(int newx, int newy, int newz){
    center[0]=newx;
    center[1]=newy;
    center[2]=newz;
}

// Sends a command over the SPI bus and writes the response to the buffer
int M3LS::sendSPICommand(int pin, int length){
    SPI.beginTransaction(SPISettings(2000000, MSBFIRST, SPI_MODE1));
    memset(recvChars, 0, 100);
    digitalWrite(pin, LOW);
    for(int i=0; i<length; i++){
        SPI.transfer(sendChars[i]);
        // Minimum delay time: 60 microseconds between SPI transfers.
        delayMicroseconds(60);
    }

    int j = 0;
    int counter = 0;
    while('<' != (recvChars[j] = SPI.transfer(IN_PROGRESS))){
        delayMicroseconds(60);
        if (counter++ == 100) break;
    }
    delayMicroseconds(60);
    while(DONE != (recvChars[++j] = SPI.transfer(IN_PROGRESS))){
        delayMicroseconds(60);
        if(j >= 99) return -1;
    }
    // DPRINT("Received from M3-LS:");
    // DPRINTLN(recvChars);
    // DPRINT("Took ");
    // DPRINT(j-1);
    // DPRINTLN(" iterations.\n");
    digitalWrite(pin, HIGH);
    SPI.endTransaction();
    return 0;
}