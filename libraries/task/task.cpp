#include <task/task.hpp>

#include <fc/time.hpp>

#include <rpc/rpc_msg.hpp>

#include <sstream>

TaskBase::TaskBase() 
    : task_type(COMPILE_TASK), 
      task_from(FROM_CLI) {
    fc::time_point sec_now = fc::time_point::now();
    task_id = sec_now.sec_since_epoch();
}

TaskImplResult::TaskImplResult() {
    error_code = 0;
    error_msg = "";
}

void TaskImplResult::init_task_base(TaskBase* task) {
    task_id = task->task_id;
    task_type = task->task_type;
    task_from = task->task_from;
}

CompileTaskResult::CompileTaskResult(TaskBase* task) {
    init_task_base(task);
}

std::string TaskImplResult::get_result_string() {
    std::stringstream stream_result;
    stream_result << "\n";
    stream_result << "  task_id : " << task_id << " \n";
    stream_result << "  task_type : " << task_type << " \n";
    stream_result << "  task_from : " << (int)task_from << " \n";
    stream_result << "  error_code : " << (int)error_code << " \n";
    stream_result << "  error_msg : " << error_msg << " \n";

    return stream_result.str();
}

std::string CompileTaskResult::get_result_string() {
    std::string result = TaskImplResult::get_result_string();
    std::stringstream stream_result;
    stream_result << "  gpc_path_file : " << gpc_path_file;
    result += stream_result.str();
    result = "{" + result + "\n}";

    return result;
}

Message CompileTaskResult::get_rpc_message() {
    CompileTaskResultRpc reslut(*this);
    Message rpc_msg(reslut);
    rpc_msg.msg_id = reslut.data.task_id;
    return rpc_msg;
}

Message RegisterTaskResult::get_rpc_message() {
    RegisterTaskResultRpc reslut(*this);
    Message rpc_msg(reslut);
    rpc_msg.msg_id = reslut.data.task_id;
    return rpc_msg;
}

Message CallTaskResult::get_rpc_message() {
    CallTaskResultRpc reslut(*this);
    Message rpc_msg(reslut);
    rpc_msg.msg_id = reslut.data.task_id;
    return rpc_msg;
}

Message TransferTaskResult::get_rpc_message() {
    TransferTaskResultRpc reslut(*this);
    Message rpc_msg(reslut);
    rpc_msg.msg_id = reslut.data.task_id;
    return rpc_msg;
}

Message UpgradeTaskResult::get_rpc_message() {
    UpgradeTaskResultRpc reslut(*this);
    Message rpc_msg(reslut);
    rpc_msg.msg_id = reslut.data.task_id;
    return rpc_msg;
}

Message DestroyTaskResult::get_rpc_message() {
    DestroyTaskResultRpc reslut(*this);
    Message rpc_msg(reslut);
    rpc_msg.msg_id = reslut.data.task_id;
    return rpc_msg;
}


std::string RegisterTaskResult::get_result_string() {
    //TODO
    return NULL;
}

std::string CallTaskResult::get_result_string() {
    //TODO
    return NULL;
}

std::string TransferTaskResult::get_result_string() {
    //TODO
    return NULL;
}

std::string UpgradeTaskResult::get_result_string() {
    //TODO
    return NULL;
}

std::string DestroyTaskResult::get_result_string() {
    //TODO
    return NULL;
}
