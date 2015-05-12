/** $lic$
 * Copyright (C) 2012-2014 by Massachusetts Institute of Technology
 * Copyright (C) 2010-2013 by The Board of Trustees of Stanford University
 *
 * This file is part of zsim.
 *
 * zsim is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, version 2.
 *
 * If you use this software in your research, we request that you reference
 * the zsim paper ("ZSim: Fast and Accurate Microarchitectural Simulation of
 * Thousand-Core Systems", Sanchez and Kozyrakis, ISCA-40, June 2013) as the
 * source of the simulator in any publications that use this software, and that
 * you send us a citation of your work.
 *
 * zsim is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <stdint.h>
#include <vector>
#include "pin.H"

#include "micro_op.h"
#include "instruction.h"
#include "basic_block.h"

// Uncomment to get a count of BBLs run. This is currently used to get a
//  distribution of inaccurate instructions decoded that are actually run
// NOTE: This is not multiprocess-safe
// #define TARGET_PROFILING

// These are absolute maximums per instruction.
// If there is some non-conforming instruction,
// either increase these limits or treat it as a special case.
#define MAX_INSTR_LOADS      4
#define MAX_INSTR_REG_READS  4
#define MAX_INSTR_REG_WRITES 4
#define MAX_INSTR_STORES     4

// Technically, even full decoders produce 1-4 uops
// we increase this for common microsequenced instructions (e.g. xchg).
#define MAX_UOPS_PER_INSTR   12

// Temporary register offsets
#define REG_LOAD_TEMP       (REG_LAST + 1)  // REG_LAST defined by PIN
#define REG_STORE_TEMP      (REG_LOAD_TEMP + MAX_INSTR_LOADS)
#define REG_STORE_ADDR_TEMP (REG_STORE_TEMP + MAX_INSTR_STORES)
#define REG_EXEC_TEMP       (REG_STORE_ADDR_TEMP + MAX_INSTR_STORES)

#define MAX_REGISTERS       (REG_EXEC_TEMP + 64)

typedef std::vector<MicroOp> MicroOpVec;

// Nehalem-style decoder. Fully static for now
class Decoder
{
public:
    // Produces a BasicBlock/Instruction with MicroOps that can be used in OOO cores
    static Instruction* decodeInstruction(INS ins);
    static BasicBlock* decodeBasicBlock(BBL bbl);

#ifdef TARGET_PROFILING
    static void profileInstruction(uint64_t insIdx);
    static void profileBasicBlock(uint64_t bblIdx);
    static void dumpInstructionProfile();
    static void dumpBasicBlockProfile();
#endif

private:
    class Instr
    {
    public:
        explicit Instr(INS _ins);

        INS ins;

        uint32_t loadOps[MAX_INSTR_LOADS];
        uint32_t numLoads;

        // These contain the register indices; by convention, flags registers are stored last
        uint32_t inRegs[MAX_INSTR_REG_READS];
        uint32_t numInRegs;
        uint32_t outRegs[MAX_INSTR_REG_WRITES];
        uint32_t numOutRegs;

        uint32_t storeOps[MAX_INSTR_STORES];
        uint32_t numStores;

     private:
         // Put registers in some canonical order -- non-flags first
         void reorderRegs(uint32_t* regArray, uint32_t numRegs);
    };

    // Return true if inaccurate decoding, false if accurate
    static bool decode(INS ins, MicroOpVec& uops);

    // Every emit function can produce 0 or more uops; it returns the number of uops.
    // These are basic templates to make our life easier

    // By default, these emit to temporary registers that depend on the index;
    //  this can be overriden, e.g. for moves
    static void emitLoad(Instr& instr, uint32_t idx, MicroOpVec& uops, uint32_t destReg = 0);
    static void emitStore(Instr& instr, uint32_t idx, MicroOpVec& uops, uint32_t srcReg = 0);

    // Emit all loads and stores for this uop
    static void emitLoads(Instr& instr, MicroOpVec& uops);
    static void emitStores(Instr& instr, MicroOpVec& uops);

    // Emits a load-store fence uop
    static void emitFence(MicroOpVec& uops, uint32_t lat);

    static void emitExecUop(uint32_t rs0, uint32_t rs1, uint32_t rd0, uint32_t rd1,
                            MicroOpVec& uops, uint32_t lat, uint8_t ports, uint8_t extraSlots = 0);

    // Emit a branch uop
    static void emitBranchUop(uint32_t rs0, uint32_t rs1, uint32_t rd0, uint32_t rd1,
                              MicroOpVec& uops, uint32_t lat, uint8_t ports);

    // Instruction emits

    static void emitBasicMove(Instr& instr, MicroOpVec& uops, uint32_t lat, uint8_t ports);
    static void emitConditionalMove(Instr& instr, MicroOpVec& uops, uint32_t lat, uint8_t ports);

    // 1 "exec" uop, 0-2 inputs, 0-2 outputs
    static void emitBasicOp(Instr& instr, MicroOpVec& uops, uint32_t lat, uint8_t ports,
                            uint8_t extraSlots = 0, bool reportUnhandled = true);

    // >1 exec uops in a chain: each uop takes 2 inputs, produces 1 output to the next op
    // in the chain; the final op writes to the 0-2 outputs
    static void emitChainedOp(Instr& instr, MicroOpVec& uops, uint32_t numUops,
                              uint32_t* latArray, uint8_t* portsArray);

    // Some convert ops need 2 chained exec uops, though they have a single input and output
    static void emitConvert2Op(Instr& instr, MicroOpVec& uops, uint32_t lat1, uint32_t lat2,
                               uint8_t ports1, uint8_t ports2);

    // Specific cases
    static void emitXchg(Instr& instr, MicroOpVec& uops);
    static void emitMul(Instr& instr, MicroOpVec& uops);
    static void emitDiv(Instr& instr, MicroOpVec& uops);

    static void emitCompareAndExchange(Instr& instr, MicroOpVec& uops);

    // Other helper functions
    static void reportUnhandledCase(Instr& instr, const char* desc);
    static void populateRegArrays(Instr& instr, uint32_t* srcRegs, uint32_t* dstRegs);
    static void dropStackRegister(Instr& instr);
    static bool dropRegister(uint32_t targetReg, uint32_t* regs, uint32_t& numRegs);

    // Macro-op (ins) fusion - Only works if instrumentation proceeds at BBL level
    static bool canFuse(INS ins);
    static bool decodeFused(INS ins, MicroOpVec& uops);

#ifdef TARGET_PROFILING
    static void dumpApproxInstrProfile(uint64_t* approxOpcodeCount, uint32_t numOpcodes);
#endif
};
