#include "simulator.h"
#include "two_level.h"

TwoLevelBranchPredictor::TwoLevelBranchPredictor(CoreModel* core_model)
   : BranchPredictor(core_model)
{
   config::Config *cfg = Sim()->getCfg();
   uint32_t log_branch_history_table_size = 0;
   uint32_t log_pattern_history_table_size = 0;
   try
   {
      log_branch_history_table_size = cfg->getInt("branch_predictor/two_level/log_branch_history_table_size");
      log_pattern_history_table_size = cfg->getInt("branch_predictor/two_level/log_pattern_history_table_size");
   }
   catch (...)
   {
      LOG_PRINT_ERROR("Config info not available for one_bit branch predictor.");
   }
   uint32_t numBhsrs = 1 << log_branch_history_table_size;
   uint32_t phtSize = 1 << log_pattern_history_table_size;

   _bhsr = new uint32_t[numBhsrs];
   _pht = new uint8_t[phtSize];
   for (uint32_t i = 0; i < numBhsrs; i++)
      _bhsr[i] = 0;
   for (uint32_t i = 0; i < phtSize; i++)
      _pht[i] = 1; // weak non-taken
   
   _bhsrMask = numBhsrs - 1;
   _phtMask = phtSize - 1;
}

TwoLevelBranchPredictor::~TwoLevelBranchPredictor()
{}

bool
TwoLevelBranchPredictor::predict(IntPtr ip, IntPtr target)
{
   uint32_t bhsrIdx = ((uint32_t)(ip >> 1)) & _bhsrMask;
   uint32_t phtIdx = _bhsr[bhsrIdx];

   // Predict
   
   // If uncommented, behaves like a global history predictor
   // bhsrIdx = 0;
   // phtIdx = (_bhsr[bhsrIdx] ^ ((uint32_t) ip)) & _phtMask;
   bool predict = _pht[phtIdx] > 1;
   // LOG_PRINT("BP Pred: %#lx bshr[%d]=%x pht=%d pred=%d", ip, bhsrIdx, phtIdx, _pht[phtIdx], predict);
   return predict;
}

void
TwoLevelBranchPredictor::update(bool predict, bool actual, IntPtr ip, IntPtr target)
{
   uint32_t bhsrIdx = ((uint32_t)(ip >> 1)) & _bhsrMask;
   uint32_t phtIdx = _bhsr[bhsrIdx];

   // Update
   
   _pht[phtIdx] = actual ? (predict ? 3 : (_pht[phtIdx]+1)) : (predict ? (_pht[phtIdx]-1) : 0); // 2-bit saturating counter
   _bhsr[bhsrIdx] = ((_bhsr[bhsrIdx] << 1) & _phtMask) | (actual ? 1: 0); // we apply phtMask here, dependence is further away
   // LOG_PRINT("BP Update: %#lx predict=%d, actual=%d, newPht=%d newBshr=%x", predict, actual, _pht[phtIdx], _bhsr[bhsrIdx]);
}

