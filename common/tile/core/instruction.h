#pragma once

#include <string>
using std::string;

#include "common_types.h"
#include "time_types.h"
#include "mcpat_info.h"
#include "micro_op.h"

// forward declaration
class CoreModel;

class Instruction
{
public:
   Instruction(uintptr_t address, uint32_t size,
               uint32_t nUops, MicroOp* uopArray);
   
   uintptr_t getAddress() const              { return _address;      }
   uint32_t getSize() const                  { return _size;         }
   uint32_t getNumUops() const               { return _nUops;        }
   const MicroOp& getUop(uint32_t i) const   { return _uopArray[i];  }
   const McPATInfo* getMcPATInfo() const     { return _mcpatInfo;    }
   void setMcPATInfo(McPATInfo* info)        { _mcpatInfo = info;    }
#ifdef TARGET_PROFILING
   void setCount(uint64_t c)                 { _count = c;           }
   void incrCount()                          { _count ++;            }
   void setApproxOpcode(uint32_t op)         { _approxOpcode = op;   }
#endif

private:
   uintptr_t _address;
   uint32_t _size;
   uint32_t _nUops;
   MicroOp* _uopArray;
   const McPATInfo* _mcpatInfo;
#ifdef TARGET_PROFILING
   uint64_t _count;
   uint32_t _approxOpcode;
#endif
};

// For operations not associated with the binary -- such as processing a packet
class DynamicInstruction
{
public:
   enum Type
   {
      NETRECV,
      SYNC,
      SPAWN
   };

   DynamicInstruction(Type type, const Time& cost);

   Type getType() const    { return _type; }
   Time getCost() const    { return _cost; }
   string getTypeStr() const;

private:
   Type _type;
   Time _cost;
};

// NetRecvInstruction - called for netRecv
class NetRecvInstruction : public DynamicInstruction
{
public:
   NetRecvInstruction(const Time& cost)
      : DynamicInstruction(NETRECV, cost)
   {}
};

// SyncInstruction - called for SYNC instructions
class SyncInstruction : public DynamicInstruction
{
public:
   SyncInstruction(const Time& cost)
      : DynamicInstruction(SYNC, cost)
   {}
};

// SpawnInstruction - set clock to particular time at the start of a thread
class SpawnInstruction : public DynamicInstruction
{
public:
   SpawnInstruction(const Time& cost)
      : DynamicInstruction(SPAWN, cost)
   {}
};
