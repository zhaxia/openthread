/*
 *    Copyright 2016 Nest Labs Inc. All Rights Reserved.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#ifndef TASKLET_HPP_
#define TASKLET_HPP_

#include <common/thread_error.hpp>

namespace Thread {

class Tasklet
{
    friend class TaskletScheduler;

public:
    typedef void (*Handler)(void *context);
    Tasklet(Handler handler, void *context);
    ThreadError Post();

private:
    void RunTask() { m_handler(m_context); }

    Handler m_handler;
    void *m_context;
    Tasklet *m_next;
};

class TaskletScheduler
{
public:
    static ThreadError Post(Tasklet &tasklet);
    static void Run();

private:
    static Tasklet *PopTasklet();
};

}  // namespace Thread

#endif  // TASKLET_HPP_
