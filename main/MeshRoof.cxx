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
#include <algorithm>
#include <meshroof.h>
#include <MeshRoof.hxx>

static const char *TAG = "MeshRoof";

MeshRoof::MeshRoof()
    : SimpleClient(),
      HomeChat()
{
    bzero(&_main_body, sizeof(_main_body));
}

MeshRoof::~MeshRoof()
{

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

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
