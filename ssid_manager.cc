#include "ssid_manager.h"
#include <cstring>
#include <algorithm>
#include <esp_log.h>
#include <nvs_flash.h>

#define TAG "SsidManager"
#define NVS_NAMESPACE "wifi"
#define MAX_WIFI_SSID_COUNT 10

SsidManager::SsidManager() {
    LoadFromNvs();
}

SsidManager::~SsidManager() {
}

void SsidManager::Clear() {
    ssid_list_.clear();
    SaveToNvs();
}

void SsidManager::LoadFromNvs() {
    ssid_list_.clear();

    // Load ssid and password from NVS from namespace "wifi"
    // ssid, ssid1, ssid2, ... ssid9
    // password, password1, password2, ... password9
    nvs_handle_t nvs_handle;
    auto ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        // The namespace doesn't exist, just return
        ESP_LOGW(TAG, "NVS namespace %s doesn't exist", NVS_NAMESPACE);
        return;
    }
    for (int i = 0; i < MAX_WIFI_SSID_COUNT; i++) {
        char ssid[33] = {0};
        char password[65] = {0};
        uint8_t bssid[6] = {0};
        uint8_t channel = 0;

        size_t ssid_len = sizeof(ssid);
        size_t password_len = sizeof(password);
        size_t bssid_len = sizeof(bssid);

        std::string ssid_key = "ssid" + std::to_string(i);
        std::string password_key = "password" + std::to_string(i);
        std::string bssid_key = "bssid" + std::to_string(i);
        std::string channel_key = "channel" + std::to_string(i);

        if (nvs_get_str(nvs_handle, ssid_key.c_str(), ssid, &ssid_len) != ESP_OK) continue;
        if (nvs_get_str(nvs_handle, password_key.c_str(), password, &password_len) != ESP_OK) continue;
        if (nvs_get_blob(nvs_handle, bssid_key.c_str(), bssid, &bssid_len) != ESP_OK) {
            memset(bssid, 0, sizeof(bssid));
        }
        if (nvs_get_u8(nvs_handle, channel_key.c_str(), &channel) != ESP_OK) {
            channel = 0;
        }

        ssid_list_.push_back({ssid, password, {bssid[0],bssid[1],bssid[2],bssid[3],bssid[4],bssid[5]}, channel});
    }
    nvs_close(nvs_handle);
}

void SsidManager::SaveToNvs() {
    nvs_handle_t nvs_handle;
    ESP_ERROR_CHECK(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle));
    for (int i = 0; i < MAX_WIFI_SSID_COUNT; i++) {
        std::string ssid_key = "ssid" + std::to_string(i);
        std::string password_key = "password" + std::to_string(i);
        std::string bssid_key = "bssid" + std::to_string(i);
        std::string channel_key = "channel" + std::to_string(i);

        if (i < ssid_list_.size()) {
            auto &item = ssid_list_[i];
            nvs_set_str(nvs_handle, ssid_key.c_str(), item.ssid.c_str());
            nvs_set_str(nvs_handle, password_key.c_str(), item.password.c_str());
            nvs_set_blob(nvs_handle, bssid_key.c_str(), item.bssid, sizeof(item.bssid));
            nvs_set_u8(nvs_handle, channel_key.c_str(), item.channel);
        } else {
            nvs_erase_key(nvs_handle, ssid_key.c_str());
            nvs_erase_key(nvs_handle, password_key.c_str());
            nvs_erase_key(nvs_handle, bssid_key.c_str());
            nvs_erase_key(nvs_handle, channel_key.c_str());
        }
    }
    nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
}

void SsidManager::AddSsid(const std::string& ssid,
                          const std::string& password,
                          uint8_t *bssid,
                          uint8_t *channel) {
    for (auto& item : ssid_list_) {
        ESP_LOGI(TAG, "compare [%s:%d] [%s:%d]", item.ssid.c_str(), item.ssid.size(), ssid.c_str(), ssid.size());
        if (item.ssid == ssid) {
            ESP_LOGW(TAG, "SSID %s already exists, overwrite it", ssid.c_str());
            item.password = password;
            if (bssid != nullptr) memcpy(item.bssid, bssid, 6);
            if (channel != nullptr) item.channel = *channel;
            SaveToNvs();
            return;
        }
    }

    if (ssid_list_.size() >= MAX_WIFI_SSID_COUNT) {
        ESP_LOGW(TAG, "SSID list is full, pop one");
        ssid_list_.pop_back();
    }
    // Add the new ssid to the front of the list
    SsidItem new_item;
    memset(&new_item, 0, sizeof(new_item));
    new_item.ssid = ssid;
    new_item.password = password;
    if (bssid != nullptr) memcpy(new_item.bssid, bssid, 6);
    if (channel != nullptr) new_item.channel = *channel;
    ssid_list_.insert(ssid_list_.begin(), new_item);
    SaveToNvs();
}

void SsidManager::RemoveSsid(int index) {
    if (index < 0 || index >= ssid_list_.size()) {
        ESP_LOGW(TAG, "Invalid index %d", index);
        return;
    }
    ssid_list_.erase(ssid_list_.begin() + index);
    SaveToNvs();
}

void SsidManager::SetDefaultSsid(int index) {
    if (index < 0 || index >= ssid_list_.size()) {
        ESP_LOGW(TAG, "Invalid index %d", index);
        return;
    }
    // Move the ssid at index to the front of the list
    auto item = ssid_list_[index];
    ssid_list_.erase(ssid_list_.begin() + index);
    ssid_list_.insert(ssid_list_.begin(), item);
    SaveToNvs();
}
