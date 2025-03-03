//
// Created by icybear on 25-3-3.
//

#ifndef MAC_BLOOM_H
#define MAC_BLOOM_H

#define BLOOM_FILTER_SIZE 1024
#define BYTE_SIZE (BLOOM_FILTER_SIZE / 8)

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
#endif //MAC_BLOOM_H
