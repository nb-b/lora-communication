#include <Arduino.h>
#include "LoRa_E22.h"

// ============================================================
// NODE C
//
// ROLE:
// Receive CALL from Node B
// Generate random sensor data C
// Send DATA C back to Node B
//
// ALL COMMUNICATION = FIXED TRANSMISSION
// ============================================================


// ============================================================
// E220 PINS
// ============================================================

#define LORA_RX 32
#define LORA_TX 33

#define AUX_PIN 21
#define M0_PIN  22
#define M1_PIN  23


HardwareSerial loraSerial(2);

LoRa_E22 e22(
    &loraSerial,
    AUX_PIN,
    M0_PIN,
    M1_PIN
);


// ============================================================
// NETWORK CONFIG
// ============================================================

#define CHANNEL 79


// ============================================================
// NODE ADDRESSES
// ============================================================

// Node A = 0x000A
#define NODE_A_ADDH 0x00
#define NODE_A_ADDL 0x01

// Node B = 0x000B
#define NODE_B_ADDH 0x00
#define NODE_B_ADDL 0x02

// Node C = 0x000C
#define NODE_C_ADDH 0x00
#define NODE_C_ADDL 0x03


// ============================================================
// RANDOM SENSOR C
// ============================================================

int readSensorC() {
    return random(10, 500);
}


// ============================================================
// SEND FIXED MESSAGE TO NODE B
// ============================================================

void sendToNodeB(String message) {

    Serial.println();
    Serial.println("----------------------------------------");
    Serial.println("NODE C -> NODE B");
    Serial.println("MODE    : FIXED");
    Serial.println("MESSAGE : " + message);
    Serial.println("----------------------------------------");


    ResponseStatus rs = e22.sendFixedMessage(
        NODE_B_ADDH,
        NODE_B_ADDL,
        CHANNEL,
        message
    );


    Serial.print("STATUS: ");
    Serial.println(rs.getResponseDescription());
}


// ============================================================
// PROCESS CALL FROM NODE B
//
// Expected:
// CALL:001
// ============================================================

void processCall(String requestId) {

    Serial.println();
    Serial.println("========================================");
    Serial.println("NODE C PROCESSING REQUEST");
    Serial.println("REQUEST ID: " + requestId);
    Serial.println("========================================");

    int sensorValueC = readSensorC();


    Serial.println();
    Serial.print("RANDOM SENSOR C VALUE: ");
    Serial.println(sensorValueC);


    String dataMessage =
        "DATA:" +
        requestId +
        ":C:" +
        String(sensorValueC);


    // Example:
    //
    // DATA:001:C:250


    Serial.println();
    Serial.println("CREATED DATA PACKET:");
    Serial.println(dataMessage);


    // --------------------------------------------------------
    // SEND DATA TO NODE B
    // --------------------------------------------------------

    sendToNodeB(dataMessage);


    Serial.println();
    Serial.println("========================================");
    Serial.println("NODE C REQUEST COMPLETE");
    Serial.println("RETURN TO IDLE");
    Serial.println("========================================");
}


// ============================================================
// HANDLE CALL PACKET
// ============================================================

void handleCall(String message) {
    int colonIndex = message.indexOf(':');


    if (colonIndex == -1) {
        Serial.println("ERROR: INVALID CALL PACKET");

        return;
    }


    // Get everything after CALL:
    String requestId =
        message.substring(colonIndex + 1);


    requestId.trim();


    // --------------------------------------------------------
    // VALIDATE REQUEST ID
    // --------------------------------------------------------

    if (requestId.length() == 0) {
        Serial.println("ERROR: EMPTY REQUEST ID");
        return;
    }


    // --------------------------------------------------------
    // PROCESS REQUEST
    // --------------------------------------------------------

    processCall(requestId);
}


// ============================================================
// RECEIVE LORA MESSAGE
// ============================================================

void receiveLoRa() {

    if (e22.available() > 1) {

        ResponseContainer rc =
            e22.receiveMessage();


        // ----------------------------------------------------
        // CHECK RECEIVE STATUS
        // ----------------------------------------------------

        if (rc.status.code != 1) {

            Serial.println();
            Serial.println("LORA RECEIVE ERROR");

            Serial.println(
                rc.status.getResponseDescription()
            );

            return;
        }


        // ----------------------------------------------------
        // GET MESSAGE
        // ----------------------------------------------------

        String message = rc.data;

        message.trim();


        Serial.println();
        Serial.println("========================================");
        Serial.println("NODE C RECEIVED");
        Serial.println("MESSAGE: " + message);
        Serial.println("========================================");


        // ----------------------------------------------------
        // CALL FROM NODE B
        // ----------------------------------------------------

        if (message.startsWith("CALL:")) {

            handleCall(message);

            return;
        }


        // ----------------------------------------------------
        // IGNORE OTHER MESSAGES
        // ----------------------------------------------------

        Serial.println();
        Serial.println("UNKNOWN / IGNORED MESSAGE");
        Serial.println(message);
    }
}


// ============================================================
// SETUP
// ============================================================

void setup() {

    Serial.begin(115200);

    delay(1000);


    Serial.println();
    Serial.println("========================================");
    Serial.println("NODE C STARTING");
    Serial.println("ROLE: SENSOR NODE");
    Serial.println("MODE: FIXED ONLY");
    Serial.println("========================================");


    // --------------------------------------------------------
    // RANDOM SEED
    // --------------------------------------------------------

    randomSeed(micros());


    // --------------------------------------------------------
    // START LORA UART
    // --------------------------------------------------------

    loraSerial.begin(
        9600,
        SERIAL_8N1,
        LORA_RX,
        LORA_TX
    );


    // --------------------------------------------------------
    // START E220
    // --------------------------------------------------------

    e22.begin();


    Serial.println();
    Serial.println("========================================");
    Serial.println("NODE C READY");
    Serial.println("WAITING FOR NODE B...");
    Serial.println("========================================");
}


// ============================================================
// LOOP
// ============================================================

void loop() {
    receiveLoRa();
    delay(10);
}