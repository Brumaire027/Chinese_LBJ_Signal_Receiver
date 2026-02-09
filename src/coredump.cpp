//
// Created by FLN1021 on 2023/10/31.
// Migrated from https://github.com/MathieuDeprez/ESP32_CoreDump_Arduino_1.0.6/blob/master/src/main.cpp
//

#include "coredump.h"

void readCoreDump() {
    size_t size = 0;
    size_t address = 0;
    if (esp_core_dump_image_get(&address, &size) == ESP_OK) {
        Serial.println("[CoreDump] Find core dump.");
        have_cd = true;
        const esp_partition_t *pt;
        pt = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_COREDUMP, "coredump");

        if (pt != nullptr) {
            uint8_t bf[256];
            char str_dst[640];
            size_t toRead;
            // "-------------------------------------------------"
            sd1.append("========================= BEGIN COREDUMP =========================\n");
            sd1.beginCD("/COREDUMP");
            Serial.println("[CoreDump] ========================= BEGIN COREDUMP =========================");
            for (size_t i = 0; i < (size / 256) + 1; i++) {
                strcpy(str_dst, "");
                toRead = (size - i * 256) > 256 ? 256 : (size - i * 256);

                esp_err_t er = esp_partition_read(pt, i * 256, bf, toRead);
                if (er != ESP_OK) {
                    Serial.printf("FAIL [%x]\n", er);
                    break;
                }

                for (unsigned char j: bf) {
                    char str_tmp[3];
                    sprintf(str_tmp, "%02x", j);
                    strcat(str_dst, str_tmp);
                }

                Serial.printf("%s", str_dst);
                sd1.append("%s", str_dst);
                sd1.appendCD(bf, sizeof bf);
            }
            sd1.append("\n========================= END COREDUMP =========================\n");
            sd1.endCD();
            Serial.println("\n[CoreDump] ========================= END COREDUMP =========================");
        } else {
            Serial.println("[CoreDump] Core dump partition not found.");
        }
        if (esp_core_dump_image_erase() == ESP_OK)
            Serial.println("[CoreDump] Core dump erased.");
    } else {
        Serial.println("[CoreDump] No core dump available.");
    }
}