#include <base/exceptions.hpp>
#include <glua/glua_contractoperations.hpp>
#include <glua/thinkyoung_lua_lib.h>

using thinkyoung::lua::api::global_glua_chain_api;

static void setGluaStateScopeValue(thinkyoung::lua::lib::GluaStateScope& scope,
                                   const std::string& str_caller,
                                   const std::string& str_caller_addr,
                                   const GluaStateValue& statevalue,
                                   const size_t limit_num) {
    thinkyoung::lua::lib::add_global_string_variable(scope.L(), "caller", str_caller.c_str());
    thinkyoung::lua::lib::add_global_string_variable(scope.L(), "caller_address", str_caller_addr.c_str());
    thinkyoung::lua::lib::set_lua_state_value(scope.L(), "evaluate_state", statevalue, GluaStateValueType::LUA_STATE_VALUE_POINTER);
    global_glua_chain_api->clear_exceptions(scope.L());
    scope.set_instructions_limit(limit_num);
}

void RegisterContractOperation::evaluate(TaskAndCallback& _inst_taskandcallback) const  {
}

void UpgradeContractOperation::evaluate(TaskAndCallback& _inst_taskandcallback) const  {
}

void DestroyContractOperation::evaluate(TaskAndCallback& _inst_taskandcallback) const {
}

void CallContractOperation::evaluate(TaskAndCallback& _inst_taskandcallback) const {
    using namespace thinkyoung::lua::lib;
    GluaStateScope scope;
    GluaStateValue statevalue;
    size_t limit_num = 0;
    statevalue.pointer_value = nullptr;
    std::string str_tmp_caller;
    std::string str_tmp_caller_addr;
    std::string str_tmp_method;
    std::string str_tmp_args;
    std::string str_tmp_contract_addr;
    int exception_code = 0;
    std::string exception_msg;
    setGluaStateScopeValue(scope, str_tmp_caller, str_tmp_caller_addr, statevalue, limit_num);
    scope.execute_contract_api_by_address(str_tmp_contract_addr.c_str(),
                                          str_tmp_method.c_str(),
                                          str_tmp_args.c_str(),
                                          nullptr);
                                          
    if (scope.L()->force_stopping == true && scope.L()->exit_code == LUA_API_INTERNAL_ERROR) {
        FC_CAPTURE_AND_THROW(thinkyoung::blockchain::lua_executor_internal_error, (""));
    }
    
    exception_code = get_lua_state_value(scope.L(), "exception_code").int_value;
    
    if (exception_code > 0) {
        exception_msg = (char*)get_lua_state_value(scope.L(), "exception_msg").string_value;
        
        if (exception_code == THINKYOUNG_API_LVM_LIMIT_OVER_ERROR) {
            FC_CAPTURE_AND_THROW(thinkyoung::blockchain::contract_run_out_of_money);
            
        } else {
            thinkyoung::blockchain::contract_error con_err(32000, "exception", exception_msg);
            throw con_err;
        }
    }
    
    int left = limit_num - scope.get_instructions_executed_count();
}

void TransferContractOperation::evaluate(TaskAndCallback& _inst_taskandcallback) const {
}