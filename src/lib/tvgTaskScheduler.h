/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd. All rights reserved.

 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#ifndef _TVG_TASK_SCHEDULER_H_
#define _TVG_TASK_SCHEDULER_H_

#include <future>
#include "tvgCommon.h"

namespace tvg
{

struct Task
{
private:
    promise<void> sender;
    future<void>  receiver;

public:
    virtual ~Task() = default;

    void get()
    {
        if (receiver.valid()) {
            receiver.get();
        }
    }

    bool valid()
    {
        return receiver.valid();
    }

protected:
    virtual void run() = 0;

private:
    void operator()()
    {
        run();
        sender.set_value();
    }

    void prepare()
    {
        sender = promise<void>();
        receiver = sender.get_future();
    }

    friend class TaskSchedulerImpl;
};

struct TaskScheduler
{
    static void init(unsigned threads);
    static void term();
    static void request(Task* task);
};

}

#endif //_TVG_TASK_SCHEDULER_H_