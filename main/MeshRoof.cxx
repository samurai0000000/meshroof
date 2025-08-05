/*
 * MeshRoof.cxx
 *
 * Copyright (C) 2025, Charles Chiou
 */

#include <stddef.h>
#include <algorithm>
#include <meshroof.h>
#include <MeshRoof.hxx>

MeshRoof::MeshRoof()
    : SimpleClient()
{

}

MeshRoof::~MeshRoof()
{

}

void MeshRoof::gotTextMessage(const meshtastic_MeshPacket &packet,
                              const string &message)
{
    SimpleClient::gotTextMessage(packet, message);
    string reply;
    bool result = false;

    if (packet.to == whoami()) {
        usb_printf("%s: %s\n",
                   getDisplayName(packet.from).c_str(), message.c_str());

        reply = lookupShortName(packet.from) + ", you said '" + message + "'!";
        if (reply.size() > 200) {
            reply = "oopsie daisie!";
        }

        result = textMessage(packet.from, packet.channel, reply);
        if (result == false) {
            usb_printf("textMessage '%s' failed!\n",
                       reply.c_str());
        } else {
            usb_printf("my_reply to %s: %s\n",
                       getDisplayName(packet.from).c_str(),
                       reply.c_str());
        }
    } else {
        usb_printf("%s on #%s: %s\n",
                   getDisplayName(packet.from).c_str(),
                   getChannelName(packet.channel).c_str(),
                   message.c_str());
        if ((packet.channel == 0) || (packet.channel == 1)) {
            string msg = message;
            transform(msg.begin(), msg.end(), msg.begin(),
                      [](unsigned char c) {
                          return tolower(c); });
            if (msg.find("hello") != string::npos) {
                reply = "greetings, " + lookupShortName(packet.from) + "!";
            } else if (msg.find(lookupShortName(whoami())) != string::npos) {
                reply = lookupShortName(packet.from) + ", you said '" +
                    message + "'!";
                if (reply.size() > 200) {
                    reply = "oopsie daisie!";
                }
            }

            if (!reply.empty()) {
                result = textMessage(0xffffffffU, packet.channel, reply);
                if (result == false) {
                    usb_printf("textMessage '%s' failed!\n",
                               reply.c_str());
                } else {
                    usb_printf("my reply to %s: %s\n",
                               getDisplayName(packet.from).c_str(),
                               reply.c_str());
                }
            }
        }
    }
}

void MeshRoof::gotTelemetry(const meshtastic_MeshPacket &packet,
                            const meshtastic_Telemetry &telemetry)
{
    SimpleClient::gotTelemetry(packet, telemetry);
}

void MeshRoof::gotRouting(const meshtastic_MeshPacket &packet,
                          const meshtastic_Routing &routing)
{
    SimpleClient::gotRouting(packet, routing);
}

void MeshRoof::gotTraceRoute(const meshtastic_MeshPacket &packet,
                             const meshtastic_RouteDiscovery &routeDiscovery)
{
    SimpleClient::gotTraceRoute(packet, routeDiscovery);
}

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
