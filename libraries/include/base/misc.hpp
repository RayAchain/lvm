#ifndef _MISC_H_
#define _MISC_H_

#define CLI_ARGS_MAX_CHARS  4*1024


struct TaskBase;
struct TaskImplResult;

class ITaskImplementFinishNotify {
public:
    virtual void task_finished(TaskImplResult* task) = 0;
};

struct TaskAndCallback{
    TaskBase* task_base;
    ITaskImplementFinishNotify* call_back;
};

#endif
