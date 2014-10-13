#include "simulator.h"
#include "branch_predictor.h"
#include "branch_predictors/one_bit.h"
#include "branch_predictors/two_level.h"

BranchPredictor::BranchPredictor()
{
   initializeCounters();
}

BranchPredictor::~BranchPredictor()
{ }

UInt64 BranchPredictor::m_mispredict_penalty;

BranchPredictor* BranchPredictor::create()
{
   try
   {
      string type = cfg->getString("branch_predictor/type");
      if (type == "one_bit")
         return new OneBitBranchPredictor(core_model);
      else if (type == "two_level")
         return new TwoLevelBranchPredictor(core_model);
      else
      {
         LOG_PRINT_ERROR("Invalid branch predictor type.");
         return 0;
      }
   }
   catch (...)
   {
      LOG_PRINT_ERROR("Config info not available while constructing branch predictor.");
      return 0;
   }
}

UInt64 BranchPredictor::getMispredictPenalty()
{
   return m_mispredict_penalty;
}

void BranchPredictor::updateCounters(bool predicted, bool actual)
{
   if (predicted == actual)
      ++m_correct_predictions;
   else
      ++m_incorrect_predictions;
}

void BranchPredictor::initializeCounters()
{
   m_correct_predictions = 0;
   m_incorrect_predictions = 0;
}

void BranchPredictor::outputSummary(std::ostream &os)
{
   os << "    Branch Predictor Statistics:" << endl
      << "      Num Correct: " << _correct_predictions << endl
      << "      Num Incorrect: " << _incorrect_predictions << endl
      << "      Accuracy (%): " << 100.0 * _correct_predictions / (_correct_predictions + _incorrect_predictions) << endl;
}
