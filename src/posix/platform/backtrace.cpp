/*
 *  Copyright (c) 2020, The OpenThread Authors.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of the copyright holder nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file
 *   This file implements backtrace for posix.
 *
 */

#include "platform-posix.h"

#include <execinfo.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "common/code_utils.hpp"
#include "common/logging.hpp"

#ifdef __ANDROID__
#include <cxxabi.h>
#include <dlfcn.h>
#include <unwind.h>

struct android_backtrace_state
{
    void **current;
    void **end;
};

_Unwind_Reason_Code android_unwind_callback(struct _Unwind_Context *context, void *arg)
{
    android_backtrace_state *state = (android_backtrace_state *)arg;
    uintptr_t                pc    = _Unwind_GetIP(context);
    if (pc)
    {
        if (state->current == state->end)
        {
            return _URC_END_OF_STACK;
        }
        else
        {
            *state->current++ = reinterpret_cast<void *>(pc);
        }
    }
    return _URC_NO_REASON;
}

void dump_stack(void)
{
    const int               max = 100;
    void *                  buffer[max];
    android_backtrace_state state;

    otLogCritPlat("android stack dump -------------------------------------->");

    state.current = buffer;
    state.end     = buffer + max;

    _Unwind_Backtrace(android_unwind_callback, &state);

    int count = (int)(state.current - buffer);

    for (int idx = 0; idx < count; idx++)
    {
        const void *addr   = buffer[idx];
        const char *symbol = "";

        Dl_info info;
        if (dladdr(addr, &info) && info.dli_sname)
        {
            symbol = info.dli_sname;
        }
        int   status    = 0;
        char *demangled = __cxxabiv1::__cxa_demangle(symbol, 0, 0, &status);

        otLogCritPlat("%03d: 0x%p %s", idx, addr, (NULL != demangled && 0 == status) ? demangled : symbol);

        if (NULL != demangled)
            free(demangled);
    }

    otLogCritPlat("android stack dump done ---------------------------------->\r\n\r\n");
}

static void signalCritical(int sig, siginfo_t *info, void *ucontext)
{
    OT_UNUSED_VARIABLE(ucontext);
    OT_UNUSED_VARIABLE(info);
    OT_UNUSED_VARIABLE(sig);

    dump_stack();
}

#else

static void signalCritical(int sig, siginfo_t *info, void *ucontext)
{
    OT_UNUSED_VARIABLE(ucontext);
    OT_UNUSED_VARIABLE(info);

    void * stackBuffer[OPENTHREAD_POSIX_CONFIG_BACKTRACE_STACK_DEPTH];
    void **stack = stackBuffer;
    char **stackSymbols;
    int    stackDepth;

    stackDepth = backtrace(stack, OPENTHREAD_POSIX_CONFIG_BACKTRACE_STACK_DEPTH);

    // Load up the symbols individually, so we can output to syslog, too.
    stackSymbols = backtrace_symbols(stack, stackDepth);
    VerifyOrExit(stackSymbols != nullptr, OT_NOOP);

    fprintf(stderr, " *** FATAL ERROR: Caught signal %d (%s):\n", sig, strsignal(sig));
    otLogCritPlat(" *** FATAL ERROR: Caught signal %d (%s):", sig, strsignal(sig));

    for (int i = 0; i < stackDepth; i++)
    {
        fprintf(stderr, "Backtrace %2d: %s\n", i, stackSymbols[i]);
        otLogCritPlat("Backtrace %2d: %s\n", i, stackSymbols[i]);
    }

    free(stackSymbols);

exit:
    exit(EXIT_FAILURE);
}
#endif

void platformBacktraceInit(void)
{
    struct sigaction sigact;

    sigact.sa_sigaction = &signalCritical;
    sigact.sa_flags     = SA_RESTART | SA_SIGINFO | SA_NOCLDWAIT;

    sigaction(SIGABRT, &sigact, (struct sigaction *)NULL);
    sigaction(SIGILL, &sigact, (struct sigaction *)NULL);
    sigaction(SIGSEGV, &sigact, (struct sigaction *)NULL);
    sigaction(SIGBUS, &sigact, (struct sigaction *)NULL);
}
