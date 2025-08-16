/*
 * MeshRoof.hxx
 *
 * Copyright (C) 2025, Charles Chiou
 */

#ifndef MESHROOF_HXX
#define MESHROOF_HXX

#include <memory>
#include <SimpleClient.hxx>
#include <HomeChat.hxx>
#include <BaseNVM.hxx>
#include <EspWifi.hxx>

using namespace std;

struct nvm_header {
    uint32_t magic;
#define NVM_HEADER_MAGIC 0x6a87f421
} __attribute__((packed));

struct nvm_main_body {
    char wifi_ssid[32];
    char wifi_passwd[64];

    uint32_t ip;
    uint32_t netmask;
    uint32_t gateway;
    uint32_t dns1;
    uint32_t dns2;
    uint32_t dns3;

    char netif_passwd[32];

    uint32_t n_authchans;
    uint32_t n_admins;
    uint32_t n_mates;
} __attribute__((packed));

struct nvm_footer {
    uint32_t magic;
#define NVM_FOOTER_MAGIC 0xe8148afd
    uint32_t crc32;
} __attribute__((packed));

/*
 * Suitable for use on resource-constraint MCU platforms.
 */
class MeshRoof : public SimpleClient, public HomeChat, public BaseNVM,
                 public enable_shared_from_this<MeshRoof> {

public:

    MeshRoof();
    ~MeshRoof();

    inline shared_ptr<EspWifi> espWifi(void) {
        return EspWifi::getInstance();
    }

protected:

    virtual void gotTextMessage(const meshtastic_MeshPacket &packet,
                                const string &message);
    virtual void gotTelemetry(const meshtastic_MeshPacket &packet,
                              const meshtastic_Telemetry &telemetry);
    virtual void gotRouting(const meshtastic_MeshPacket &packet,
                            const meshtastic_Routing &routing);
    virtual void gotTraceRoute(const meshtastic_MeshPacket &packet,
                               const meshtastic_RouteDiscovery &routeDiscovery);

protected:

    virtual int vprintf(const char *format, va_list ap) const;

public:

    string getWifiSsid(void) const;
    string getWifiPasswd(void) const;
    string getIpString(void) const;
    string getNetmaskString(void) const;
    string getGatewayString(void) const;
    string getDns1String(void) const;
    string getDns2String(void) const;
    string getDns3String(void) const;
    uint32_t getIp(void) const;
    uint32_t getNetmask(void) const;
    uint32_t getGateway(void) const;
    uint32_t getDns1(void) const;
    uint32_t getDns2(void) const;
    uint32_t getDns3(void) const;
    string getNetIfPassword(void) const;

    bool setWifiSsid(const string &ssid);
    bool setWifiPasswd(const string &passwd);
    bool setIp(const string &addr);
    bool setNetmask(const string &addr);
    bool setGateway(const string &addr);
    bool setDns1(const string &addr);
    bool setDns2(const string &addr);
    bool setDns3(const string &addr);
    bool setNetIfPasswd(const string &passwd);

    virtual bool loadNvm(void);
    virtual bool saveNvm(void);
    bool applyNvmToHomeChat(void);

private:

    struct nvm_main_body _main_body;
};

#endif

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
