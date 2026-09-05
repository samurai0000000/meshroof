/*
 * MeshRoof.cxx
 *
 * Copyright (C) 2025, Charles Chiou
 */

#include <stddef.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <esp_log.h>
#include <esp_rom_sys.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <ping/ping_sock.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <esp_crc.h>
#include <driver/gpio.h>
#include <driver/temperature_sensor.h>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <meshroof.h>
#include <MeshRoof.hxx>

static const char *TAG = "MeshRoof";

MeshRoof::MeshRoof()
    : SimpleClient(), HomeChat(), BaseNvm(), MorseBuzzer()
{
    bzero(&_main_body, sizeof(_main_body));
    _isAmplifying = false;
    _amplifierGain = "high";
    _resetCount = 0;
    _lastReset = time(NULL);

    gpio_reset_pin(AMPLIFY_PIN);
    gpio_set_direction(AMPLIFY_PIN, GPIO_MODE_OUTPUT);
    gpio_reset_pin(SWITCH_PIN);
    gpio_set_direction(SWITCH_PIN, GPIO_MODE_OUTPUT);
    amplify(false);

    gpio_reset_pin(OUTRESET_PIN);
    gpio_set_direction(OUTRESET_PIN, GPIO_MODE_OUTPUT);
    reset();

    gpio_reset_pin(BUZZER_PIN);
    gpio_set_direction(BUZZER_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(BUZZER_PIN, false);

    gpio_reset_pin(ONBOARD_LED_PIN);
    gpio_set_direction(ONBOARD_LED_PIN, GPIO_MODE_OUTPUT);
    setOnboardLed(false);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
    temperature_sensor_config_t cfg =
        TEMPERATURE_SENSOR_CONFIG_DEFAULT(-10, 80);
#pragma GCC diagnostic pop

    _esp_temp_handle = NULL;
    ESP_ERROR_CHECK(temperature_sensor_install(&cfg,
                                               (temperature_sensor_handle_t *)
                                               &_esp_temp_handle));
    ESP_ERROR_CHECK(temperature_sensor_enable((temperature_sensor_handle_t)
                                              _esp_temp_handle));
}

MeshRoof::~MeshRoof()
{

}

void MeshRoof::amplify(bool onOff)
{
    _isAmplifying = onOff;
    gpio_set_level(AMPLIFY_PIN, !onOff);
    gpio_set_level(SWITCH_PIN, !onOff);
}

bool MeshRoof::isAmplifying(void) const
{
    return _isAmplifying;
}

string MeshRoof::getAmplifierGain(void) const
{
    return _amplifierGain.empty() ? "high" : _amplifierGain;
}

void MeshRoof::setAmplifierGain(const string &gain)
{
    _amplifierGain = gain;
}

string MeshRoof::getAmplifierPower(void) const
{
    return _isAmplifying ? "27dBm" : "0dBm";
}

void MeshRoof::reset(void)
{
    _resetCount++;

    sendDisconnect();

    gpio_set_level(OUTRESET_PIN, false);
    vTaskDelay(pdMS_TO_TICKS(500));
    gpio_set_level(OUTRESET_PIN, true);

    _lastReset = time(NULL);
}

unsigned int MeshRoof::getResetCount(void) const
{
    return _resetCount;
}

time_t MeshRoof::getLastReset(void) const
{
    return _lastReset;
}

unsigned int MeshRoof::getLastResetSecsAgo(void) const
{
    time_t now;

    now = time(NULL);

    return now - _lastReset;
}

void MeshRoof::buzz(unsigned int ms)
{
    gpio_set_level(BUZZER_PIN, true);
    vTaskDelay(pdMS_TO_TICKS(ms));
    gpio_set_level(BUZZER_PIN, false);
}

void MeshRoof::buzz(unsigned int freq, unsigned int ms)
{
    if (freq == 0 || ms == 0) {
        buzz(ms > 0 ? ms : 200);
        return;
    }

    int64_t start = esp_timer_get_time();
    int64_t duration_us = (int64_t) ms * 1000;
    int half_period_us = 1000000 / (freq * 2);
    if (half_period_us < 50) {
        half_period_us = 50;
    }

    while ((esp_timer_get_time() - start) < duration_us) {
        gpio_set_level(BUZZER_PIN, 1);
        esp_rom_delay_us(half_period_us);
        gpio_set_level(BUZZER_PIN, 0);
        esp_rom_delay_us(half_period_us);
    }
}

void MeshRoof::buzzMorseCode(const string &text, bool clearPrevious)
{
    if (clearPrevious) {
        this->clearMorseText();
    }

    this->addMorseText(text);
}

struct PingSyncCtx {
    SemaphoreHandle_t sem;
    bool success;
    uint32_t rtt;
    esp_ip_addr_t target_addr;
};

static void ping_sync_success(esp_ping_handle_t hdl, void *args)
{
    PingSyncCtx *ctx = (PingSyncCtx *) args;
    esp_ping_get_profile(hdl, ESP_PING_PROF_TIMEGAP, &ctx->rtt, sizeof(ctx->rtt));
    esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &ctx->target_addr, sizeof(ctx->target_addr));
    ctx->success = true;
}

static void ping_sync_timeout(esp_ping_handle_t hdl, void *args)
{
    PingSyncCtx *ctx = (PingSyncCtx *) args;
    ctx->success = false;
}

static void ping_sync_end(esp_ping_handle_t hdl, void *args)
{
    PingSyncCtx *ctx = (PingSyncCtx *) args;
    if (ctx->sem) {
        xSemaphoreGive(ctx->sem);
    }
}

bool MeshRoof::pingHost(const string &host, string &result_ip, uint32_t &rtt_ms)
{
    struct addrinfo hint;
    struct addrinfo *res = NULL;
    struct in_addr addr4;
    ip_addr_t target_addr;
    esp_ping_config_t ping_config;
    esp_ping_callbacks_t cbs;
    esp_ping_handle_t hdl = NULL;
    PingSyncCtx ctx;
    bool ok = false;

    memset(&hint, 0, sizeof(hint));
    memset(&target_addr, 0, sizeof(target_addr));

    if (getaddrinfo(host.c_str(), NULL, &hint, &res) != 0 || res == NULL) {
        return false;
    }

    addr4 = ((struct sockaddr_in *) (res->ai_addr))->sin_addr;
    inet_addr_to_ip4addr(ip_2_ip4(&target_addr), &addr4);
    freeaddrinfo(res);

    ctx.sem = xSemaphoreCreateBinary();
    if (ctx.sem == NULL) {
        return false;
    }
    ctx.success = false;
    ctx.rtt = 0;
    memset(&ctx.target_addr, 0, sizeof(ctx.target_addr));

    ping_config = ESP_PING_DEFAULT_CONFIG();
    ping_config.target_addr = target_addr;
    ping_config.count = 1;
    ping_config.timeout_ms = 1500;

    cbs.on_ping_success = ping_sync_success;
    cbs.on_ping_timeout = ping_sync_timeout;
    cbs.on_ping_end = ping_sync_end;
    cbs.cb_args = &ctx;

    if (esp_ping_new_session(&ping_config, &cbs, &hdl) == ESP_OK) {
        if (esp_ping_start(hdl) == ESP_OK) {
            xSemaphoreTake(ctx.sem, pdMS_TO_TICKS(2000));
            esp_ping_stop(hdl);
        }
        esp_ping_delete_session(hdl);
    }

    vSemaphoreDelete(ctx.sem);

    if (ctx.success) {
        result_ip = inet_ntoa(ctx.target_addr.u_addr.ip4);
        rtt_ms = ctx.rtt;
        ok = true;
    }

    return ok;
}

bool MeshRoof::isOnboardLedOn(void) const
{
    return _onboardLed;
}

void MeshRoof::setOnboardLed(bool onOff)
{
    _onboardLed = onOff;
    gpio_set_level(ONBOARD_LED_PIN, onOff);
}

void MeshRoof::flipOnboardLed(void)
{
    _onboardLed = !_onboardLed;
    gpio_set_level(ONBOARD_LED_PIN, _onboardLed);
}

float MeshRoof::getCpuTempC(void) const
{
    float tempC = 0.0;
    esp_err_t ret;

    if (_esp_temp_handle == NULL) {
        goto done;
    }

    ret = temperature_sensor_get_celsius((temperature_sensor_handle_t)
                                         _esp_temp_handle, &tempC);
    if (ret != ESP_OK) {
        goto done;
    }

done:

    return tempC;
}

void MeshRoof::gotTextMessage(const meshtastic_MeshPacket &packet,
                              const string &message)
{
    bool result = false;
    SimpleClient::gotTextMessage(packet, message);

    result = handleTextMessage(packet, message);
    if (result) {
        return;
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

string MeshRoof::handleUnknown(uint32_t node_num, uint32_t dest,
                               uint8_t channel, string &message)
{
    string reply;
    string first_word;

    (void)(node_num);
    (void)(dest);
    (void)(channel);

    first_word = message.substr(0, message.find(' '));
    toLowercase(first_word);
    message = message.substr(first_word.size());
    trimWhitespace(message);

    if (first_word == "status") {
        reply = handleStatus(node_num, message);
    } else if (first_word == "rollcall") {
        reply = handleRollcall(node_num, message);
    } else if (first_word == "identify") {
        reply = handleRollcall(node_num, message);
        if (reply.compare(0, 9, "rollcall:") == 0) {
            reply.replace(0, 8, "identify");
        }
    } else if (first_word == "wifi") {
        reply = handleWifi(node_num, message);
    } else if (first_word == "net") {
        reply = handleNet(node_num, message);
    } else if (first_word == "amplify") {
        reply = handleAmplify(node_num, message);
    } else if (first_word == "reset") {
        reply = handleReset(node_num, message);
    } else if (first_word == "buzz") {
        reply = handleBuzz(node_num, message);
    } else if (first_word == "morse") {
        reply = handleMorse(node_num, message);
    }

    return reply;
}

string MeshRoof::handleRollcall(uint32_t node_num, string &message)
{
    (void)(node_num);
    string target = message;
    trimWhitespace(target);

    if (!target.empty()) {
        toLowercase(target);
        uint32_t my_num = _client != NULL ? _client->whoami() : 0;
        string short_name = _client != NULL ? _client->lookupShortName(my_num) : "";
        string long_name = _client != NULL ? _client->lookupLongName(my_num) : "";
        toLowercase(short_name);
        toLowercase(long_name);

        char hex1[16], hex2[16];
        snprintf(hex1, sizeof(hex1), "!%08lx", (unsigned long) my_num);
        snprintf(hex2, sizeof(hex2), "0x%08lx", (unsigned long) my_num);

        if (target != hex1 && target != hex2 &&
            target != short_name && target != long_name &&
            target != "all" && target != "meshroof") {
            return "";  // Target does not match this node, remain silent
        }
    }

    return "rollcall: app=meshroof ver=2.1.4 hw=esp32s3 caps=amplify,wifi,net,cpu_temp,buzzer";
}

string MeshRoof::handleStatus(uint32_t node_num, string &message)
{
    stringstream ss;

    (void)(node_num);
    (void)(message);

    ss << "status: amplify=" << (isAmplifying() ? "on" : "off")
       << " gain=" << getAmplifierGain()
       << " pa=" << getAmplifierPower()
       << " reset_count=" << getResetCount()
       << " last_reset=" << getLastResetSecsAgo() << "s"
       << " temp_chip=" << fixed << setprecision(1) << getCpuTempC();

    return ss.str();
}

string MeshRoof::handleEnv(uint32_t node_num, string &message)
{
    stringstream ss;

    ss << HomeChat::handleEnv(node_num, message);
    if (!ss.str().empty()) {
        ss << " ";
    }

    ss << "temp_chip=";
    ss << fixed << setprecision(1) << getCpuTempC();

    return ss.str();
}

string MeshRoof::handleWifi(uint32_t node_num, string &message)
{
    stringstream ss;
    const wifi_event_sta_connected_t *sta_connected =
        espWifi()->getStaConnected();
    const esp_netif_ip_info_t *ip_info = espWifi()->getIpInfo();

    (void)(node_num);
    (void)(message);

    if (sta_connected->bssid[0] == 0x0) {
        ss << "wifi: status=disconnected";
    } else {
        char buf[40];

        bzero(buf, sizeof(buf));
        memcpy(buf, sta_connected->ssid,
               min((size_t) sta_connected->ssid_len, sizeof(buf)));
        ss << "wifi: status=connected";
        ss << " ssid=" << buf;
        ss << " rssi=" << espWifi()->getRssi();
        snprintf(buf, sizeof(buf) - 1,
                 " ip=" IPSTR, IP2STR(&ip_info->ip));
        ss << buf;
    }

    return ss.str();
}

string MeshRoof::handleNet(uint32_t node_num, string &message)
{
    stringstream ss;
    const esp_netif_ip_info_t *ip_info = espWifi()->getIpInfo();
    const esp_netif_dns_info_t *dns1_info = espWifi()->getDns1Info();
    char buf[80];

    (void)(node_num);

    string cmd = message;
    trimWhitespace(cmd);
    string first_word = cmd.substr(0, cmd.find(' '));
    toLowercase(first_word);

    if (first_word == "ping") {
        string host = cmd.substr(first_word.size());
        trimWhitespace(host);
        if (host.empty()) {
            return "net: ping requires host argument";
        }
        string target_ip;
        uint32_t rtt_ms = 0;
        if (pingHost(host, target_ip, rtt_ms)) {
            snprintf(buf, sizeof(buf) - 1, "net: ping=%s rtt=%lums",
                     target_ip.c_str(), (unsigned long) rtt_ms);
            return string(buf);
        } else {
            snprintf(buf, sizeof(buf) - 1, "net: ping=%s failed", host.c_str());
            return string(buf);
        }
    }

    snprintf(buf, sizeof(buf) - 1,
             "net: ip=" IPSTR " gw=" IPSTR " dns=" IPSTR,
             IP2STR(&ip_info->ip),
             IP2STR(&ip_info->gw),
             IP2STR(&dns1_info->ip.u_addr.ip4));
    return string(buf);
}

string MeshRoof::handleAmplify(uint32_t node_num, string &message)
{
    string reply;

    (void)(node_num);

    string args = message;
    trimWhitespace(args);
    string first_word = args.substr(0, args.find(' '));
    toLowercase(first_word);

    if (first_word.empty()) {
        reply = "amplify: state=" + string(isAmplifying() ? "on" : "off") +
                " gain=" + getAmplifierGain() +
                " pa=" + getAmplifierPower();
    } else if (first_word == "on") {
        amplify(true);
        reply = "amplify: state=on";
    } else if (first_word == "off") {
        amplify(false);
        reply = "amplify: state=off";
    } else if (first_word == "gain") {
        string level = args.substr(first_word.size());
        trimWhitespace(level);
        if (!level.empty()) {
            setAmplifierGain(level);
            reply = "amplify: gain=" + level;
        } else {
            reply = "amplify: gain=" + getAmplifierGain();
        }
    } else {
        reply = "syntax error!";
    }

    return reply;
}

static const char *getResetReasonString(esp_reset_reason_t reason)
{
    switch (reason) {
    case ESP_RST_POWERON:   return "poweron";
    case ESP_RST_EXT:       return "ext";
    case ESP_RST_SW:        return "software";
    case ESP_RST_PANIC:     return "panic";
    case ESP_RST_INT_WDT:   return "int_wdt";
    case ESP_RST_TASK_WDT:  return "task_wdt";
    case ESP_RST_WDT:       return "wdt";
    case ESP_RST_DEEPSLEEP: return "deepsleep";
    case ESP_RST_BROWNOUT:  return "brownout";
    case ESP_RST_SDIO:      return "sdio";
    default:                return "unknown";
    }
}

string MeshRoof::handleReset(uint32_t node_num, string &message)
{
    stringstream ss;

    (void)(node_num);

    toLowercase(message);

    if (message.empty()) {
        ss << "reset: count=" << getResetCount();
        ss << " reason=" << getResetReasonString(esp_reset_reason());
        ss << " secs_ago=" << getLastResetSecsAgo();
    } else if (message == "apply") {
        reset();
        ss << "reset: applied=yes";
    } else {
        ss << "syntax error!";
    }

    return ss.str();
}

string MeshRoof::handleBuzz(uint32_t node_num, string &message)
{
    unsigned int freq = 2000;
    unsigned int dur = 300;

    (void)(node_num);

    string args = message;
    trimWhitespace(args);

    if (!args.empty()) {
        stringstream ss(args);
        if (ss >> freq) {
            if (!(ss >> dur)) {
                dur = 300;
            }
        }
    }

    buzz(freq, dur);

    stringstream ss_rep;
    ss_rep << "buzz: freq=" << freq << " dur=" << dur;
    return ss_rep.str();
}

string MeshRoof::handleMorse(uint32_t node_num, string &message)
{
    string reply;

    (void)(node_num);

    trimWhitespace(message);
    addMorseText(message);
    reply = "morse: text='" + message + "'";

    return reply;
}

int MeshRoof::vprintf(const char *format, va_list ap) const
{
    return usb_vprintf(format, ap);
}

string MeshRoof::getWifiSsid(void) const
{
    return _main_body.wifi_ssid;
}

string MeshRoof::getWifiPasswd(void) const
{
    return _main_body.wifi_passwd;
}

string MeshRoof::getIpString(void) const
{
    struct in_addr sin_addr = {
        .s_addr = _main_body.ip,
    };
    char buf[32] = { '\0', };

    inet_ntop(AF_INET, &sin_addr, buf, sizeof(buf));

    return string(buf);
}

string MeshRoof::getNetmaskString(void) const
{
    struct in_addr sin_addr = {
        .s_addr = _main_body.netmask,
    };
    char buf[32] = { '\0', };

    inet_ntop(AF_INET, &sin_addr, buf, sizeof(buf));

    return string(buf);
}

string MeshRoof::getGatewayString(void) const
{
    struct in_addr sin_addr = {
        .s_addr = _main_body.gateway,
    };
    char buf[32] = { '\0', };

    inet_ntop(AF_INET, &sin_addr, buf, sizeof(buf));

    return string(buf);
}

string MeshRoof::getDns1String(void) const
{
    struct in_addr sin_addr = {
        .s_addr = _main_body.dns1,
    };
    char buf[32] = { '\0', };

    inet_ntop(AF_INET, &sin_addr, buf, sizeof(buf));

    return string(buf);
}

string MeshRoof::getDns2String(void) const
{
    struct in_addr sin_addr = {
        .s_addr = _main_body.dns2,
    };
    char buf[32] = { '\0', };

    inet_ntop(AF_INET, &sin_addr, buf, sizeof(buf));

    return string(buf);
}

string MeshRoof::getDns3String(void) const
{
    struct in_addr sin_addr = {
        .s_addr = _main_body.dns3,
    };
    char buf[32] = { '\0', };

    inet_ntop(AF_INET, &sin_addr, buf, sizeof(buf));

    return string(buf);
}

uint32_t MeshRoof::getIp(void) const
{
    return _main_body.ip;
}

uint32_t MeshRoof::getNetmask(void) const
{
    return _main_body.netmask;
}

uint32_t MeshRoof::getGateway(void) const
{
    return _main_body.gateway;
}

uint32_t MeshRoof::getDns1(void) const
{
    return _main_body.dns1;
}

uint32_t MeshRoof::getDns2(void) const
{
    return _main_body.dns2;
}

uint32_t MeshRoof::getDns3(void) const
{
    return _main_body.dns3;
}

string MeshRoof::getNetIfPassword(void) const
{
    return _main_body.netif_passwd;
}

bool MeshRoof::setWifiSsid(const string &ssid)
{
    if (ssid.length() >= sizeof(_main_body.wifi_ssid)) {
        return false;
    }

    memset(_main_body.wifi_ssid, 0x0, sizeof(_main_body.wifi_ssid));
    memcpy(_main_body.wifi_ssid, ssid.c_str(), ssid.length());

    return true;
}

bool MeshRoof::setWifiPasswd(const string &passwd)
{
    if (passwd.length() >= sizeof(_main_body.wifi_passwd)) {
        return false;
    }

    memset(_main_body.wifi_passwd, 0x0, sizeof(_main_body.wifi_passwd));
    memcpy(_main_body.wifi_passwd, passwd.c_str(), passwd.length());

    return true;
}

bool MeshRoof::setIp(const string &addr)
{
    int ret;
    struct in_addr in_addr;

    ret = inet_aton(addr.c_str(), &in_addr);
    if (ret != 1) {
        return false;
    }

    _main_body.ip = in_addr.s_addr;

    return true;
}

bool MeshRoof::setNetmask(const string &addr)
{
    int ret;
    struct in_addr in_addr;

    ret = inet_aton(addr.c_str(), &in_addr);
    if (ret != 1) {
        return false;
    }

    _main_body.netmask = in_addr.s_addr;

    return true;
}

bool MeshRoof::setGateway(const string &addr)
{
    int ret;
    struct in_addr in_addr;

    ret = inet_aton(addr.c_str(), &in_addr);
    if (ret != 1) {
        return false;
    }

    _main_body.gateway = in_addr.s_addr;

    return true;
}

bool MeshRoof::setDns1(const string &addr)
{
    int ret;
    struct in_addr in_addr;

    ret = inet_aton(addr.c_str(), &in_addr);
    if (ret != 1) {
        return false;
    }

    _main_body.dns1 = in_addr.s_addr;

    return true;
}

bool MeshRoof::setDns2(const string &addr)
{
    int ret;
    struct in_addr in_addr;

    ret = inet_aton(addr.c_str(), &in_addr);
    if (ret != 1) {
        return false;
    }

    _main_body.dns2 = in_addr.s_addr;

    return true;
}

bool MeshRoof::setDns3(const string &addr)
{
    int ret;
    struct in_addr in_addr;

    ret = inet_aton(addr.c_str(), &in_addr);
    if (ret != 1) {
        return false;
    }

    _main_body.dns3 = in_addr.s_addr;

    return true;
}

bool MeshRoof::setNetIfPasswd(const string &passwd)
{
    if (passwd.length() >= sizeof(_main_body.netif_passwd)) {
        return false;
    }

    memset(_main_body.netif_passwd, 0x0, sizeof(_main_body.netif_passwd));
    memcpy(_main_body.netif_passwd, passwd.c_str(), passwd.length());

    return true;
}

#define FLASH_TARGET_SIZE   8192

struct nvm_meta {
    size_t size;
};

static bool nvm_blob_size(uint32_t n_authchans, uint32_t n_admins,
                          uint32_t n_mates, size_t *out)
{
    size_t size;
    size_t add;

    size = sizeof(struct nvm_header) + sizeof(struct nvm_main_body) +
        sizeof(struct nvm_footer);

    add = (size_t) n_authchans * sizeof(struct nvm_authchan_entry);
    if ((n_authchans != 0) &&
        ((add / sizeof(struct nvm_authchan_entry)) != n_authchans)) {
        return false;
    }
    if (size > (((size_t) -1) - add)) {
        return false;
    }
    size += add;

    add = (size_t) n_admins * sizeof(struct nvm_admin_entry);
    if ((n_admins != 0) &&
        ((add / sizeof(struct nvm_admin_entry)) != n_admins)) {
        return false;
    }
    if (size > (((size_t) -1) - add)) {
        return false;
    }
    size += add;

    add = (size_t) n_mates * sizeof(struct nvm_mate_entry);
    if ((n_mates != 0) &&
        ((add / sizeof(struct nvm_mate_entry)) != n_mates)) {
        return false;
    }
    if (size > (((size_t) -1) - add)) {
        return false;
    }
    size += add;

    *out = size;
    return true;
}

bool MeshRoof::loadNvm(void)
{
    bool result = false;
    bool nvs_opened = false;
    esp_err_t err;
    nvs_handle_t handle = 0;
    uint8_t *buf = NULL;
    size_t size = 0;
    size_t need = 0;
    struct nvm_meta nvm_meta;
    const struct nvm_header *header = NULL;
    const struct nvm_main_body *main_body = NULL;
    const struct nvm_authchan_entry *authchans = NULL;
    const struct nvm_admin_entry *admins = NULL;
    const struct nvm_mate_entry *mates = NULL;
    struct nvm_footer *footer = NULL;
    uint32_t stored_crc;
    uint32_t calc_crc;
    unsigned int i;

    err = nvs_open("meshroof", NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open: %s!", esp_err_to_name(err));
        result = false;
        goto done;
    }
    nvs_opened = true;

    size = sizeof(nvm_meta);
    err = nvs_get_blob(handle, "nvm_meta", &nvm_meta, &size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_get_blob (meta): %s!", esp_err_to_name(err));
        result = false;
        goto done;
    }

    if ((nvm_meta.size < (sizeof(struct nvm_header) +
                          sizeof(struct nvm_main_body) +
                          sizeof(struct nvm_footer))) ||
        (nvm_meta.size > FLASH_TARGET_SIZE)) {
        ESP_LOGE(TAG, "Too big size=%zu!", nvm_meta.size);
        result = false;
        goto done;
    }

    size = nvm_meta.size;
    buf = (uint8_t *) malloc(size);
    if (buf == NULL) {
        ESP_LOGE(TAG, "malloc failed!");
        result = false;
        goto done;
    }

    err = nvs_get_blob(handle, "meshroof", buf, &size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_get_blob: %s!", esp_err_to_name(err));
        result = false;
        goto done;
    }

    header = (const struct nvm_header *) buf;
    if (header->magic != NVM_HEADER_MAGIC) {
        ESP_LOGE(TAG, "Wrong header magic!");
        result = false;
        goto done;
    }
    main_body = (const struct nvm_main_body *)
        (((const uint8_t *) header) + sizeof(*header));
    if (nvm_blob_size(main_body->n_authchans, main_body->n_admins,
                      main_body->n_mates, &need) == false) {
        ESP_LOGE(TAG, "NVM size overflow!");
        result = false;
        goto done;
    }
    if ((need > FLASH_TARGET_SIZE) || (need != size)) {
        ESP_LOGE(TAG, "Bad NVM size=%zu need=%zu!", size, need);
        result = false;
        goto done;
    }
    authchans = (const struct nvm_authchan_entry *)
        (((uint8_t *) main_body) + sizeof(*main_body));
    admins = (const struct nvm_admin_entry *)
        (((uint8_t *) authchans) +
         (sizeof(struct nvm_authchan_entry) * main_body->n_authchans));
    mates = (const struct nvm_mate_entry *)
        (((uint8_t *) admins) +
         (sizeof(struct nvm_admin_entry) * main_body->n_admins));
    footer = (struct nvm_footer *)
        (((uint8_t *) mates) +
         (sizeof(struct nvm_mate_entry) * main_body->n_mates));
    if (footer->magic != NVM_FOOTER_MAGIC) {
        ESP_LOGE(TAG, "Wrong footer magic!");
        result = false;
        goto done;
    }
    stored_crc = footer->crc32;
    footer->crc32 = 0;
    calc_crc = esp_crc32_le(0, buf, (uint32_t) size);
    footer->crc32 = stored_crc;
    if ((stored_crc != 0) && (stored_crc != calc_crc)) {
        ESP_LOGE(TAG, "Bad NVM crc32!");
        result = false;
        goto done;
    }
    memcpy(&_main_body, main_body, sizeof(struct nvm_main_body));
    _nvm_authchans.clear();
    for (i = 0; i < main_body->n_authchans; i++) {
        _nvm_authchans.push_back(authchans[i]);
    }
    _nvm_admins.clear();
    for (i = 0; i < main_body->n_admins; i++) {
        _nvm_admins.push_back(admins[i]);
    }
    _nvm_mates.clear();
    for (i = 0; i < main_body->n_mates; i++) {
        _nvm_mates.push_back(mates[i]);
    }

    result = true;

done:

    if (buf) {
        free(buf);
    }

    if (nvs_opened) {
        nvs_close(handle);
    }

    return result;
}

bool MeshRoof::saveNvm(void)
{
    bool result = false;
    bool nvs_opened = false;
    esp_err_t err;
    nvs_handle_t handle = 0;
    uint8_t *buf = NULL;
    size_t size = 0;
    struct nvm_meta nvm_meta;
    struct nvm_header *header = NULL;
    struct nvm_main_body *main_body = NULL;
    struct nvm_authchan_entry *authchans = NULL;
    struct nvm_admin_entry *admins = NULL;
    struct nvm_mate_entry *mates = NULL;
    struct nvm_footer *footer = NULL;
    unsigned int i;

    _main_body.n_authchans = nvmAuthchans().size();
    _main_body.n_admins = nvmAdmins().size();
    _main_body.n_mates = nvmMates().size();

    if (nvm_blob_size(_main_body.n_authchans, _main_body.n_admins,
                      _main_body.n_mates, &size) == false) {
        result = false;
        goto done;
    }
    if (size > FLASH_TARGET_SIZE) {
        ESP_LOGE(TAG, "Too big size=%zu!", size);
        result = false;
        goto done;
    }

    buf = (uint8_t *) malloc(size);
    if (buf == NULL) {
        result = false;
        goto done;
    }

    memset(buf, 0x0, size);

    header = (struct nvm_header *) buf;
    header->magic = NVM_HEADER_MAGIC;
    main_body = (struct nvm_main_body *)
        (((uint8_t *) header) + sizeof(struct nvm_header));
    memcpy(main_body, &_main_body, sizeof(struct nvm_main_body));
    authchans = (struct nvm_authchan_entry *)
        (((uint8_t *) main_body) + sizeof(struct nvm_main_body));
    for (i = 0; i < _main_body.n_authchans; i++) {
        memcpy(&authchans[i], &nvmAuthchans()[i],
               sizeof(struct nvm_authchan_entry));
    }
    admins = (struct nvm_admin_entry *)
        (((uint8_t *) authchans) +
         (sizeof(struct nvm_authchan_entry) * _main_body.n_authchans));
    for (i = 0; i < _main_body.n_admins; i++) {
        memcpy(&admins[i], &nvmAdmins()[i],
               sizeof(struct nvm_admin_entry));
    }
    mates = (struct nvm_mate_entry *)
        (((uint8_t *) admins) +
         (sizeof(struct nvm_admin_entry) * _main_body.n_admins));
    for (i = 0; i < _main_body.n_mates; i++) {
        memcpy(&mates[i], &nvmMates()[i],
               sizeof(struct nvm_mate_entry));
    }
    footer = (struct nvm_footer *)
        (((uint8_t *) mates) +
         (sizeof(struct nvm_mate_entry) * _main_body.n_mates));
    footer->magic = NVM_FOOTER_MAGIC;
    footer->crc32 = 0;
    footer->crc32 = esp_crc32_le(0, buf, (uint32_t) size);

    err = nvs_open("meshroof", NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open: %s!", esp_err_to_name(err));
        result = false;
        goto done;
    }
    nvs_opened = true;

    nvm_meta.size = size;
    err = nvs_set_blob(handle, "nvm_meta", &nvm_meta, sizeof(nvm_meta));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_set_blob (meta): %s!", esp_err_to_name(err));
        result = false;
        goto done;
    }

    err = nvs_set_blob(handle, "meshroof", buf, size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_set_blob: %s!", esp_err_to_name(err));
        result = false;
        goto done;
    }

    err = nvs_commit(handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_commit: %s!", esp_err_to_name(err));
        result = false;
        goto done;
    }

    result = true;

done:

    if (buf) {
        free(buf);
    }

    if (nvs_opened) {
        nvs_close(handle);
    }

    return result;
}

bool MeshRoof::applyNvmToHomeChat(void)
{
    bool result = true;


    clearAuthchansAdminsMates();

    for (vector<struct nvm_authchan_entry>::const_iterator it =
             nvmAuthchans().begin(); it != nvmAuthchans().end(); it++) {
        if (addAuthChannel(it->name, it->psk) == false) {
            result = false;
        }
    }

    for (vector<struct nvm_admin_entry>::const_iterator it =
             nvmAdmins().begin(); it != nvmAdmins().end(); it++) {
        if (addAdmin(it->node_num, it->pubkey) == false) {
            result = false;
        }
    }

    for (vector<struct nvm_mate_entry>::const_iterator it =
             nvmMates().begin(); it != nvmMates().end(); it++) {
        if (addMate(it->node_num, it->pubkey) == false) {
            result = false;
        }
    }

    return result;
}

void MeshRoof::sleepForMs(unsigned int ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}

void MeshRoof::toggleBuzzer(bool onOff)
{
    if (onOff) {
        gpio_set_level(BUZZER_PIN, true);
    } else {
        gpio_set_level(BUZZER_PIN, false);
    }
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
