#include <Arduino.h>
#include "LoRa_E220.h"

#define RXD2 16
#define TXD2 17

#define AUX_PIN 21
#define M0_PIN 22
#define M1_PIN 23

LoRa_E220 e220ttl(&Serial2, AUX_PIN, M0_PIN, M1_PIN);

#define MY_ADDRESS 1

struct Packet
{
    uint8_t source;
    uint8_t destination;
    uint8_t ttl;
    uint16_t sequence;
    char message[64];
};

void receive(Packet &packet)
{
    Serial.println("\n========== PACKET ==========");

    Serial.print("Source      : ");
    Serial.println(packet.source);

    Serial.print("Destination : ");
    Serial.println(packet.destination);

    Serial.print("TTL         : ");
    Serial.println(packet.ttl);

    Serial.print("Sequence    : ");
    Serial.println(packet.sequence);

    if (packet.destination != MY_ADDRESS)
    {
        Serial.println("Packet is not for me.");
        return;
    }

    Serial.println("-------------------------------------");
    Serial.println("Message Delivered Successfully");
    Serial.print("From Node ");
    Serial.println(packet.source);

    Serial.print("Payload : ");
    Serial.println(packet.message);
    Serial.println("-------------------------------------");
}

void setup()
{
    Serial.begin(115200);

    Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);

    e220ttl.begin();

    Serial.println("Node A Ready");
}

void loop()
{
    if (e220ttl.available() > 1)
    {
        ResponseStructContainer rc = e220ttl.receiveMessage(sizeof(Packet));

        if (rc.status.code == E220_SUCCESS)
        {
            Packet packet = *(Packet *)rc.data;

            receive(packet);
        }

        rc.close();
    }
}