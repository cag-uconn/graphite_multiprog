#pragma once

#include <iostream>

class CoreModel;

#include "fixed_types.h"
#include "time_types.h"

class BranchPredictor
{
public:
   BranchPredictor(CoreModel* core_model);
   virtual ~BranchPredictor();

   static BranchPredictor* create(CoreModel* core_model);
   virtual void outputSummary(std::ostream &os);

   uint16_t getMispredictPenalty() { return _mispredict_penalty; }
   bool handle(uintptr_t address);

   uint64_t getNumCorrectPredictions()    { return _correct_predictions; }
   uint64_t getNumIncorrectPredictions()  { return _incorrect_predictions; }

private:
   virtual bool predict(IntPtr ip, IntPtr target) = 0;
   virtual void update(bool prediction, bool actual, IntPtr ip, IntPtr target) = 0;
   void initializeCounters();
   void updateCounters(bool prediction, bool actual);

   CoreModel* _core_model;
   uint16_t _mispredict_penalty;
   uint64_t _correct_predictions;
   uint64_t _incorrect_predictions;
};
