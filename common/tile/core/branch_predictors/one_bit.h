#pragma once

#include "branch_predictor.h"

#include <vector>

class OneBitBranchPredictor : public BranchPredictor
{
public:
   OneBitBranchPredictor(CoreModel* core_model);
   ~OneBitBranchPredictor();

   bool predict(IntPtr ip, IntPtr target);
   void update(bool prediction, bool actual, IntPtr ip, IntPtr target);

private:
   std::vector<bool> _bits;
};
