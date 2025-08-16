/*
 * MeshRoofShell.cxx
 *
 * Copyright (C) 2025, Charles Chiou
 */

#include <esp_timer.h>
#include <esp_system.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <libmeshtastic.h>
#include <serial.h>
#include <MeshRoof.hxx>
#include <MeshRoofShell.hxx>

extern shared_ptr<MeshRoof> meshroof;

MeshRoofShell::MeshRoofShell(shared_ptr<SimpleClient> client)
    : SimpleShell(client)
{
    _help_list.push_back("exit");
    _help_list.push_back("wcfg");
    _help_list.push_back("disc");
    _help_list.push_back("hb");
    _help_list.push_back("wifi");
    _help_list.push_back("net");
    _help_list.push_back("ping");
}

MeshRoofShell::~MeshRoofShell()
{

}

int MeshRoofShell::tx_write(const uint8_t *buf, size_t size)
{
    int ret = 0;
    uint32_t ctx = (uint32_t) _ctx;

    if (ctx == 0) {
        ret = usb_tx_write(buf, size);
    } else {
        int tcp_fd = (ctx & 0x7fffffff);
        ret = write(tcp_fd, buf, size);
    }

    return ret;
}

int MeshRoofShell::printf(const char *format, ...)
{
    int ret = 0;
    uint32_t ctx = (uint32_t) _ctx;
    va_list ap;

    if (ctx == 0) {
        va_start(ap, format);
        ret = usb_vprintf(format, ap);
        va_end(ap);
    } else {
        int tcp_fd = (ctx & 0x7fffffff);
        char buf[512];
        char *s = NULL;
        int len = 0;

        va_start(ap, format);
        len = vsnprintf(buf, sizeof(buf) - 1, format, ap);
        va_end(ap);

        s = buf;
        while (len > 0) {
            ret = write(tcp_fd, s, len);
            if (ret <= 0) {
                ret = -1;
                break;
            }

            len -= ret;
            s += len;
        }
    }

    return ret;
}

int MeshRoofShell::rx_ready(void) const
{
    int ret = 0;
    uint32_t ctx = (uint32_t) _ctx;

    if (ctx == 0) {
        ret = usb_rx_ready();
    } else {
        ret = 1;
    }

    return ret;
}

int MeshRoofShell::rx_read(uint8_t *buf, size_t size)
{
    int ret = 0;
    uint32_t ctx = (uint32_t) _ctx;

    if (ctx == 0) {
        ret = usb_rx_read_timeout(buf, size, pdMS_TO_TICKS(100));
    } else {
        int tcp_fd = (ctx & 0x7fffffff);
        fd_set rfds;
        struct timeval timeout = {
            .tv_sec = 0,
            .tv_usec = 500000,
        };

        FD_ZERO(&rfds);
        FD_SET(tcp_fd, &rfds);

        ret = select(tcp_fd + 1, &rfds, NULL, NULL, &timeout);

        if (ret > 0) {
            ret = read(tcp_fd, buf, size);
        }
    }

    return ret;
}

int MeshRoofShell::system(int argc, char **argv)
{
    size_t total_heap = heap_caps_get_total_size(MALLOC_CAP_INTERNAL);
    size_t free_heap = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    size_t used_heap = total_heap - free_heap;

    SimpleShell::system(argc, argv);
    this->printf("Total Heap: %zu\n", total_heap);
    this->printf(" Free Heap: %zu\n", free_heap);
    this->printf(" Used Heap: %zu\n", used_heap);

    return 0;
}

int MeshRoofShell::reboot(int argc, char **argv)
{
    (void)(argc);
    (void)(argv);

    meshroof->sendDisconnect();
    esp_restart();
    for (;;);

    return 0;
}

int MeshRoofShell::nvm(int argc, char **argv)
{
    wifi(argc, argv);
    net(argc, argv);
    SimpleShell::nvm(argc, argv);

    return 0;
}

int MeshRoofShell::exit(int argc, char **argv)
{
    uint32_t ctx = (uint32_t) _ctx;

    if (ctx != 0) {
        int tcp_fd = (ctx & 0x7fffffff);
        close(tcp_fd);
    }

    return 0;
}

int MeshRoofShell::wcfg(int argc, char **argv)
{
    int ret = 0;

    (void)(argc);
    (void)(argv);

    if (meshroof->sendWantConfig() != true) {
        this->printf("failed!\n");
        ret = -1;
    }

    return ret;
}

int MeshRoofShell::disc(int argc, char **argv)
{
   int ret = 0;

    (void)(argc);
    (void)(argv);

    if (meshroof->sendDisconnect() != true) {
        this->printf("failed!\n");
        ret = -1;
    }

    return ret;
}

int MeshRoofShell::hb(int argc, char **argv)
{
   int ret = 0;

    (void)(argc);
    (void)(argv);

    if (meshroof->sendHeartbeat() != true) {
        this->printf("failed!\n");
        ret = -1;
    }

    return ret;
}

int MeshRoofShell::wifi(int argc, char **argv)
{
    int ret = 0;
    shared_ptr<EspWifi> wifi = meshroof->espWifi();

    if (argc == 1) {
        this->printf("ssid: %s\n", meshroof->getWifiSsid().c_str());
        this->printf("passwd: %s\n", meshroof->getWifiPasswd().c_str());
    } else if ((argc == 2) && (strcmp(argv[1], "status") == 0)) {
        const wifi_event_sta_connected_t *sta_connected =
            meshroof->espWifi()->getStaConnected();
        if (sta_connected->bssid[0] == 0x0) {
            this->printf("Wifi not connected\n");
        } else {
            char ssid[40];

            bzero(ssid, sizeof(ssid));
            memcpy(ssid, sta_connected->ssid,
                   min((size_t) sta_connected->ssid_len, sizeof(ssid)));
            this->printf("ssid: %s\n", ssid);
            this->printf("bssid: %.2x:%.2x:%.2x:%.2x:%.2x:%.2x\n",
                         sta_connected->bssid[0],
                         sta_connected->bssid[1],
                         sta_connected->bssid[2],
                         sta_connected->bssid[3],
                         sta_connected->bssid[4],
                         sta_connected->bssid[5]);
            this->printf("channel: %d\n", (int) sta_connected->channel);
            this->printf("rssi: %d\n", meshroof->espWifi()->getRssi());
        }
    } else if ((argc == 2) && (strcmp(argv[1], "stop") == 0)) {
        meshroof->espWifi()->stop();
    } else if ((argc == 2) && (strcmp(argv[1], "start") == 0)) {
        meshroof->espWifi()->start();
    } else if ((argc == 2) && (strcmp(argv[1], "apply") == 0)) {
        meshroof->espWifi()->stop();
        meshroof->espWifi()->start();
    } else if ((argc == 3) && (strcmp(argv[1], "ssid") == 0)) {
        if ((meshroof->setWifiSsid(argv[2]) == true) &&
            (meshroof->saveNvm() == true)) {
            this->printf("ok\n");
        } else {
            this->printf("failed!\n");
            ret = -1;
        }
    } else if ((argc == 3) && (strcmp(argv[1], "passwd") == 0)) {
        if ((meshroof->setWifiPasswd(argv[2]) == true) &&
            (meshroof->saveNvm() == true)) {
            this->printf("ok\n");
        } else {
            this->printf("failed!\n");
            ret = -1;
        }
    } else {
        this->printf("syntax error!\n");
    }

    return ret;
}

int MeshRoofShell::net(int argc, char **argv)
{
    int ret = 0;

    if (argc == 1) {
        if (meshroof->getIp() == 0) {
            this->printf("ip: dhcp\n");
        } else {
            this->printf("ip:      %s\n", meshroof->getIpString().c_str());
            this->printf("netmask: %s\n", meshroof->getNetmaskString().c_str());
            this->printf("gateway: %s\n", meshroof->getGatewayString().c_str());
            this->printf("dns1:    %s\n", meshroof->getDns1String().c_str());
            this->printf("dns2:    %s\n", meshroof->getDns2String().c_str());
            this->printf("dns3:    %s\n", meshroof->getDns3String().c_str());
        }
    } else if ((argc == 2) && (strcmp(argv[1], "status") == 0)) {
        const esp_netif_ip_info_t *ip_info =
            meshroof->espWifi()->getIpInfo();
        const esp_netif_dns_info_t *dns1_info =
            meshroof->espWifi()->getDns1Info();
        const esp_netif_dns_info_t *dns2_info =
            meshroof->espWifi()->getDns2Info();
        const esp_netif_dns_info_t *dns3_info =
            meshroof->espWifi()->getDns3Info();

        this->printf("ip:      " IPSTR "\n", IP2STR(&ip_info->ip));
        this->printf("netmask: " IPSTR "\n", IP2STR(&ip_info->netmask));
        this->printf("gateway: " IPSTR "\n", IP2STR(&ip_info->gw));
        this->printf("dns1:    " IPSTR "\n", IP2STR(&dns1_info->ip.u_addr.ip4));
        this->printf("dns2:    " IPSTR "\n", IP2STR(&dns2_info->ip.u_addr.ip4));
        this->printf("dns3:    " IPSTR "\n", IP2STR(&dns3_info->ip.u_addr.ip4));
    } else if ((argc == 2) && (strcmp(argv[1], "apply") == 0)) {
        meshroof->espWifi()->applyNetIf();
        this->printf("ok\n");
    } else if ((argc == 7) &&
               (strcmp(argv[1], "ip") == 0) &&
               (strcmp(argv[3], "netmask") == 0) &&
               ((strcmp(argv[5], "gateway") == 0) ||
                (strcmp(argv[5], "gw") == 0))) {
        if ((meshroof->setIp(argv[2]) == true) &&
            (meshroof->setNetmask(argv[4]) == true) &&
            (meshroof->setGateway(argv[6]) == true) &&
            (meshroof->saveNvm() == true)) {
            meshroof->espWifi()->applyNetIf();
            this->printf("ok\n");
        } else {
            this->printf("failed\n");
        }
    } else if ((argc == 3) && (strcmp(argv[1], "ip") == 0) &&
               ((strcmp(argv[2], "dhcp") == 0))) {
        if ((meshroof->setIp("0.0.0.0") == true) &&
            (meshroof->saveNvm() == true)) {
            meshroof->espWifi()->applyNetIf();
            this->printf("ok\n");
        } else {
            this->printf("failed\n");
        }
    } else if ((argc == 3) && (strcmp(argv[1], "ip") == 0)) {
        if ((meshroof->setIp(argv[2]) == true) &&
            (meshroof->saveNvm() == true)) {
            this->printf("ok\n");
        } else {
            this->printf("failed\n");
        }
    } else if ((argc == 3) && (strcmp(argv[1], "netmask") == 0)) {
        if ((meshroof->setNetmask(argv[2]) == true) &&
            (meshroof->saveNvm() == true)) {
            this->printf("ok\n");
        } else {
            this->printf("failed\n");
        }
    } else if ((argc == 3) && (strcmp(argv[1], "gateway") == 0)) {
        if ((meshroof->setGateway(argv[2]) == true) &&
            (meshroof->saveNvm() == true)) {
            this->printf("ok\n");
        } else {
            this->printf("failed\n");
        }
    } else if ((argc == 3) && (strcmp(argv[1], "dns1") == 0)) {
        if ((meshroof->setDns1(argv[2]) == true) &&
            (meshroof->saveNvm() == true)) {
            this->printf("ok\n");
        } else {
            this->printf("failed\n");
        }
    } else if ((argc == 3) && (strcmp(argv[1], "dns2") == 0)) {
        if ((meshroof->setDns2(argv[2]) == true) &&
            (meshroof->saveNvm() == true)) {
            this->printf("ok\n");
        } else {
            this->printf("failed\n");
        }
    } else if ((argc == 3) && (strcmp(argv[1], "dns3") == 0)) {
        if ((meshroof->setDns3(argv[2]) == true) &&
            (meshroof->saveNvm() == true)) {
            this->printf("ok\n");
        } else {
            this->printf("failed\n");
        }
    } else {
        this->printf("syntax error!\n");
    }

    return ret;
}

int MeshRoofShell::ping(int argc, char **argv)
{
    int ret = 0;

    (void)(argc);
    (void)(argv);

    return ret;
}

int MeshRoofShell::unknown_command(int argc, char **argv)
{
    int ret = 0;

    if (strcmp(argv[0], "exit") == 0) {
        ret = this->exit(argc, argv);
    } else if (strcmp(argv[0], "wcfg") == 0) {
        ret = this->wcfg(argc, argv);
    } else if (strcmp(argv[0], "disc") == 0) {
        ret = this->disc(argc, argv);
    } else if (strcmp(argv[0], "hb") == 0) {
        ret = this->hb(argc, argv);
    } else if (strcmp(argv[0], "wifi") == 0) {
        ret = this->wifi(argc, argv);
    } else if (strcmp(argv[0], "net") == 0) {
        ret = this->net(argc, argv);
    } else if (strcmp(argv[0], "ping") == 0) {
        ret = this->ping(argc, argv);
    } else {
        this->printf("Unknown command '%s'!\n", argv[0]);
        ret = -1;
    }

    return ret;
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
