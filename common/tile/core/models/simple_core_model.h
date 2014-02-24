#ifndef SIMPLE_CORE_MODEL_H
#define SIMPLE_CORE_MODEL_H

#include "core_model.h"

class SimpleCoreModel : public CoreModel
{
public:
   SimpleCoreModel(Core *core);
   ~SimpleCoreModel();

   void outputSummary(std::ostream &os, const Time& target_completion_time);

private:
   void handleInstruction(Instruction *instruction);
};

#endif
