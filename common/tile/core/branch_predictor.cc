#include "simulator.h"
#include "core_model.h"
#include "core.h"
#include "branch_predictor.h"
#include "branch_predictors/one_bit.h"
#include "branch_predictors/two_level.h"

BranchPredictor::BranchPredictor(CoreModel* core_model)
   : _core_model(core_model)
{
   config::Config *cfg = Sim()->getCfg();
   try
   {
      _mispredict_penalty = cfg->getInt("branch_predictor/mispredict_penalty");
   }
   catch (...)
   {
      LOG_PRINT_ERROR("Config info not available while constructing branch predictor.");
   }
   initializeCounters();
}

BranchPredictor::~BranchPredictor()
{}

BranchPredictor* BranchPredictor::create(CoreModel* core_model)
{
   config::Config *cfg = Sim()->getCfg();
   try
   {
      string type = cfg->getString("branch_predictor/type");
      if (type == "one_bit")
         return new OneBitBranchPredictor(core_model);
      else if (type == "two_level")
         return new TwoLevelBranchPredictor(core_model);
      else
         LOG_PRINT_ERROR("Invalid branch predictor type.");
   }
   catch (...)
   {
      LOG_PRINT_ERROR("Config info not available while constructing branch predictor.");
   }
   return NULL;
}

bool
BranchPredictor::handle(uintptr_t address)
{
   const DynamicBranchInfo& info = _core_model->getDynamicBranchInfo();

   bool prediction = predict(address, info._target);
   bool correct = (prediction == info._taken);

   update(prediction, info._taken, address, info._target);
   updateCounters(prediction, info._taken);

   _core_model->popDynamicBranchInfo();
   return !correct;
}

void
BranchPredictor::updateCounters(bool prediction, bool actual)
{
   if (prediction == actual)
      ++_correct_predictions;
   else
      ++_incorrect_predictions;
}

void
BranchPredictor::initializeCounters()
{
   _correct_predictions = 0;
   _incorrect_predictions = 0;
}

void
BranchPredictor::outputSummary(std::ostream &os)
{
   os << "    Branch Predictor Statistics:" << endl
      << "      Num Correct: " << _correct_predictions << endl
      << "      Num Incorrect: " << _incorrect_predictions << endl
      << "      Accuracy (%): " << 100.0 * _correct_predictions / (_correct_predictions + _incorrect_predictions) << endl;
}
