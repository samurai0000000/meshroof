/*
 * MeshRoofShell.cxx
 *
 * Copyright (C) 2025, Charles Chiou
 */

#include <esp_timer.h>
#include <esp_system.h>
#include <esp_heap_caps.h>
#include <lwip/ip_addr.h>
#include <lwip/inet.h>
#include <lwip/netdb.h>
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
    _help_list.push_back("wifi");
    _help_list.push_back("net");
    _help_list.push_back("ping");
    _help_list.push_back("amplify");
    _help_list.push_back("buzz");
    _help_list.push_back("morse");
    _help_list.push_back("reset");
}

MeshRoofShell::~MeshRoofShell()
{

}

int MeshRoofShell::tx_write(const uint8_t *buf, size_t size)
{
    int ret = 0;
    uint32_t ctx = (uint32_t) _ctx;

    if (ctx == 1) {
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
    char *buf = NULL;

    if (ctx == 1) {
        va_start(ap, format);
        ret = usb_vprintf(format, ap);
        va_end(ap);
    } else {
        int tcp_fd = (ctx & 0x7fffffff);
        char *s = NULL;
        int len = 0;

        buf = (char *) malloc(512);
        if (buf == NULL) {
            goto done;
        }

        va_start(ap, format);
        len = vsnprintf(buf, 512 - 1, format, ap);
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

done:

    if (buf) {
        free(buf);
    }

    return ret;
}

int MeshRoofShell::rx_ready(void) const
{
    int ret = 0;
    uint32_t ctx = (uint32_t) _ctx;

    if (ctx == 1) {
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

    if (ctx == 1) {
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
    char cTaskListBuffer[1024];

    SimpleShell::system(argc, argv);
    this->printf("Total Heap: %zu\n", total_heap);
    this->printf(" Free Heap: %zu\n", free_heap);
    this->printf(" Used Heap: %zu\n", used_heap);
    this->printf("  CPU Temp: %.1fC\n", meshroof->getCpuTempC());
    bzero(cTaskListBuffer, sizeof(cTaskListBuffer));
    vTaskList(cTaskListBuffer);
    this->printf("  FreeRTOS:\n");
    this->printf("Name        State  Priority  StackRem   Task#   CPU Affn\n");
    this->printf("--------------------------------------------------------\n");
    this->printf("%s", cTaskListBuffer);

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

    if (ctx != 1) {
        int tcp_fd = (ctx & 0x7fffffff);
        close(tcp_fd);
    }

    return 0;
}

int MeshRoofShell::wifi(int argc, char **argv)
{
    int ret = 0;
    shared_ptr<EspWifi> wifi = meshroof->espWifi();

    if (argc == 1) {
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
    } else if ((argc == 2) && (strcmp(argv[1], "nvm") == 0)) {
        this->printf("ssid: %s\n", meshroof->getWifiSsid().c_str());
        this->printf("passwd: %s\n", meshroof->getWifiPasswd().c_str());
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
        const esp_netif_ip_info_t *ip_info =
            meshroof->espWifi()->getIpInfo();
        const esp_netif_dns_info_t *dns1_info =
            meshroof->espWifi()->getDns1Info();
        const esp_netif_dns_info_t *dns2_info =
            meshroof->espWifi()->getDns2Info();
        const esp_netif_dns_info_t *dns3_info =
            meshroof->espWifi()->getDns3Info();

        if (meshroof->getIp() == 0) {
            this->printf("(dhcp)\n");
        } else {
            this->printf("(static ip)\n");
        }
        this->printf("ip:      " IPSTR "\n", IP2STR(&ip_info->ip));
        this->printf("netmask: " IPSTR "\n", IP2STR(&ip_info->netmask));
        this->printf("gateway: " IPSTR "\n", IP2STR(&ip_info->gw));
        this->printf("dns1:    " IPSTR "\n", IP2STR(&dns1_info->ip.u_addr.ip4));
        this->printf("dns2:    " IPSTR "\n", IP2STR(&dns2_info->ip.u_addr.ip4));
        this->printf("dns3:    " IPSTR "\n", IP2STR(&dns3_info->ip.u_addr.ip4));
    } else if ((argc == 2) && (strcmp(argv[1], "nvm") == 0)) {
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

void MeshRoofShell::on_ping_success(esp_ping_handle_t hdl, void *args)
{
    MeshRoofShell *mrs = (MeshRoofShell *) args;
    uint8_t ttl;
    uint16_t seqno;
    uint32_t elapsed_time, recv_len;
    esp_ip_addr_t target_addr;

    esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
    esp_ping_get_profile(hdl, ESP_PING_PROF_TTL, &ttl, sizeof(ttl));
    esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR,
                         &target_addr, sizeof(target_addr));
    esp_ping_get_profile(hdl, ESP_PING_PROF_SIZE, &recv_len, sizeof(recv_len));
    esp_ping_get_profile(hdl, ESP_PING_PROF_TIMEGAP,
                         &elapsed_time, sizeof(elapsed_time));
    mrs->printf("%d bytes from %s icmp_seq=%d ttl=%d time=%d ms\n",
                recv_len, inet_ntoa(target_addr.u_addr.ip4), seqno,
                ttl, elapsed_time);
}

void MeshRoofShell::on_ping_timeout(esp_ping_handle_t hdl, void *args)
{
    MeshRoofShell *mrs = (MeshRoofShell *) args;
    uint16_t seqno = 0;
    esp_ip_addr_t target_addr;

    esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
    esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR,
                         &target_addr, sizeof(target_addr));
    mrs->printf("From %s icmp_seq=%d timeout\n",
                inet_ntoa(target_addr.u_addr.ip4), seqno);
}

void MeshRoofShell::on_ping_end(esp_ping_handle_t hdl, void *args)
{
    MeshRoofShell *mrs = (MeshRoofShell *) args;
    uint32_t transmitted;
    uint32_t received;
    uint32_t total_time_ms;

    esp_ping_get_profile(hdl, ESP_PING_PROF_REQUEST,
                         &transmitted, sizeof(transmitted));
    esp_ping_get_profile(hdl, ESP_PING_PROF_REPLY,
                         &received, sizeof(received));
    esp_ping_get_profile(hdl, ESP_PING_PROF_DURATION,
                         &total_time_ms, sizeof(total_time_ms));
    mrs->printf("%d packets transmitted, %d received, time %dms\n",
                transmitted, received, total_time_ms);
}

int MeshRoofShell::ping(int argc, char **argv)
{
    int ret = 0;
    ip_addr_t target_addr;
    struct addrinfo hint;
    struct addrinfo *res = NULL;
    struct in_addr addr4;
    esp_ping_config_t ping_config;
    esp_ping_callbacks_t cbs;
    esp_ping_handle_t hdl = NULL;

    if (argc == 1) {
        goto done;
    } else if (argc > 3) {
        ret = -1;
        this->printf("syntax error!\n");
        goto done;
    }

    memset(&hint, 0, sizeof(hint));
    memset(&target_addr, 0, sizeof(target_addr));
    ret = getaddrinfo(argv[1], NULL, &hint, &res);
    if (ret != 0) {
        this->printf("cannot resolve %s!\n", argv[1]);
        goto done;
    }

    addr4 = ((struct sockaddr_in *) (res->ai_addr))->sin_addr;
    inet_addr_to_ip4addr(ip_2_ip4(&target_addr), &addr4);
    if (res) {
        freeaddrinfo(res);
        res = NULL;
    }

    ping_config = ESP_PING_DEFAULT_CONFIG();
    ping_config.target_addr = target_addr;
    ping_config.count = ESP_PING_COUNT_INFINITE;

    cbs.on_ping_success = on_ping_success;
    cbs.on_ping_timeout = on_ping_timeout;
    cbs.on_ping_end = on_ping_end;
    cbs.cb_args = this;

    ret = esp_ping_new_session(&ping_config, &cbs, &hdl);
    if (ret != ESP_OK) {
        ret = -1;
        this->printf("esp_ping_new_session failed!\n");
        goto done;
    }

    ret = esp_ping_start(hdl);
    if (ret != ESP_OK) {
        ret = -1;
        this->printf("esp_ping_start failed!\n");
        goto done;
    }

    for (;;) {
        char c;

        ret = this->rx_read((uint8_t *) &c, 1);
        if (ret == -1) {
            break;
        }

        if (c == 0xff) {  // IAC received
            static const uint8_t iac_do_tm[3] = { 0xff, 0xfd, 0x06};
            static const uint8_t iac_will_tm[3] = { 0xff, 0xfb, 0x06};
            char iac2;

            ret = this->rx_read((uint8_t *) &iac2, 1);
            if (ret == 1) {
                switch (iac2) {
                case 0xf4:  // IAC IP (interrupt process)
                    ret = this->tx_write(iac_do_tm, sizeof(iac_do_tm));
                    ret = this->tx_write(iac_will_tm, sizeof(iac_will_tm));
                    this->printf("\n> ");
                    _inproc.i = 0;
                    break;
                default:
                    break;
                }
            }

            break;
        }

        if (c == '\x03') {
            break;
        }
    }

    esp_ping_stop(hdl);

    ret = 0;

done:

    if (hdl) {
        vTaskDelay(pdMS_TO_TICKS(3000));
        esp_ping_delete_session(hdl);
    }

    return ret;
}

int MeshRoofShell::amplify(int argc, char **argv)
{
    int ret = 0;

    if (argc == 1) {
        this->printf("amplify: %s\n", meshroof->isAmplifying() ? "on" : "off");
    } else if ((argc == 2) && (strcmp(argv[1], "on") == 0)) {
        meshroof->amplify(true);
        this->printf("amplify on\n");
    } else if ((argc == 2) && (strcmp(argv[1], "off") == 0)) {
        meshroof->amplify(false);
        this->printf("amplify off\n");
    } else {
        this->printf("syntax error!\n");
        ret = -1;
    }

    return ret;
}

int MeshRoofShell::buzz(int argc, char **argv)
{
    if (argc == 1) {
        meshroof->buzz();
    } else if ((argc == 2)) {
        unsigned int ms;

        try {
            ms = stoul(argv[1]);
            meshroof->buzz(ms);
        } catch (const invalid_argument &e) {
            this->printf("syntax error!\n");
        }
    } else {
        this->printf("syntax error!\n");
    }

    return -1;
}

int MeshRoofShell::morse(int argc, char **argv)
{
    string text;

    for (int i = 1; i < argc; i++) {
        text += argv[i];
    }

    meshroof->addMorseText(text);

    return 0;
}

int MeshRoofShell::reset(int argc, char **argv)
{
    int ret = 0;

    if (argc == 1) {
        time_t now, last;
        unsigned int secs_ago = 0;

        last = meshroof->getLastReset();
        now = time(NULL);
        secs_ago = now - last;

        this->printf("reset count: %u\n", meshroof->getResetCount());
        if (secs_ago != 0) {
            this->printf("last reset: %u seconds ago\n", secs_ago);
        }
    } else if ((argc == 2) && strcmp(argv[1], "apply") == 0) {
        meshroof->reset();
    } else {
        this->printf("syntax error!\n");
        ret = -1;
    }

    return ret;
}

int MeshRoofShell::unknown_command(int argc, char **argv)
{
    int ret = 0;

    if (strcmp(argv[0], "exit") == 0) {
        ret = this->exit(argc, argv);
    } else if (strcmp(argv[0], "wifi") == 0) {
        ret = this->wifi(argc, argv);
    } else if (strcmp(argv[0], "net") == 0) {
        ret = this->net(argc, argv);
    } else if (strcmp(argv[0], "ping") == 0) {
        ret = this->ping(argc, argv);
    } else if (strcmp(argv[0], "amplify") == 0) {
        ret = this->amplify(argc, argv);
    } else if (strcmp(argv[0], "buzz") == 0) {
        ret = this->buzz(argc, argv);
    } else if (strcmp(argv[0], "morse") == 0) {
        ret = this->morse(argc, argv);
    } else if (strcmp(argv[0], "reset") == 0) {
        ret = this->reset(argc, argv);
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
