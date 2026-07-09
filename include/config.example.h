#pragma once

// Copy this file to include/config.local.h and adjust the values for your boat.
// config.local.h is ignored by Git so private Wi-Fi settings stay local.

static constexpr const char* WIFI_AP_SSID = "Jetboat_Telemetrie";
static constexpr const char* WIFI_AP_PASSWORD = "change-me-before-use";

static constexpr unsigned char WIFI_AP_LOCAL_IP[4] = {192, 168, 4, 1};
static constexpr unsigned char WIFI_AP_GATEWAY[4] = {192, 168, 4, 1};
static constexpr unsigned char WIFI_AP_SUBNET[4] = {255, 255, 255, 0};
