/*
 * EspWifi.hxx
 *
 * Copyright (C) 2025, Charles Chiou
 */

#ifndef WIFI_HXX
#define WIFI_HXX

#include <esp_wifi.h>
#include <esp_event.h>
#include <memory>

using namespace std;

class EspWifi {

public:

    static shared_ptr<EspWifi> getInstance(void);

    int start(void);
    int stop(void);

    inline const char *ssid(void) const {
        return (const char *) _config.sta.ssid;
    }

    inline string passwd(void) const {
        return (const char *) _config.sta.password;
    }

    int getRssi(void) const;
    inline const wifi_event_sta_connected_t *getStaConnected(void) const {
        return &_sta_connected;
    }
    inline const esp_netif_ip_info_t *getIpInfo(void) const {
        return &_ip_info;
    }
    inline const esp_netif_dns_info_t *getDns1Info(void) const {
        return &_dns1_info;
    }
    inline const esp_netif_dns_info_t *getDns2Info(void) const {
        return &_dns2_info;
    }
    inline const esp_netif_dns_info_t *getDns3Info(void) const {
        return &_dns3_info;
    }

    void applyNetIf(void);

private:

 	EspWifi();
    ~EspWifi();

    void resetStatus(void);

    static void espEventHandler(void *arg, esp_event_base_t event_base,
                                int32_t event_id, void *event_data);
    void gotStaStart(const void *e);
    void gotStaStop(const void *e);
    void gotStaDisconnected(const void *e);
    void gotHomeChannelChange(const void *e);
    void gotStaConnected(const void *e);

    void gotStaGotIp(const void *e);

    bool _started;
    esp_netif_t *_sta_netif;
    wifi_init_config_t _init_config;
    wifi_config_t _config;
    wifi_event_sta_connected_t _sta_connected;
    esp_netif_ip_info_t _ip_info;
    esp_netif_dns_info_t _dns1_info;
    esp_netif_dns_info_t _dns2_info;
    esp_netif_dns_info_t _dns3_info;

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
