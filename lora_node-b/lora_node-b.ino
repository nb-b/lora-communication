#include <Arduino.h>
#include "LoRa_E22.h"

#define RXD2 16
#define TXD2 17

#define AUX_PIN 21
#define M0_PIN 22
#define M1_PIN 23

LoRa_E22 e22(&Serial2, AUX_PIN, M0_PIN, M1_PIN);

#define MY_ADDRESS 2
#define CHANNEL    23

struct Packet
{
    uint8_t source;
    uint8_t destination;
    uint8_t ttl;
    uint16_t sequence;
    char message[64];
};

struct Route
{
    uint8_t destination;
    uint8_t nextHopHigh;
    uint8_t nextHopLow;
    uint8_t channel;
};

/*
 * Routing Table
 *
 * Destination -> Next Hop
 *
 * Node A -> Node A
 * Node C -> Node C
 */
Route routes[] =
{
    {1, 0x00, 0x01, CHANNEL},
    {3, 0x00, 0x03, CHANNEL},
};

Route *findRoute(uint8_t destination)
{
    for (Route &r : routes)
    {
        if (r.destination == destination)
            return &r;
    }

    return nullptr;
}

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

    // Is this packet for Node B?
    if (packet.destination == MY_ADDRESS)
    {
        Serial.println("-------------------------------------");
        Serial.println("Message Delivered Successfully");
        Serial.print("From Node ");
        Serial.println(packet.source);

        Serial.print("Payload : ");
        Serial.println(packet.message);
        Serial.println("-------------------------------------");
        return;
    }

    // TTL expired?
    if (packet.ttl == 0)
    {
        Serial.println("Dropped (TTL expired)");
        return;
    }

    Route *route = findRoute(packet.destination);

    if (route == nullptr)
    {
        Serial.println("No route found.");
        return;
    }

    packet.ttl--;

    Serial.println("Packet is not for me.");
    Serial.println("Forwarding...");

    ResponseStatus rs = e22.sendFixedMessage(
        route->nextHopHigh,
        route->nextHopLow,
        route->channel,
        &packet,
        sizeof(Packet));

    Serial.println(rs.getResponseDescription());
}

void setup()
{
    Serial.begin(115200);

    Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);

    e22.begin();

    Serial.println("Node B Router Ready");
}

void loop()
{
    if (e22.available() > 1)
    {
        ResponseStructContainer rc = e22.receiveMessage(sizeof(Packet));

        if (rc.status.code == E22_SUCCESS)
        {
            Packet packet = *(Packet *)rc.data;

            receive(packet);
        }

        rc.close();
    }
}