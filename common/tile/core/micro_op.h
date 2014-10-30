#pragma once

#include <stdint.h>
#include <string>
using std::string;

// Uop reg limits
#define MAX_UOP_SRC_REGS 2
#define MAX_UOP_DST_REGS 2

class MicroOp
{
public:
    // NOTE this uses strongly typed enums, a C++11 feature.
    // This saves a bunch of typecasts while keeping UopType enums 1-byte long.
    // If you use gcc < 4.6 or some other compiler,
    //  either go back to casting or lose compactness in the layout.
    enum Type : uint8_t {INVALID, GENERAL, BRANCH, LOAD, STORE, STORE_ADDR, FENCE};

    uint16_t rs[MAX_UOP_SRC_REGS];
    uint16_t rd[MAX_UOP_DST_REGS];
    uint16_t lat;        // Latency (in cycles)
    uint16_t decCycle;
    Type     type;       // 1 byte
    uint8_t  portMask;
    uint8_t  extraSlots; // FU exec slots (for units with >1 occupancy)
    uint8_t  pad;        // Pad to 4-byte multiple

    string getTypeStr() const;
    void clear();
};
// 16 bytes. TODO(dsm): check performance with wider operands
