//
// Created by icybear on 25-3-2.
//

#ifndef MACRING_H
#define MACRING_H
#include <cstdint>
#include <cstring>

#define MAC_ADDRESS_RING_SIZE 512

typedef struct {
    uint8_t addr[6];
} MacAddress;

#define BLOOM_FILTER_SIZE 1024  // Number of bits
#define BYTE_SIZE (BLOOM_FILTER_SIZE / 8)  // Convert to bytes
#define HASH_COUNT 3  // Number of hash functions

class BloomFilter {
    uint8_t filter[BYTE_SIZE]{};

    size_t hash(const uint8_t mac[6]) const {
        return (mac[5] * 31 + mac[4] * 29 + mac[3] * 23 + mac[2] * 19 + mac[1] * 17 + mac[0] * 13) % BLOOM_FILTER_SIZE;
    }

    void setBit(size_t index) {
        filter[index / 8] |= (1 << (index % 8));
    }

    bool checkBit(size_t index) const {
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
    BloomFilter bloom_filter = {};
  public:
    void push(MacAddress address) {
        if(!bloom_filter.contains(address.addr)) {
            addressRing[macRingSize % MAC_ADDRESS_RING_SIZE] = address;
            macRingSize++;
            bloom_filter.insert(address.addr);
        }
    }

    [[nodiscard]] bool hasNext() const {
        return current <= macRingSize;
    }

    MacAddress* pop() {
        return &addressRing[current++ % (MAC_ADDRESS_RING_SIZE )];
    }

    void dump(Preferences *pref) {
        pref->putBytes("mac_dump", &macRingSize, sizeof(macRingSize));
    }

    void load(Preferences *pref) {
        pref->getBytes("mac_dump", &macRingSize, sizeof(macRingSize));
    }

    void dumpToSerial() {
        for (int i = 0; i < MAC_ADDRESS_RING_SIZE; ++i) {
            auto mac = addressRing[i].addr;
            if(mac[0] == 0 && mac[1] == 0 && mac[2] == 0 && mac[3] == 0 && mac[4] == 0) continue;
            char addr[18];
            sprintf(addr, "%02X:%02X:%02X:%02X:%02X:%02X",
                    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
            Serial.println(addr);
        }
    }
};
#endif