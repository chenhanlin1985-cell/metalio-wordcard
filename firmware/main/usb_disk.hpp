#pragma once
// One-shot RTC request, consumed only after our deliberate software restart.
bool usb_disk_boot_requested();
[[noreturn]] void usb_disk_request_boot();
[[noreturn]] void usb_disk_run();
