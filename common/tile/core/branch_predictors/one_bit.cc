#include "simulator.h"
#include "one_bit.h"

OneBitBranchPredictor::OneBitBranchPredictor(CoreModel* core_model)
   : BranchPredictor(core_model)
{
   config::Config *cfg = Sim()->getCfg();
   uint32_t log_branch_history_table_size = 0;
   try
   {
      log_branch_history_table_size = cfg->getInt("branch_predictor/one_bit/log_branch_history_table_size");
   }
   catch (...)
   {
      LOG_PRINT_ERROR("Config info not available for one_bit branch predictor.");
   }
   _bits = vector<bool>(1 << log_branch_history_table_size);
}

OneBitBranchPredictor::~OneBitBranchPredictor()
{
}

bool
OneBitBranchPredictor::predict(IntPtr ip, IntPtr target)
{
   UInt32 index = ip % _bits.size();
   return _bits[index];
}

void
OneBitBranchPredictor::update(bool prediction, bool actual, IntPtr ip, IntPtr target)
{
   UInt32 index = ip % _bits.size();
   _bits[index] = actual;
}
