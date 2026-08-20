#include <Arduino.h>
#include "LoRa_E220.h"

// ============================================================
// NODE A
//
// ROLE:
// PI -> A  : Transparent CALL
// A -> B   : Fixed CALL
// B -> A   : Fixed DATA
// C -> A   : Fixed DATA forwarded by B
// A -> PI  : Transparent final result
// ============================================================


// ============================================================
// E220 PINS
// ============================================================

#define LORA_RX 16
#define LORA_TX 17

#define AUX_PIN 21
#define M0_PIN  22
#define M1_PIN  23


HardwareSerial loraSerial(2);

LoRa_E220 e220ttl(
    &loraSerial,
    AUX_PIN,
    M0_PIN,
    M1_PIN
);


// ============================================================
// NETWORK CONFIG
// ============================================================

// IMPORTANT:
// Must match your E220 channel configuration
#define CHANNEL 79


// ------------------------------------------------------------
// NODE ADDRESSES
// ------------------------------------------------------------

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
// COLLECTION STATE
// ============================================================

enum E220OperationMode {
    MODE_TRANSPARENT,
    MODE_FIXED
};

E220OperationMode currentMode = MODE_TRANSPARENT;

bool collecting = false;

String currentRequestId = "";

String dataA = "";
String dataB = "";
String dataC = "";

bool gotB = false;
bool gotC = false;

unsigned long collectionStart = 0;

const unsigned long COLLECTION_TIMEOUT = 15000;


// ============================================================
// RANDOM SENSOR A
// ============================================================

int readSensorA() {

    // Random sensor value
    return random(10, 500);
}


// ============================================================
// Change Mode Transmision
// ============================================================

bool changeTransmissionMode(E220OperationMode newMode) {

    // Already in requested mode
    if (currentMode == newMode) {
        return true;
    }

    Serial.println();
    Serial.println("========================================");
    Serial.println("CHANGING E220 TRANSMISSION MODE");

    if (newMode == MODE_FIXED) {
        Serial.println("NEW MODE: FIXED");
    } else {
        Serial.println("NEW MODE: TRANSPARENT");
    }

    Serial.println("========================================");


    // Read current E220 configuration
    ResponseStructContainer c = e220ttl.getConfiguration();

    if (c.status.code != 1) {

        Serial.println("FAILED TO READ CONFIG");
        Serial.println(c.status.getResponseDescription());

        return false;
    }


    Configuration configuration = *(Configuration *)c.data;


    // ========================================================
    // CHANGE TRANSMISSION MODE
    // ========================================================

    if (newMode == MODE_FIXED) {
        configuration.TRANSMISSION_MODE.fixedTransmission = FT_FIXED_TRANSMISSION;
    } else {
        configuration.TRANSMISSION_MODE.fixedTransmission = FT_TRANSPARENT_TRANSMISSION;
    }


    // ========================================================
    // WRITE NEW CONFIG TO E220
    // ========================================================

    ResponseStatus rs = e220ttl.setConfiguration(
        configuration,
        WRITE_CFG_PWR_DWN_LOSE
    );


    if (rs.code != 1) {

        Serial.println("FAILED TO WRITE CONFIG");
        Serial.println(rs.getResponseDescription());

        c.close();

        return false;
    }


    // Close configuration container
    c.close();


    // Save current software state
    currentMode = newMode;


    Serial.println("CONFIGURATION CHANGED SUCCESSFULLY");


    // Wait for E220 to stabilize
    delay(500);


    return true;
}


// ============================================================
// SEND FIXED MESSAGE TO NODE B
// ============================================================

void sendToNodeB(String message) {

    Serial.println();
    Serial.println("--------------------------------");
    Serial.println("NODE A -> NODE B");
    Serial.println("MODE    : FIXED");
    Serial.println("MESSAGE : " + message);
    Serial.println("--------------------------------");


    ResponseStatus rs = e220ttl.sendFixedMessage(
        NODE_B_ADDH,
        NODE_B_ADDL,
        CHANNEL,
        message
    );


    Serial.print("STATUS: ");
    Serial.println(rs.getResponseDescription());
}


// ============================================================
// SEND TRANSPARENT MESSAGE TO PI
// ============================================================

void sendToPi(String message) {

    if (!changeTransmissionMode(MODE_TRANSPARENT)) {
        Serial.println("FAILED CHANGE MODE TO TRANSPARENT");
        collecting = false;

        return;
    }

    Serial.println();
    Serial.println("--------------------------------");
    Serial.println("NODE A -> PI");
    Serial.println("MODE    : TRANSPARENT");
    Serial.println("MESSAGE : " + message);
    Serial.println("--------------------------------");

    ResponseStatus rs = e220ttl.sendMessage(message);


    Serial.print("STATUS: ");
    Serial.println(rs.getResponseDescription());
}


// ============================================================
// START NEW COLLECTION
//
// Example received from PI:
//
// CALL:001
// ============================================================

void startCollection(String requestId) {

    // --------------------------------------------------------
    // PREVENT OVERLAPPING REQUESTS
    // --------------------------------------------------------

    if (collecting) {

        Serial.println();
        Serial.println("WARNING: ALREADY COLLECTING");
        Serial.println("IGNORE NEW CALL");

        return;
    }


    Serial.println();
    Serial.println("========================================");
    Serial.println("NODE A START COLLECTION");
    Serial.println("REQUEST ID: " + requestId);
    Serial.println("========================================");


    // --------------------------------------------------------
    // RESET STATE
    // --------------------------------------------------------

    currentRequestId = requestId;

    collecting = true;

    gotB = false;
    gotC = false;

    dataA = "";
    dataB = "";
    dataC = "";

    collectionStart = millis();


    // --------------------------------------------------------
    // READ SENSOR A
    // --------------------------------------------------------

    int sensorValueA = readSensorA();


    dataA = String(sensorValueA);


    Serial.println();
    Serial.println("NODE A SENSOR DATA");
    Serial.println(dataA);


    // --------------------------------------------------------
    // FORWARD CALL TO NODE B
    // --------------------------------------------------------
    if (!changeTransmissionMode(MODE_FIXED)) {
      Serial.println("FAILED TO CHANGE TO FIXED MODE");
      collecting = false;
      return;
    }

    String callMessage =
        "CALL:" +
        currentRequestId;


    sendToNodeB(callMessage);
}


// ============================================================
// HANDLE DATA PACKET
//
// Expected:
//
// DATA:001:B:120
// DATA:001:C:250
// ============================================================

void handleData(String message) {

    // --------------------------------------------------------
    // MUST HAVE ACTIVE REQUEST
    // --------------------------------------------------------

    if (!collecting) {

        Serial.println();
        Serial.println("WARNING: NOT COLLECTING");
        Serial.println("IGNORE DATA: " + message);

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
    // DATA:001:B:120

    int p1 = message.indexOf(':');
    int p2 = message.indexOf(':', p1 + 1);
    int p3 = message.indexOf(':', p2 + 1);
    int p4 = message.indexOf(':', p3 + 1);
    int p5 = message.indexOf(':', p4 + 1);
    int p6 = message.indexOf(':', p5 + 1);
    int p7 = message.indexOf(':', p6 + 1);


    // Invalid packet

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

    String nodeB =
        message.substring(p2 + 1, p3);

    String valueB =
        message.substring(p3 + 1, p4);

    String nodeC =
        message.substring(p4 + 1, p5);

    String valueC =
        message.substring(p5 + 1, p6);


    // --------------------------------------------------------
    // VALIDATE PACKET TYPE
    // --------------------------------------------------------

    if (packetType != "DATA") {
        Serial.println("ERROR: UNKNOWN PACKET TYPE");
        return;
    }

    // --------------------------------------------------------
    // VALIDATE REQUEST ID
    // --------------------------------------------------------

    if (requestId != currentRequestId) {
        Serial.println();
        Serial.println("WARNING: WRONG REQUEST ID");

        Serial.println(
            "EXPECTED: " +
            currentRequestId
        );

        Serial.println(
            "RECEIVED: " +
            requestId
        );

        return;
    }


    // --------------------------------------------------------
    // NODE B DATA
    // --------------------------------------------------------

    if (nodeB == "B") {
        dataB = valueB;

        gotB = true;

        Serial.println();
        Serial.println("========================================");
        Serial.println("NODE A RECEIVED DATA FROM B");
        Serial.println("DATA: " + dataB);
        Serial.println("========================================");
    }

    // --------------------------------------------------------
    // NODE C DATA
    // --------------------------------------------------------

    if (nodeC == "C") {
        dataC = valueC;

        gotC = true;

        Serial.println();
        Serial.println("========================================");
        Serial.println("NODE A RECEIVED DATA FROM C");
        Serial.println("DATA: " + dataC);
        Serial.println("========================================");
    }

    // --------------------------------------------------------
    // UNKNOWN NODE
    // --------------------------------------------------------

    else {

        Serial.println();
        Serial.println("WARNING: UNKNOWN NODE");

        Serial.println(
            "NODE: " + nodeB
        );

        return;
    }


    // --------------------------------------------------------
    // CHECK IF COLLECTION COMPLETE
    // --------------------------------------------------------

    if (gotB && gotC) {
        finishCollection();
    }
}


// ============================================================
// FINISH COLLECTION
// ============================================================

void finishCollection() {

    Serial.println();
    Serial.println("========================================");
    Serial.println("ALL SENSOR DATA RECEIVED");
    Serial.println("========================================");

    Serial.println("A: " + dataA);
    Serial.println("B: " + dataB);
    Serial.println("C: " + dataC);

    // --------------------------------------------------------
    // CREATE FINAL PI PACKET
    // --------------------------------------------------------

    String finalMessage =
        "DATA:" +
        currentRequestId +
        ":A:" +
        dataA +
        ":B:" +
        dataB +
        ":C:" +
        dataC +
        "break";


    // Example:
    //
    // DATA:001:A:100:B:120:C:250


    Serial.println();
    Serial.println("FINAL MESSAGE FOR PI:");
    Serial.println(finalMessage);


    // --------------------------------------------------------
    // SEND TO PI
    // --------------------------------------------------------

    sendToPi(finalMessage);


    // --------------------------------------------------------
    // RESET COLLECTION
    // --------------------------------------------------------

    collecting = false;


    Serial.println();
    Serial.println("========================================");
    Serial.println("COLLECTION COMPLETE");
    Serial.println("NODE A RETURN TO IDLE");
    Serial.println("========================================");
}


// ============================================================
// RECEIVE LORA MESSAGE
// ============================================================

void receiveLoRa() {

    if (e220ttl.available() > 1) {

        ResponseContainer rc =
            e220ttl.receiveMessage();


        // ----------------------------------------------------
        // CHECK RECEIVE STATUS
        // ----------------------------------------------------

        if (rc.status.code != 1) {

            Serial.println();
            Serial.println("LORA RECEIVE ERROR:");

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
        Serial.println("NODE A RECEIVED");
        Serial.println("MESSAGE: " + message);
        Serial.println("========================================");


        // ----------------------------------------------------
        // CALL FROM PI
        //
        // CALL:001
        // ----------------------------------------------------

        if (message.startsWith("CALL:")) {

            String requestId =
                message.substring(5);

            requestId.trim();


            // Check empty ID

            if (requestId.length() == 0) {

                Serial.println(
                    "ERROR: EMPTY REQUEST ID"
                );

                return;
            }


            startCollection(requestId);

            return;
        }


        // ----------------------------------------------------
        // DATA FROM B OR C
        //
        // DATA:001:B:120
        // DATA:001:C:250
        // ----------------------------------------------------

        if (message.startsWith("DATA:")) {

            handleData(message);

            return;
        }


        // ----------------------------------------------------
        // UNKNOWN MESSAGE
        // ----------------------------------------------------

        Serial.println();
        Serial.println("UNKNOWN MESSAGE");
        Serial.println(message);
    }
}


// ============================================================
// COLLECTION TIMEOUT
// ============================================================

void checkCollectionTimeout() {

    if (!collecting) {
        return;
    }


    unsigned long elapsed =
        millis() - collectionStart;


    if (elapsed >= COLLECTION_TIMEOUT) {

        Serial.println();
        Serial.println("========================================");
        Serial.println("COLLECTION TIMEOUT");
        Serial.println("========================================");

        Serial.print("REQUEST ID: ");
        Serial.println(currentRequestId);

        Serial.print("GOT B: ");
        Serial.println(gotB ? "YES" : "NO");

        Serial.print("GOT C: ");
        Serial.println(gotC ? "YES" : "NO");


        String timeoutMessage =
            "DATA:" +
            currentRequestId +
            ":A:" +
            dataA +
            ":B:" +
            dataB +
            ":C:" +
            dataC +
            "break";


        Serial.println();
        Serial.println("SEND PARTIAL DATA TO PI:");
        Serial.println(timeoutMessage);

        sendToPi(timeoutMessage);

        collecting = false;

        Serial.println();
        Serial.println("NODE A RETURN TO IDLE");
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
    Serial.println("NODE A STARTING");
    Serial.println("ROLE: PI GATEWAY");
    Serial.println("PI -> A : TRANSPARENT");
    Serial.println("A -> B  : FIXED");
    Serial.println("B -> A  : FIXED");
    Serial.println("A -> PI : TRANSPARENT");
    Serial.println("========================================");

    randomSeed(
        micros() ^
        analogRead(34)
    );

    loraSerial.begin(
        9600,
        SERIAL_8N1,
        LORA_RX,
        LORA_TX
    );

    e220ttl.begin();

    // Make sure Node A starts in TRANSPARENT mode
    currentMode = MODE_FIXED;

    if (!changeTransmissionMode(MODE_TRANSPARENT)) {
        Serial.println("FATAL: CANNOT SET TRANSPARENT MODE");
    } else {

        Serial.println("NODE A READY");
        Serial.println("MODE: TRANSPARENT");
        Serial.println("WAITING FOR PI...");
    }
}


// ============================================================
// LOOP
// ============================================================

void loop() {
    receiveLoRa();
    checkCollectionTimeout();
    delay(10);
}