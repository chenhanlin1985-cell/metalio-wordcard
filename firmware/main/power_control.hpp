#pragma once

// One sampler shared by normal and exclusive-USB boots. It never writes SD or UI.
bool power_key_start();
bool power_key_requested();
bool power_key_released();
void power_key_cancel();
void power_key_status();
[[noreturn]] void power_cut_until_off();
