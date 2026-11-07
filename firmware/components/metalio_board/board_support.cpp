#include "config.h"
#include "metalio_sd.h"
extern "C" void MetalioSd_ReleaseHold(void) {
    const gpio_num_t pins[] = {SDMMC_CLK_PIN, SDMMC_CMD_PIN, SDMMC_D0_PIN,
                               SDMMC_D1_PIN,  SDMMC_D2_PIN,  SDMMC_D3_PIN};
    for (auto pin : pins)
        gpio_hold_dis(pin);
}
