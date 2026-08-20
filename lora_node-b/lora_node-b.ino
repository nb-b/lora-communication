#include <Arduino.h>
#include "LoRa_E22.h"

// ============================================================
// NODE B
//
// ROLE:
// Receive CALL from Node A
// Generate random sensor data B
// Forward CALL to Node C
// Receive DATA from Node C
// Send DATA B and DATA C to Node A
//
// ALL COMMUNICATION = FIXED TRANSMISSION
// ============================================================


// ============================================================
// E22 PINS
// ============================================================

#define LORA_RX 16
#define LORA_TX 17

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
// STATE
// ============================================================

bool waitingForC = false;

String currentRequestId = "";

String dataB = "";

unsigned long waitStart = 0;

const unsigned long C_TIMEOUT = 10000;


// ============================================================
// RANDOM SENSOR B
// ============================================================

int readSensorB() {
    return random(10, 500);
}


// ============================================================
// SEND FIXED MESSAGE TO NODE A
// ============================================================

void sendToNodeA(String message) {

    Serial.println();
    Serial.println("----------------------------------------");
    Serial.println("NODE B -> NODE A");
    Serial.println("MODE    : FIXED");
    Serial.println("MESSAGE : " + message);
    Serial.println("----------------------------------------");


    ResponseStatus rs = e22.sendFixedMessage(
        NODE_A_ADDH,
        NODE_A_ADDL,
        CHANNEL,
        message
    );


    Serial.print("STATUS: ");
    Serial.println(rs.getResponseDescription());
}


// ============================================================
// SEND FIXED MESSAGE TO NODE C
// ============================================================

void sendToNodeC(String message) {

    Serial.println();
    Serial.println("----------------------------------------");
    Serial.println("NODE B -> NODE C");
    Serial.println("MODE    : FIXED");
    Serial.println("MESSAGE : " + message);
    Serial.println("----------------------------------------");


    ResponseStatus rs = e22.sendFixedMessage(
        NODE_C_ADDH,
        NODE_C_ADDL,
        CHANNEL,
        message
    );


    Serial.print("STATUS: ");
    Serial.println(rs.getResponseDescription());
}


// ============================================================
// START COLLECTION
//
// Called when receiving:
//
// CALL:001
//
// from Node A
// ============================================================

void startCollection(String requestId) {

    // --------------------------------------------------------
    // PREVENT OVERLAPPING REQUEST
    // --------------------------------------------------------

    if (waitingForC) {

        Serial.println();
        Serial.println("WARNING: ALREADY PROCESSING REQUEST");
        Serial.println("IGNORE NEW CALL");

        return;
    }


    Serial.println();
    Serial.println("========================================");
    Serial.println("NODE B START COLLECTION");
    Serial.println("REQUEST ID: " + requestId);
    Serial.println("========================================");


    // --------------------------------------------------------
    // RESET STATE
    // --------------------------------------------------------

    currentRequestId = requestId;

    dataB = "";

    waitingForC = false;


    // --------------------------------------------------------
    // GENERATE RANDOM SENSOR B
    // --------------------------------------------------------

    int sensorValueB = readSensorB();


    dataB =
        "DATA:" +
        currentRequestId +
        ":B:" +
        String(sensorValueB);


    Serial.println();
    Serial.println("NODE B SENSOR DATA");
    Serial.println(dataB);


    // --------------------------------------------------------
    // CREATE CALL FOR NODE C
    // --------------------------------------------------------

    String callMessage =
        "CALL:" +
        currentRequestId;


    // --------------------------------------------------------
    // SEND CALL TO NODE C
    // --------------------------------------------------------

    sendToNodeC(callMessage);


    // --------------------------------------------------------
    // START WAITING FOR NODE C
    // --------------------------------------------------------

    waitingForC = true;

    waitStart = millis();


    Serial.println();
    Serial.println("WAITING FOR NODE C...");
}


// ============================================================
// HANDLE DATA FROM NODE C
//
// Expected:
//
// DATA:001:C:250
// ============================================================

void handleDataFromC(String message) {

    // --------------------------------------------------------
    // CHECK STATE
    // --------------------------------------------------------

    if (!waitingForC) {

        Serial.println();
        Serial.println("WARNING: NOT WAITING FOR NODE C");
        Serial.println("IGNORE: " + message);

        return;
    }


    // --------------------------------------------------------
    // PARSE PACKET
    // --------------------------------------------------------

    // Format:
    //
    // DATA:REQUEST_ID:NODE:VALUE
    //
    // Example:
    //
    // DATA:001:C:250

    int p1 = message.indexOf(':');
    int p2 = message.indexOf(':', p1 + 1);
    int p3 = message.indexOf(':', p2 + 1);


    if (p1 == -1 ||
        p2 == -1 ||
        p3 == -1) {

        Serial.println();
        Serial.println("ERROR: INVALID DATA PACKET");
        Serial.println(message);

        return;
    }


    String packetType =
        message.substring(0, p1);

    String requestId =
        message.substring(p1 + 1, p2);

    String node =
        message.substring(p2 + 1, p3);


    // --------------------------------------------------------
    // VALIDATE TYPE
    // --------------------------------------------------------

    if (packetType != "DATA") {

        Serial.println("ERROR: NOT DATA PACKET");

        return;
    }


    // --------------------------------------------------------
    // VALIDATE REQUEST ID
    // --------------------------------------------------------

    if (requestId != currentRequestId) {

        Serial.println();
        Serial.println("WARNING: WRONG REQUEST ID");

        Serial.println(
            "EXPECTED: " + currentRequestId
        );

        Serial.println(
            "RECEIVED: " + requestId
        );

        return;
    }


    // --------------------------------------------------------
    // MUST BE NODE C
    // --------------------------------------------------------

    if (node != "C") {

        Serial.println();
        Serial.println("WARNING: EXPECTED NODE C DATA");

        Serial.println(
            "RECEIVED NODE: " + node
        );

        return;
    }


    // --------------------------------------------------------
    // NODE C DATA ACCEPTED
    // --------------------------------------------------------

    Serial.println();
    Serial.println("========================================");
    Serial.println("NODE B RECEIVED DATA FROM C");
    Serial.println("DATA: " + message);
    Serial.println("========================================");


    // ========================================================
    // COMBINE B + C DATA INTO ONE MESSAGE
    // ========================================================
    int lastColon = dataB.lastIndexOf(':');
    String valueB = dataB.substring(lastColon + 1);
    lastColon = message.lastIndexOf(':');
    String valueC = message.substring(lastColon + 1);

    // Final format:
    // DATA:882:B:190:C:191

    String combinedMessage =
        "DATA:" +
        currentRequestId +
        ":B:" +
        valueB +
        ":C:" +
        valueC;

    Serial.println();
    Serial.println("========================================");
    Serial.println("FORWARDING COMBINED B + C DATA TO NODE A");
    Serial.println("MESSAGE: " + combinedMessage);
    Serial.println("========================================");

    sendToNodeA(combinedMessage);


    // --------------------------------------------------------
    // FINISH REQUEST
    // --------------------------------------------------------

    waitingForC = false;

    Serial.println();
    Serial.println("========================================");
    Serial.println("NODE B REQUEST COMPLETE");
    Serial.println("RETURN TO IDLE");
    Serial.println("========================================");
}


// ============================================================
// HANDLE CALL
// ============================================================

void handleCall(String message) {

    // Expected:
    //
    // CALL:001

    int colonIndex = message.indexOf(':');

    if (colonIndex == -1) {

        Serial.println("INVALID CALL PACKET");

        return;
    }


    String requestId =
        message.substring(colonIndex + 1);

    requestId.trim();


    if (requestId.length() == 0) {

        Serial.println("EMPTY REQUEST ID");

        return;
    }


    startCollection(requestId);
}


// ============================================================
// RECEIVE LORA MESSAGE
// ============================================================

void receiveLoRa() {

    if (e22.available() > 1) {

        ResponseContainer rc = e22.receiveMessage();

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
        Serial.println("NODE B RECEIVED");
        Serial.println("MESSAGE: " + message);
        Serial.println("========================================");


        // ----------------------------------------------------
        // CALL FROM NODE A
        // ----------------------------------------------------

        if (message.startsWith("CALL:")) {
            handleCall(message);

            return;
        }


        // ----------------------------------------------------
        // DATA FROM NODE C
        // ----------------------------------------------------

        if (message.startsWith("DATA:")) {

            handleDataFromC(message);

            return;
        }


        // ----------------------------------------------------
        // UNKNOWN PACKET
        // ----------------------------------------------------

        Serial.println();
        Serial.println("UNKNOWN MESSAGE");
        Serial.println(message);
    }
}


// ============================================================
// CHECK NODE C TIMEOUT
// ============================================================

void checkTimeout() {

    if (!waitingForC) {
        return;
    }


    unsigned long elapsed =
        millis() - waitStart;


    if (elapsed >= C_TIMEOUT) {

        Serial.println();
        Serial.println("========================================");
        Serial.println("NODE C TIMEOUT");
        Serial.println("REQUEST ID: " + currentRequestId);
        Serial.println("========================================");


        // ----------------------------------------------------
        // SEND B DATA TO A ANYWAY
        // ----------------------------------------------------

        if (dataB.length() > 0) {

            Serial.println();
            Serial.println("SEND B DATA TO A ANYWAY");

            sendToNodeA(dataB);
        }

        waitingForC = false;

        Serial.println();
        Serial.println("NODE B RETURN TO IDLE");
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
    Serial.println("NODE B STARTING");
    Serial.println("MODE: FIXED ONLY");
    Serial.println("========================================");

    randomSeed(micros());

    loraSerial.begin(
        9600,
        SERIAL_8N1,
        LORA_RX,
        LORA_TX
    );

    e22.begin();


    Serial.println();
    Serial.println("========================================");
    Serial.println("NODE B READY");
    Serial.println("WAITING FOR NODE A...");
    Serial.println("========================================");
}


// ============================================================
// LOOP
// ============================================================

void loop() {
    if (Serial.available()) {
        String command = Serial.readStringUntil('\n');
        command.trim();
        if (command.startsWith("CALL:")) {

            Serial.println();
            Serial.println("========================================");
            Serial.println("TEST CALL FROM SERIAL");
            Serial.println(command);
            Serial.println("========================================");

            handleCall(command);

        } else {

            Serial.println("UNKNOWN SERIAL COMMAND");
            Serial.println(command);
        }
    }
    receiveLoRa();
    checkTimeout();
    delay(10);
}