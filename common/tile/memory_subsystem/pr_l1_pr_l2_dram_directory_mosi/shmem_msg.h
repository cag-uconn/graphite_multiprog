#pragma once

#include "mem_component.h"
#include "common_types.h"
#include "../shmem_msg.h"

namespace PrL1PrL2DramDirectoryMOSI
{
   class ShmemMsg : public ::ShmemMsg
   {
   public:
      enum Type
      {
         INVALID,
         EX_REQ,
         SH_REQ,
         INV_REQ,
         FLUSH_REQ,
         WB_REQ,
         INV_FLUSH_COMBINED_REQ,
         EX_REP,
         SH_REP,
         UPGRADE_REP,
         INV_REP,
         FLUSH_REP,
         WB_REP,
         NULLIFY_REQ
      }; 

      ShmemMsg();
      ShmemMsg(Type msg_type,
               MemComponent::Type sender_mem_component,
               MemComponent::Type receiver_mem_component,
               tile_id_t requester,
               tile_id_t single_receiver,
               IntPtr address,
               bool modeled);
      ShmemMsg(Type msg_type,
               MemComponent::Type sender_mem_component,
               MemComponent::Type receiver_mem_component,
               tile_id_t requester,
               tile_id_t single_receiver,
               IntPtr address,
               const Byte* data_buf,
               UInt32 data_length,
               bool modeled);
      ShmemMsg(const ShmemMsg& shmem_msg);
      explicit ShmemMsg(const Byte* msg_buf);
      
      ~ShmemMsg();

      void makeMsgBuf(Byte* msg_buf) const;
      UInt32 getMsgLen() const;

      // Get the msg type as a string
      static string getName(Type type);

      // Modeled Parameters
      UInt32 getModeledLength();

      Type getType() const                               { return _msg_type; }
      MemComponent::Type getSenderMemComponent() const   { return _sender_mem_component; }
      MemComponent::Type getReceiverMemComponent() const { return _receiver_mem_component; }
      tile_id_t getRequester() const                     { return _requester; }
      tile_id_t getSingleReceiver() const                { return _single_receiver; }
      IntPtr getAddress() const                          { return _address; }
      const Byte* getDataBuf() const                     { return _data_buf; }
      UInt32 getDataLength() const                       { return _data_length; }
      bool isModeled() const                             { return _modeled; }

      void setMsgType(Type msg_type)                     { _msg_type = msg_type; }

   private:   
      Type _msg_type;
      MemComponent::Type _sender_mem_component;
      MemComponent::Type _receiver_mem_component;
      tile_id_t _requester;
      tile_id_t _single_receiver;
      IntPtr _address;
      const Byte* _data_buf;
      UInt32 _data_length;
      bool _modeled;

      static const UInt32 _num_msg_type_bits = 4;
   };

   #define SPELL_SHMSG(x)        (ShmemMsg::getName(x).c_str())

}
