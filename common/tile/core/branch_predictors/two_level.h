#pragma once

#include "branch_predictor.h"

// Per-address branch history, global pattern history

class TwoLevelBranchPredictor : public BranchPredictor
{
public:
   TwoLevelBranchPredictor(CoreModel* core_model);
   ~TwoLevelBranchPredictor();

   bool predict(IntPtr ip, IntPtr target);
   void update(bool predict, bool actual, IntPtr ip, IntPtr target);

private:
   uint32_t* _bhsr;
   uint8_t*  _pht;
   uint32_t  _bhsrMask;
   uint32_t  _phtMask;
};
