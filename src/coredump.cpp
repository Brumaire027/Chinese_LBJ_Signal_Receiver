#include "coredump.h"

void checkCoreDumpPresence() {
    size_t address = 0;
    size_t size = 0;
    have_cd = esp_core_dump_image_get(&address, &size) == ESP_OK;
    // Retain on-chip diagnostic evidence. Normal boot must not create SD dump
    // files, copy binary dumps into text logs, or automatically erase flash.
    if (have_cd)
        Serial.println("[CoreDump] Image present; automatic SD export disabled.");
}
