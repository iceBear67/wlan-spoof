//
// Created by icybear on 25-3-2.
//

#ifndef MACRING_H
#define MACRING_H
#include "mac_bloom.h"
#include <mac.h>

#define MAC_ADDRESS_RING_SIZE 682 // 682 * 6 == 4092, 1 page in flash.


class UniqueMacRing {
    int current = 0;
    int macRingSize = 0;
    MacAddress addressRing[MAC_ADDRESS_RING_SIZE] = {};
    BloomFilter bloomFilter = {};
    bool dirty = false;

public:
    bool push(MacAddress address) {
#ifdef FILTER_RANDOM_MAC
        if(isRandomMac(address.addr)) {
            return false;
        }
#endif
        if (!bloomFilter.contains(address.addr)) {
            addressRing[macRingSize % MAC_ADDRESS_RING_SIZE] = address;
            macRingSize++;
            bloomFilter.insert(address.addr);
            dirty = true;
            return true;
        }
        return false;
    }

    [[nodiscard]] bool hasNext() const {
        return current <= macRingSize;
    }

    MacAddress *pop() {
        return &addressRing[current++ % (MAC_ADDRESS_RING_SIZE)];
    }

    void dump(Preferences *pref) {
        if (dirty) {
            dirty = false;
            pref->putBytes("mac_dump", &addressRing, sizeof(addressRing));
            pref->putUInt("mac_ring_size", macRingSize);
            pref->putUInt("mac_ring_current", current);
        }
    }

    void load(Preferences *pref) {
        pref->getBytes("mac_dump", &addressRing, sizeof(addressRing));
        macRingSize = pref->getInt("mac_ring_size", macRingSize);
        current = pref->getInt("mac_ring_current", current);
    }

    void dumpToSerial() {
        int count = 0;
        for (auto &i: addressRing) {
            auto mac = i.addr;
            if (mac[0] == 0 && mac[1] == 0 && mac[2] == 0 && mac[3] == 0 && mac[4] == 0) continue;
            count++;
            auto str = macToString(mac);
            if (isRandomMac(mac)) {
                printf("%s (random)\n", str.c_str());
            } else {
                Serial.println(str.c_str());
            }
        }
        log_i("%d known addresses in total.", count);
    }
};
#endif
