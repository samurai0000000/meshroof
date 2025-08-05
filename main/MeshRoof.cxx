/*
 * MeshRoof.cxx
 *
 * Copyright (C) 2025, Charles Chiou
 */

#include <stddef.h>
#include <algorithm>
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
