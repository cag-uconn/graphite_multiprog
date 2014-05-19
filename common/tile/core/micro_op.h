#pragma once

class MicroOp
{
public:
   enum Type
   {
      LOAD,
      EXEC,
      STORE,
      LFENCE,
      SFENCE,
      MFENCE
   };

   MicroOp(Type type): _type(type) {}
   ~MicroOp() {}

   Type getType() const
   { return _type; }

private:
   Type _type;
};

typedef vector<MicroOp> MicroOpList;
