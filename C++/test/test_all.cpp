#include "gtest/gtest.h"
#include "M3LS.h"
using ::testing::Return;

TEST(Constructor, SingleAxis){
    // Initialize test parameters
    int pins[] = {1};
    int numAxes = 1;

    // Initialize mock Arduino and SPI
    ArduinoMock* arduinoMock = arduinoMockInstance();
    SPIMock* spiMock = SPIMockInstance();

    // Set up expected calls for constructor
    EXPECT_CALL(*arduinoMock, delay(50));
    EXPECT_CALL(*spiMock, begin());
    for (int pin = 0; pin < numAxes; pin++){
        EXPECT_CALL(*arduinoMock, pinMode(pins[pin], OUTPUT));
        EXPECT_CALL(*arduinoMock, digitalWrite(pins[pin], HIGH));
    }
    M3LS m3 = M3LS(pins[0]);

    // Set up exected calls for begin
    EXPECT_CALL(*arduinoMock, delay(250)).Times(numAxes * 4);
    m3.begin();

    // Cleanup mock
    releaseArduinoMock();
    releaseSPIMock();
}

// TEST(Constructor, DoubleAxis){
//     // Initialize test parameters
//     int pins[] = {1, 2};
//     int numAxes = 2;

//     // Initialize mock Arduino instance and expected calls
//     ArduinoMock* arduinoMock = arduinoMockInstance();
//     for (int pin = 0; pin < numAxes; pin++){
//         EXPECT_CALL(*arduinoMock, pinMode(pins[pin], OUTPUT));
//         EXPECT_CALL(*arduinoMock, digitalWrite(pins[pin], HIGH));
//     }

//     // Initialize M3LS and execute functions to be tested
//     M3LS m3 = M3LS(pins[0], pins[1]);
//     m3.begin();
//     releaseArduinoMock();
// }

// TEST(Constructor, TripleAxis){
//     // Initialize test parameters
//     int pins[] = {1, 2, 3};
//     int numAxes = 3;

//     // Initialize mock Arduino instance and expected calls
//     ArduinoMock* arduinoMock = arduinoMockInstance();
//     for (int pin = 0; pin < numAxes; pin++){
//         EXPECT_CALL(*arduinoMock, pinMode(pins[pin], OUTPUT));
//         EXPECT_CALL(*arduinoMock, digitalWrite(pins[pin], HIGH));
//     }

//     // Initialize M3LS and execute functions to be tested
//     M3LS m3 = M3LS(pins[0], pins[1], pins[2]);
//     m3.begin();
//     releaseArduinoMock();
// }
