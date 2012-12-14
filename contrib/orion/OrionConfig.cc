#include "OrionConfig.h"

#include <iostream>
#include <string>

#include "ConfigFile.h"
#include "TechParameter.h"

using namespace std;

OrionConfig* OrionConfig::m_singleton = NULL;
void OrionConfig::allocate(string& orion_cfg_file_, uint32_t tech_node_)
{
   assert(!m_singleton);
   m_singleton = new OrionConfig(orion_cfg_file_, tech_node_);
}
void OrionConfig::release()
{
   assert(m_singleton);
   delete m_singleton;
}
OrionConfig* OrionConfig::getSingleton()
{
   assert(m_singleton);
   return m_singleton;
}

string OrionConfig::ms_param_name[] = {
  "TECH_NODE",
  "TRANSISTOR_TYPE",
  "VDD",
  "FREQUENCY",
  "NUM_INPUT_PORT",
  "NUM_OUTPUT_PORT",
  "FLIT_WIDTH",
  "NUM_VIRTUAL_CLASS",
  "NUM_VIRTUAL_CHANNEL",
  "IS_IN_SHARED_BUFFER",
  "IS_OUT_SHARED_BUFFER",
  "IS_IN_SHARED_SWITCH",
  "IS_OUT_SHARED_SWITCH",
  "IS_INPUT_BUFFER",
  "IN_BUF_MODEL",
  "IN_BUF_NUM_SET",
  "IN_BUF_NUM_READ_PORT",
  "IS_OUTPUT_BUFFER",
  "OUT_BUF_MODEL",
  "OUT_BUF_NUM_SET",
  "OUT_BUF_NUM_WRITE_PORT",
  "SRAM_NUM_DATA_END",
  "SRAM_ROWDEC_MODEL",
  "SRAM_ROWDEC_PRE_MODEL",
  "SRAM_WORDLINE_MODEL",
  "SRAM_BITLINE_PRE_MODEL",
  "SRAM_BITLINE_MODEL",
  "SRAM_AMP_MODEL",
  "SRAM_OUTDRV_MODEL",
  "CROSSBAR_MODEL",
  "CROSSBAR_CONNECT_TYPE",
  "CROSSBAR_TRANS_GATE_TYPE",
  "CROSSBAR_MUX_DEGREE",
  "CROSSBAR_NUM_IN_SEG",
  "CROSSBAR_NUM_OUT_SEG",
  "CROSSBAR_LEN_IN_WIRE",
  "CROSSBAR_LEN_OUT_WIRE",
  "VA_MODEL",
  "VA_IN_ARB_MODEL",
  "VA_IN_ARB_FF_MODEL",
  "VA_OUT_ARB_MODEL",
  "VA_OUT_ARB_FF_MODEL",
  "VA_BUF_MODEL",
  "SA_IN_ARB_MODEL",
  "SA_IN_ARB_FF_MODEL",
  "SA_OUT_ARB_MODEL",
  "SA_OUT_ARB_FF_MODEL",
  "WIRE_LAYER_TYPE",
  "WIRE_WIDTH_SPACING",
  "WIRE_BUFFERING_MODEL",
  "WIRE_IS_SHIELDING",
  "IS_HTREE_CLOCK",
  "ROUTER_DIAGONAL"
};

OrionConfig::OrionConfig(const string& cfg_fn_, uint32_t tech_node_)
{
  uint32_t num_param = sizeof(ms_param_name)/sizeof(string);

  for(uint32_t i = 0; i < num_param; i++)
  {
    m_params_map[ms_param_name[i]] = "NOT_SET";
  }

  read_file(cfg_fn_);
  m_params_map["TECH_NODE"] = T_as_string<uint32_t>(tech_node_);

  m_tech_param_ptr = new TechParameter(this);
}

OrionConfig::OrionConfig(const OrionConfig& orion_cfg_)
{
  m_params_map = orion_cfg_.m_params_map;
  m_num_in_port = orion_cfg_.m_num_in_port;
  m_num_out_port = orion_cfg_.m_num_out_port;
  m_num_vclass = orion_cfg_.m_num_vclass;
  m_num_vchannel = orion_cfg_.m_num_vchannel;
  m_in_buf_num_set = orion_cfg_.m_in_buf_num_set;
  m_flit_width = orion_cfg_.m_flit_width;

  m_tech_param_ptr = new TechParameter(this);
}

OrionConfig::~OrionConfig()
{
  delete m_tech_param_ptr;
}

void OrionConfig::set_num_in_port(uint32_t num_in_port_)
{
  m_params_map[string("NUM_INPUT_PORT")] = T_as_string<uint32_t>(num_in_port_);
  m_num_in_port = num_in_port_;
  return;
}

void OrionConfig::set_num_out_port(uint32_t num_out_port_)
{
  m_params_map[string("NUM_OUTPUT_PORT")] = T_as_string<uint32_t>(num_out_port_);
  m_num_out_port = num_out_port_;
  return;
}

void OrionConfig::set_num_vclass(uint32_t num_vclass_)
{
  m_params_map[string("NUM_VIRTUAL_CLASS")] = T_as_string<uint32_t>(num_vclass_);
  m_num_vclass = num_vclass_;
  return;
}

void OrionConfig::set_num_vchannel(uint32_t num_vchannel_)
{
  m_params_map[string("NUM_VIRTUAL_CHANNEL")] = T_as_string<uint32_t>(num_vchannel_);
  m_num_vchannel = num_vchannel_;
  return;
}

void OrionConfig::set_in_buf_num_set(uint32_t in_buf_num_set_)
{
  m_params_map[string("IN_BUF_NUM_SET")] = T_as_string<uint32_t>(in_buf_num_set_);
  m_in_buf_num_set = in_buf_num_set_;
  return;
}

void OrionConfig::set_flit_width(uint32_t flit_width_)
{
  m_params_map[string("FLIT_WIDTH")] = T_as_string<uint32_t>(flit_width_);
  m_flit_width = flit_width_;
  return;
}

void OrionConfig::set_frequency(float frequency_)
{
   m_params_map[string("FREQUENCY")] = T_as_string<float>(frequency_);
}

void OrionConfig::read_file(const string& filename_)
{
  ConfigFile cfg_file(filename_);

  uint32_t num_param = sizeof(ms_param_name)/sizeof(string);
  for(uint32_t i = 0; i < num_param; i++)
  {
    cfg_file.readInto(m_params_map[ms_param_name[i]], ms_param_name[i]);
  }

  m_num_in_port = get<uint32_t>("NUM_INPUT_PORT");
  m_num_out_port = get<uint32_t>("NUM_OUTPUT_PORT");
  m_num_vclass = get<uint32_t>("NUM_VIRTUAL_CLASS");
  m_num_vchannel = get<uint32_t>("NUM_VIRTUAL_CHANNEL");
  m_in_buf_num_set = get<uint32_t>("IN_BUF_NUM_SET");
  m_flit_width = get<uint32_t>("FLIT_WIDTH");
  return;
}

void OrionConfig::print_config(ostream& out_)
{
  uint32_t num_param = sizeof(ms_param_name)/sizeof(string);

  for(uint32_t i = 0; i < num_param; i++)
  {
    out_ << ms_param_name[i] << " = " << m_params_map[ms_param_name[i]] << endl;
  }
  return;
}
