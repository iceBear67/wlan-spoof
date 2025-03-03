//
// Created by icybear on 25-3-2.
//

#ifndef MACRING_H
#define MACRING_H
#include <cstdint>
#include <cstring>

#define MAC_ADDRESS_RING_SIZE 682 // 682 * 6 == 4092, 1 page in flash.

typedef struct {
    uint8_t addr[6];
} MacAddress;

#define BLOOM_FILTER_SIZE 1024  // Number of bits
#define BYTE_SIZE (BLOOM_FILTER_SIZE / 8)  // Convert to bytes
#define HASH_COUNT 3  // Number of hash functions

class BloomFilter {
    uint8_t filter[BYTE_SIZE]{};

    static size_t hash(const uint8_t mac[6]) {
        return (mac[5] * 31 + mac[4] * 29 + mac[3] * 23 + mac[2] * 19 + mac[1] * 17 + mac[0] * 13) % BLOOM_FILTER_SIZE;
    }

    void setBit(size_t index) {
        filter[index / 8] |= (1 << (index % 8));
    }

    [[nodiscard]] bool checkBit(size_t index) const {
        return filter[index / 8] & (1 << (index % 8));
    }

public:
    BloomFilter() {
        memset(filter, 0, BYTE_SIZE);
    }

    void insert(const uint8_t mac[6]) {
        setBit(hash(mac));
    }

    bool contains(const uint8_t mac[6]) const {
        return checkBit(hash(mac));
    }
};

class UniqueMacRing {
    int current = 0;
    int macRingSize = 0;
    MacAddress addressRing[MAC_ADDRESS_RING_SIZE] = {};
    BloomFilter bloomFilter = {};
    bool dirty = false;
  public:
    bool push(MacAddress address) {
#ifdef FILTER_RANDOM_MAC
        uint8_t last = address.addr[0] & 0xF;
        bool isRandom = last == 0x2 || last == 0xA ||
               last == 0xE || last == 0x6;
        if(isRandom) return false;
#endif
        if(!bloomFilter.contains(address.addr)) {
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

    MacAddress* pop() {
        return &addressRing[current++ % (MAC_ADDRESS_RING_SIZE )];
    }

    void dump(Preferences *pref) {
        if(dirty) {
            dirty = false;
            pref->putBytes("mac_dump", &addressRing, sizeof(addressRing));
        }
    }

    void load(Preferences *pref) {
        // counter wouldn't get updated. These data are for debugging purpose only, they
        // are equal to trash in memory.
        pref->getBytes("mac_dump", &addressRing, sizeof(addressRing));
    }

    void dumpToSerial() {
        int count=0;
        for (auto & i : addressRing) {
            auto mac = i.addr;
            if(mac[0] == 0 && mac[1] == 0 && mac[2] == 0 && mac[3] == 0 && mac[4] == 0) continue;
            count++;
            char addr[18];
            sprintf(addr, "%02X:%02X:%02X:%02X:%02X:%02X",
                    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
            Serial.println(addr);
        }
        log_i("%d known addresses in total.", count);
    }
};
#endif