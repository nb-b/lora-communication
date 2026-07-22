#include <Arduino.h>
#include "LoRa_E22.h"

#define RXD2 16
#define TXD2 17

#define AUX_PIN 21
#define M0_PIN 22
#define M1_PIN 23

LoRa_E22 e22(&Serial2, AUX_PIN, M0_PIN, M1_PIN);

#define MY_ADDRESS 3

struct Packet
{
    uint8_t source;
    uint8_t destination;
    uint8_t ttl;
    uint16_t sequence;
    char message[64];
};

uint16_t sequenceNumber = 0;

void sendPacket(uint8_t destination, const char *text)
{
    Packet packet;

    packet.source = MY_ADDRESS;
    packet.destination = destination;
    packet.ttl = 5;
    packet.sequence = sequenceNumber++;

    memset(packet.message, 0, sizeof(packet.message));
    strncpy(packet.message, text, sizeof(packet.message) - 1);

    // Next hop is always Node B
    ResponseStatus rs = e22.sendFixedMessage(
        0x00,       // ADDH of Node B
        0x02,       // ADDL of Node B
        23,         // Channel
        &packet,
        sizeof(packet));

    Serial.println(rs.getResponseDescription());

    Serial.println("---------------");
    Serial.print("To Node : ");
    Serial.println(destination);

    Serial.print("Via Node : 2");
    Serial.println();

    Serial.print("Sequence : ");
    Serial.println(packet.sequence);

    Serial.print("TTL : ");
    Serial.println(packet.ttl);

    Serial.print("Message : ");
    Serial.println(packet.message);

    Serial.println("---------------");
}

void setup()
{
    Serial.begin(115200);

    Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);

    e22.begin();

    Serial.println("Node C Ready");
    Serial.println("Type a message then press ENTER");
}

void loop()
{
    if (Serial.available())
    {
        String msg = Serial.readStringUntil('\n');
        msg.trim();

        if (msg.length())
        {
            sendPacket(1, msg.c_str());
        }
    }
}