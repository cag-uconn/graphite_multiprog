#include "simulator.h"
#include "one_bit.h"

OneBitBranchPredictor::OneBitBranchPredictor(UInt32 size)
   : m_bits(size)
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

bool OneBitBranchPredictor::predict(IntPtr ip, IntPtr target)
{
   UInt32 index = ip % m_bits.size();
   return m_bits[index];
}

void OneBitBranchPredictor::update(bool predicted, bool actual, IntPtr ip, IntPtr target)
{
   updateCounters(predicted, actual);
   UInt32 index = ip % m_bits.size();
   m_bits[index] = actual;
}
