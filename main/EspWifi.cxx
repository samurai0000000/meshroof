/*
 * EspWifi.cxx
 *
 * Copyright (C) 2025, Charles Chiou
 */

#include <esp_log.h>
#include <EspWifi.hxx>
#include <MeshRoof.hxx>

extern shared_ptr<MeshRoof> meshroof;

static const char *TAG = "EspWifi";
static shared_ptr<EspWifi> g_wifi = NULL;

shared_ptr<EspWifi> EspWifi::getInstance(void)
{
    if (g_wifi == NULL) {
        g_wifi = shared_ptr<EspWifi>(new EspWifi(), [](EspWifi *p) {
            delete p;
        });
    }

    return g_wifi;
}

EspWifi::EspWifi()
    : _started(false),
      _sta_netif(NULL)
{
    int ret;

    esp_netif_init();
    esp_event_loop_create_default();

    _sta_netif = esp_netif_create_default_wifi_sta();
    if (_sta_netif == NULL) {
        ESP_LOGE(TAG, "esp_netif_create_default_wifi_sta returned NULL!");
    }

    _init_config = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&_init_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_init ret=%d", ret);
    }

    ret = esp_event_handler_instance_register(
        WIFI_EVENT,
        ESP_EVENT_ANY_ID,
        espEventHandler,
        this,
        NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_event_handler_instance_register ret=%d", ret);
    }

    ret = esp_event_handler_instance_register(
        IP_EVENT,
        IP_EVENT_STA_GOT_IP,
        espEventHandler,
        this,
        NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_event_handler_instance_register ret=%d", ret);
    }
}

EspWifi::~EspWifi()
{

}

int EspWifi::start(void)
{
    esp_err_t ret = 0;

    if (_started) {
        ret = 0;
        goto done;
    }

    bzero(&_config, sizeof(_config));
    memcpy(_config.sta.ssid, meshroof->getWifiSsid().c_str(),
           min(meshroof->getWifiSsid().size(), sizeof(_config.sta.ssid)));
    memcpy(_config.sta.password, meshroof->getWifiPasswd().c_str(),
           min(meshroof->getWifiPasswd().size(), sizeof(_config.sta.password)));
    _config.sta.scan_method = WIFI_FAST_SCAN;
    _config.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
    _config.sta.threshold.rssi = -127;
    _config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    _config.sta.threshold.rssi_5g_adjustment = 0;

    ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_set_mode ret=%d", ret);
    }

    ret = esp_wifi_set_config(WIFI_IF_STA, &_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_set_config ret=%d", ret);
    }

    esp_log_level_set("wifi", ESP_LOG_ERROR);
    esp_log_level_set("esp_netif_handlers", ESP_LOG_ERROR);

    ret = esp_wifi_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_start ret=%d", ret);
    }

    _started = true;
    ret = 0;

done:

    return (int) ret;
}

int EspWifi::stop(void)
{
    int ret = 0;

    if (!_started) {
        ret = 0;
        goto done;
    }

    ret = esp_wifi_stop();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_stop ret=%d", ret);
    }

    _started = false;
    ret = 0;

done:

    return ret;
}

int EspWifi::getRssi(void) const
{
    int rssi = 0;

    if (_started) {
        esp_err_t ret;

        ret = esp_wifi_sta_get_rssi(&rssi);
        if (ret != ESP_OK) {
            rssi = 0;
        }
    }

    return rssi;
}

void EspWifi::resetStatus(void)
{
    bzero(&_sta_connected, sizeof(_sta_connected));
    bzero(&_ip_info, sizeof(_ip_info));
    bzero(&_dns1_info, sizeof(_dns1_info));
    bzero(&_dns2_info, sizeof(_dns2_info));
    bzero(&_dns3_info, sizeof(_dns3_info));
}

static const char *wifi_event_to_str(int32_t event_id)
{
    switch (event_id) {
    case WIFI_EVENT_WIFI_READY:
        return "WIFI_EVENT_WIFI_READY";
    case WIFI_EVENT_SCAN_DONE:
        return "WIFI_EVENT_SCAN_DONE";
    case WIFI_EVENT_STA_START:
        return "WIFI_EVENT_STA_START";
    case WIFI_EVENT_STA_STOP:
        return "WIFI_EVENT_STA_STOP";
    case WIFI_EVENT_STA_CONNECTED:
        return "WIFI_EVENT_STA_CONNECTED";
    case WIFI_EVENT_STA_DISCONNECTED:
        return "WIFI_EVENT_STA_CONNECTED";
    case WIFI_EVENT_STA_AUTHMODE_CHANGE:
        return "WIFI_EVENT_STA_AUTHMODE_CHANGE";
    case WIFI_EVENT_STA_WPS_ER_SUCCESS:
        return "WIFI_EVENT_STA_WPS_ER_SUCCESS";
    case WIFI_EVENT_STA_WPS_ER_FAILED:
        return "WIFI_EVENT_STA_WPS_ER_FAILED";
    case WIFI_EVENT_STA_WPS_ER_TIMEOUT:
        return "WIFI_EVENT_STA_WPS_ER_FAILED";
    case WIFI_EVENT_STA_WPS_ER_PIN:
        return "WIFI_EVENT_STA_WPS_ER_PIN";
    case WIFI_EVENT_STA_WPS_ER_PBC_OVERLAP:
        return "WIFI_EVENT_STA_WPS_ER_PBC_OVERLAP";
    case WIFI_EVENT_AP_START:
        return "WIFI_EVENT_AP_START";
    case WIFI_EVENT_AP_STOP:
        return "WIFI_EVENT_AP_STOP";
    case WIFI_EVENT_AP_STACONNECTED:
        return "WIFI_EVENT_AP_STACONNECTED";
    case WIFI_EVENT_AP_STADISCONNECTED:
        return "WIFI_EVENT_AP_STADISCONNECTED";
    case WIFI_EVENT_AP_PROBEREQRECVED:
        return "WIFI_EVENT_AP_PROBEREQRECVED";
    case WIFI_EVENT_FTM_REPORT:
        return "WIFI_EVENT_FTM_REPORT";
    case WIFI_EVENT_STA_BSS_RSSI_LOW:
        return "WIFI_EVENT_STA_BSS_RSSI_LOW";
    case WIFI_EVENT_ACTION_TX_STATUS:
        return "WIFI_EVENT_ACTION_TX_STATUS";
    case WIFI_EVENT_ROC_DONE:
        return "WIFI_EVENT_ROC_DONE";
    case WIFI_EVENT_STA_BEACON_TIMEOUT:
        return "WIFI_EVENT_STA_BEACON_TIMEOUT";
    case WIFI_EVENT_CONNECTIONLESS_MODULE_WAKE_INTERVAL_START:
        return "WIFI_EVENT_CONNECTIONLESS_MODULE_WAKE_INTERVAL_START";
    case WIFI_EVENT_AP_WPS_RG_SUCCESS:
        return "WIFI_EVENT_AP_WPS_RG_SUCCESS";
    case WIFI_EVENT_AP_WPS_RG_FAILED:
        return "WIFI_EVENT_AP_WPS_RG_FAILED";
    case WIFI_EVENT_AP_WPS_RG_TIMEOUT:
        return "WIFI_EVENT_AP_WPS_RG_TIMEOUT";
    case WIFI_EVENT_AP_WPS_RG_PIN:
        return "WIFI_EVENT_AP_WPS_RG_PIN";
    case WIFI_EVENT_AP_WPS_RG_PBC_OVERLAP:
        return "WIFI_EVENT_AP_WPS_RG_PBC_OVERLAP";
    case WIFI_EVENT_ITWT_SETUP:
        return "WIFI_EVENT_ITWT_SETUP";
    case WIFI_EVENT_ITWT_TEARDOWN:
        return "WIFI_EVENT_ITWT_TEARDOWN";
    case WIFI_EVENT_ITWT_PROBE:
        return "WIFI_EVENT_ITWT_PROBE";
    case WIFI_EVENT_ITWT_SUSPEND:
        return "WIFI_EVENT_ITWT_SUSPEND";
    case WIFI_EVENT_TWT_WAKEUP:
        return "WIFI_EVENT_TWT_WAKEUP";
    case WIFI_EVENT_BTWT_SETUP:
        return "WIFI_EVENT_BTWT_SETUP";
    case WIFI_EVENT_BTWT_TEARDOWN:
        return "WIFI_EVENT_BTWT_TEARDOWN";
    case WIFI_EVENT_NAN_STARTED:
        return "WIFI_EVENT_NAN_STARTED";
    case WIFI_EVENT_NAN_STOPPED:
        return "WIFI_EVENT_NAN_STOPPED";
    case WIFI_EVENT_NAN_SVC_MATCH:
        return "WIFI_EVENT_NAN_SVC_MATCH";
    case WIFI_EVENT_NAN_REPLIED:
        return "WIFI_EVENT_NAN_REPLIED";
    case WIFI_EVENT_NAN_RECEIVE:
        return "WIFI_EVENT_NAN_RECEIVE";
    case WIFI_EVENT_NDP_INDICATION:
        return "WIFI_EVENT_NDP_INDICATION";
    case WIFI_EVENT_NDP_CONFIRM:
        return "WIFI_EVENT_NDP_CONFIRM";
    case WIFI_EVENT_NDP_TERMINATED:
        return "WIFI_EVENT_NDP_TERMINATED";
    case WIFI_EVENT_HOME_CHANNEL_CHANGE:
        return "WIFI_EVENT_HOME_CHANNEL_CHANGE";
    case WIFI_EVENT_STA_NEIGHBOR_REP:
        return "WIFI_EVENT_STA_NEIGHBOR_REP";
    case WIFI_EVENT_AP_WRONG_PASSWORD:
        return "WIFI_EVENT_AP_WRONG_PASSWORD";
    case WIFI_EVENT_STA_BEACON_OFFSET_UNSTABLE:
        return "WIFI_EVENT_STA_BEACON_OFFSET_UNSTABLE";
    case WIFI_EVENT_DPP_URI_READY:
        return "WIFI_EVENT_DPP_URI_READY";
    case WIFI_EVENT_DPP_CFG_RECVD:
        return "WIFI_EVENT_DPP_CFG_RECVD";
    case WIFI_EVENT_DPP_FAILED:
        return "WIFI_EVENT_DPP_FAILED";
    default:
        break;
    }

    return "??? WIFI_EVENT ???";
}

void EspWifi::espEventHandler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data)
{
    EspWifi *wifi = (EspWifi *) arg;

    if (event_base == WIFI_EVENT) {
        switch (event_id) {
        case WIFI_EVENT_STA_START:
            wifi->gotStaStart(event_data);
            break;
        case WIFI_EVENT_STA_STOP:
            wifi->gotStaStop(event_data);
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            wifi->gotStaDisconnected(event_data);
            break;
        case WIFI_EVENT_HOME_CHANNEL_CHANGE:
            wifi->gotHomeChannelChange(event_data);
            break;
        case WIFI_EVENT_STA_CONNECTED:
            wifi->gotStaConnected(event_data);
            break;
        default:
            ESP_LOGI(TAG, "Unhandled %s (%d)",
                     wifi_event_to_str(event_id),
                     event_id);
            break;
        }
    }

    if (event_base == IP_EVENT) {
        switch (event_id) {
        case IP_EVENT_STA_GOT_IP:
            wifi->gotStaGotIp(event_data);
            break;
        default:
            ESP_LOGI(TAG, "Unhandled IP_EVENT %d", event_id);
            break;
        }
    }
}

void EspWifi::gotStaStart(const void *e)
{
    (void)(e);

    _started = true;
    esp_wifi_connect();
}

void EspWifi::gotStaStop(const void *e)
{
    (void)(e);

    esp_wifi_disconnect();
    _started = false;
}

void EspWifi::gotStaDisconnected(const void *e)
{
    (void)(e);

    resetStatus();

    if (_started) {
        esp_wifi_connect();
    }
}

void EspWifi::gotHomeChannelChange(const void *e)
{
    (void)(e);
}

void EspWifi::gotStaConnected(const void *e)
{
    const wifi_event_sta_connected_t *sta_connected =
        (const wifi_event_sta_connected_t *) e;

    memcpy(&_sta_connected, sta_connected, sizeof(_sta_connected));
}

void EspWifi::gotStaGotIp(const void *e)
{
    const ip_event_got_ip_t *event = (const ip_event_got_ip_t *) e;

    memcpy(&_ip_info, &event->ip_info, sizeof(_ip_info));
    esp_netif_get_dns_info(_sta_netif, ESP_NETIF_DNS_MAIN, &_dns1_info);
    esp_netif_get_dns_info(_sta_netif, ESP_NETIF_DNS_BACKUP, &_dns2_info);
    esp_netif_get_dns_info(_sta_netif, ESP_NETIF_DNS_FALLBACK, &_dns3_info);
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
