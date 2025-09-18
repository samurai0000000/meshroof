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
#include <nvs_flash.h>
#include <nvs.h>
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

void MeshRoof::buzzMorseCode(const string &text, bool clearPrevious)
{
    if (clearPrevious) {
        this->clearMorseText();
    }

    this->addMorseText(text);
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

string MeshRoof::handleUnknown(uint32_t node_num, string &message)
{
    string reply;
    string first_word;

    (void)(node_num);

    first_word = message.substr(0, message.find(' '));
    toLowercase(first_word);
    message = message.substr(first_word.size());
    trimWhitespace(message);

    if (first_word == "status") {
        reply = handleStatus(node_num, message);
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

string MeshRoof::handleStatus(uint32_t node_num, string &message)
{
    string reply;

    (void)(node_num);
    (void)(message);

    return reply;
}

string MeshRoof::handleEnv(uint32_t node_num, string &message)
{
    stringstream ss;

    ss << HomeChat::handleEnv(node_num, message);
    if (!ss.str().empty()) {
        ss << endl;
    }

    ss << "cpu temperature: ";
    ss <<  setprecision(3) << getCpuTempC();

    return ss.str();
}

string MeshRoof::handleWifi(uint32_t node_num, string &message)
{
    stringstream ss;
    const wifi_event_sta_connected_t *sta_connected =
        espWifi()->getStaConnected();

    (void)(node_num);

    if (sta_connected->bssid[0] == 0x0) {
        ss << "Wifi not connected";
    } else {
        char buf[40];

        bzero(buf, sizeof(buf));
        memcpy(buf, sta_connected->ssid,
               min((size_t) sta_connected->ssid_len, sizeof(buf)));
        ss << "Wifi is connected" << endl;
        ss << "ssid: " << buf << endl;
        snprintf(buf, sizeof(buf) - 1,
                 "bssid: %.2x:%.2x:%.2x:%.2x:%.2x:%.2x",
                 sta_connected->bssid[0],
                 sta_connected->bssid[1],
                 sta_connected->bssid[2],
                 sta_connected->bssid[3],
                 sta_connected->bssid[4],
                 sta_connected->bssid[5]);
        ss << buf << endl;
        ss << "channel: " << (int) sta_connected->channel << endl;
        ss << "rssi: " << espWifi()->getRssi();
    }

    return ss.str();
}

string MeshRoof::handleNet(uint32_t node_num, string &message)
{
    stringstream ss;
    const esp_netif_ip_info_t *ip_info = espWifi()->getIpInfo();
    const esp_netif_dns_info_t *dns1_info = espWifi()->getDns1Info();
    const esp_netif_dns_info_t *dns2_info = espWifi()->getDns2Info();
    const esp_netif_dns_info_t *dns3_info = espWifi()->getDns3Info();
    char buf[80];

    (void)(node_num);

    if (getIp() == 0) {
        this->printf("(dhcp)\n");
    } else {
        this->printf("(static ip)\n");
    }

    snprintf(buf, sizeof(buf) - 1,
             "ip:      " IPSTR, IP2STR(&ip_info->ip));
    ss << buf << endl;
    snprintf(buf, sizeof(buf) - 1,
             "netmask: " IPSTR, IP2STR(&ip_info->netmask));
    ss << buf << endl;
    snprintf(buf, sizeof(buf) - 1,
             "gateway: " IPSTR, IP2STR(&ip_info->gw));
    ss << buf << endl;
    snprintf(buf, sizeof(buf) - 1,
             "dns1:    " IPSTR, IP2STR(&dns1_info->ip.u_addr.ip4));
    ss << buf << endl;
    snprintf(buf, sizeof(buf) - 1,
             "dns2:    " IPSTR, IP2STR(&dns2_info->ip.u_addr.ip4));
    ss << buf << endl;
    snprintf(buf, sizeof(buf) - 1,
             "dns3:    " IPSTR, IP2STR(&dns3_info->ip.u_addr.ip4));
    ss << buf;

    return ss.str();
}

string MeshRoof::handleAmplify(uint32_t node_num, string &message)
{
    string reply;

    (void)(node_num);
    (void)(message);

    return reply;
}

string MeshRoof::handleReset(uint32_t node_num, string &message)
{
    string reply;

    (void)(node_num);
    (void)(message);

    return reply;
}

string MeshRoof::handleBuzz(uint32_t node_num, string &message)
{
    string reply;

    (void)(node_num);
    (void)(message);

    buzz();

    return reply;
}

string MeshRoof::handleMorse(uint32_t node_num, string &message)
{
    string reply;

    (void)(node_num);
    (void)(message);

    addMorseText(message);
    reply = "buzzing morse code: '" + message + "'";

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
    if (ssid.length() > sizeof(_main_body.wifi_ssid)) {
        return false;
    }

    memset(_main_body.wifi_ssid, 0x0, sizeof(_main_body.wifi_ssid));
    memcpy(_main_body.wifi_ssid, ssid.c_str(), ssid.length());

    return true;
}

bool MeshRoof::setWifiPasswd(const string &passwd)
{
    if (passwd.length() > sizeof(_main_body.wifi_passwd)) {
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
    if (passwd.length() > sizeof(_main_body.netif_passwd)) {
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

bool MeshRoof::loadNvm(void)
{
    bool result = false;
    esp_err_t err;
    nvs_handle_t handle;
    uint8_t *buf;
    size_t size = 0;
    struct nvm_meta nvm_meta;
    const struct nvm_header *header = NULL;
    const struct nvm_main_body *main_body = NULL;
    const struct nvm_authchan_entry *authchans = NULL;
    const struct nvm_admin_entry *admins = NULL;
    const struct nvm_mate_entry *mates = NULL;
    const struct nvm_footer *footer = NULL;
    unsigned int i;


    size = sizeof(struct nvm_header) + sizeof(struct nvm_main_body);
    buf = (uint8_t *) malloc(size);
    if (buf == NULL) {
        ESP_LOGE(TAG, "malloc failed!");
        result = false;
        goto done;
    }

    err = nvs_open("meshroof", NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open: %s!", esp_err_to_name(err));
        result = false;
        goto done;
    }

    size = sizeof(nvm_meta);
    err = nvs_get_blob(handle, "nvm_meta", &nvm_meta, &size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_get_blob (meta): %s!", esp_err_to_name(err));
        result = false;
        goto done;
    }

    if (size > FLASH_TARGET_SIZE) {
        ESP_LOGE(TAG, "Too big size=%zu!", size);
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
    size =
        sizeof(struct nvm_header) +
        sizeof(struct nvm_main_body) +
        (main_body->n_authchans * sizeof(struct nvm_authchan_entry)) +
        (main_body->n_admins * sizeof(struct nvm_admin_entry)) +
        (main_body->n_mates * sizeof(struct nvm_mate_entry)) +
        sizeof(struct nvm_footer);
    if (size > FLASH_TARGET_SIZE) {
        ESP_LOGE(TAG, "Too big size=%zu!", size);
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
    footer = (const struct nvm_footer *)
        (((uint8_t *) mates) +
         (sizeof(struct nvm_mate_entry) * main_body->n_mates));
    if (footer->magic != NVM_FOOTER_MAGIC) {
        ESP_LOGE(TAG, "Wrong footer magic!");
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

    nvs_close(handle);

    return result;
}

bool MeshRoof::saveNvm(void)
{
    bool result = false;
    esp_err_t err;
    nvs_handle_t handle;
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

    size =
        sizeof(struct nvm_header) +
        sizeof(struct nvm_main_body) +
        (sizeof(struct nvm_authchan_entry) * _main_body.n_authchans) +
        (sizeof(struct nvm_admin_entry) * _main_body.n_admins) +
        (sizeof(struct nvm_mate_entry) * _main_body.n_mates) +
        sizeof(struct nvm_footer);

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

    err = nvs_open("meshroof", NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open: %s!", esp_err_to_name(err));
        result = false;
        goto done;
    }

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

    nvs_close(handle);

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
