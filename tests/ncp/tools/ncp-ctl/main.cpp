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

#include <arpa/inet.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <cmdline.h>
#include <ncp/ncp.pb-c.h>

#include <common/code_utils.hpp>
#include <common/thread_error.hpp>

namespace Thread {

enum
{
    kModeRxOnWhenIdle      = 1 << 3,
    kModeSecureDataRequest = 1 << 2,
    kModeFFD               = 1 << 1,
    kModeFullNetworkData   = 1 << 0,
};

int hex2bin(const char *hex, uint8_t *bin, uint16_t bin_length)
{
    uint16_t hex_length = strlen(hex);
    const char *hex_end = hex + hex_length;
    uint8_t *cur = bin;
    uint8_t num_chars = hex_length & 1;
    uint8_t byte = 0;

    if ((hex_length + 1) / 2 > bin_length)
    {
        return -1;
    }

    while (hex < hex_end)
    {
        if ('A' <= *hex && *hex <= 'F')
        {
            byte |= 10 + (*hex - 'A');
        }
        else if ('a' <= *hex && *hex <= 'f')
        {
            byte |= 10 + (*hex - 'a');
        }
        else if ('0' <= *hex && *hex <= '9')
        {
            byte |= *hex - '0';
        }
        else
        {
            return -1;
        }

        hex++;
        num_chars++;

        if (num_chars >= 2)
        {
            num_chars = 0;
            *cur++ = byte;
            byte = 0;
        }
        else
        {
            byte <<= 4;
        }
    }

    return cur - bin;
}

ThreadError ProcessPrimitiveKey(ThreadControl *message)
{
    switch (message->primitive.value_case)
    {
    case THREAD_PRIMITIVE__VALUE_BYTES:
        for (unsigned i = 0; i < message->primitive.bytes.len; i++)
        {
            printf("%02x", message->primitive.bytes.data[i]);
        }

        printf("\n");
        break;

    default:
        break;
    }

    return kThreadError_None;
}

ThreadError ProcessPrimitiveKeySequence(ThreadControl *message)
{
    switch (message->primitive.value_case)
    {
    case THREAD_PRIMITIVE__VALUE_UINT32:
        printf("key_sequence: %d\n", message->primitive.uint32);
        break;

    default:
        break;
    }

    return kThreadError_None;
}

ThreadError ProcessPrimitiveMeshLocalPrefix(ThreadControl *message)
{
    switch (message->primitive.value_case)
    {
    case THREAD_PRIMITIVE__VALUE_BYTES:
        struct in6_addr in6_addr;
        memset(&in6_addr, 0, sizeof(in6_addr));
        memcpy(&in6_addr, message->primitive.bytes.data, message->primitive.bytes.len);

        char buf[80];

        if (inet_ntop(AF_INET6, &in6_addr, buf, sizeof(buf)) != NULL)
        {
            printf("%s/64\n", buf);
        }
        else
        {
            printf("error\n");
        }

        break;

    default:
        break;
    }

    return kThreadError_None;
}

ThreadError ProcessPrimitiveMode(ThreadControl *message)
{
    switch (message->primitive.value_case)
    {
    case THREAD_PRIMITIVE__VALUE_UINT32:
        printf("mode: ");

        if (message->primitive.uint32 & kModeRxOnWhenIdle)
        {
            printf("r");
        }

        if (message->primitive.uint32 & kModeSecureDataRequest)
        {
            printf("s");
        }

        if (message->primitive.uint32 & kModeFFD)
        {
            printf("d");
        }

        if (message->primitive.uint32 & kModeFullNetworkData)
        {
            printf("n");
        }

        printf("\n");
        break;

    default:
        break;
    }

    return kThreadError_None;
}

ThreadError ProcessPrimitiveStatus(ThreadControl *message)
{
    switch (message->primitive.value_case)
    {
    case THREAD_PRIMITIVE__VALUE_BOOL:
        if (message->primitive.bool_)
        {
            printf("status: up\n");
        }
        else
        {
            printf("status: down\n");
        }

        break;

    default:
        break;
    }

    return kThreadError_None;
}

ThreadError ProcessPrimitiveTimeout(ThreadControl *message)
{
    switch (message->primitive.value_case)
    {
    case THREAD_PRIMITIVE__VALUE_UINT32:
        printf("timeout: %d\n", message->primitive.uint32);
        break;

    default:
        break;
    }

    return kThreadError_None;
}

ThreadError ProcessPrimitiveState(ThreadControl *message)
{
    switch (message->primitive.value_case)
    {
    case THREAD_PRIMITIVE__VALUE_UINT32:
        printf("timeout: %d\n", message->primitive.uint32);
        break;

    default:
        break;
    }

    return kThreadError_None;
}

ThreadError ProcessPrimitiveChannel(ThreadControl *message)
{
    switch (message->primitive.value_case)
    {
    case THREAD_PRIMITIVE__VALUE_UINT32:
        printf("channel: %d\n", message->primitive.uint32);
        break;

    default:
        break;
    }

    return kThreadError_Parse;
}

ThreadError ProcessPrimitivePanId(ThreadControl *message)
{
    switch (message->primitive.value_case)
    {
    case THREAD_PRIMITIVE__VALUE_UINT32:
        printf("panid: 0x%02x\n", message->primitive.uint32);
        break;

    default:
        break;
    }

    return kThreadError_Parse;
}

ThreadError ProcessPrimitiveExtendedPanId(ThreadControl *message)
{
    switch (message->primitive.value_case)
    {
    case THREAD_PRIMITIVE__VALUE_BYTES:
    {
        uint8_t *bytes = message->primitive.bytes.data;
        printf("xpanid: %02x%02x%02x%02x%02x%02x%02x%02x\n",
               bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], bytes[6], bytes[7]);
        break;
    }

    default:
        break;
    }

    return kThreadError_Parse;
}

ThreadError ProcessPrimitiveNetworkName(ThreadControl *message)
{
    switch (message->primitive.value_case)
    {
    case THREAD_PRIMITIVE__VALUE_BYTES:
    {
        printf("netname: %s\n", message->primitive.bytes.data);
        break;
    }

    default:
        break;
    }

    return kThreadError_Parse;
}

ThreadError ProcessPrimitiveShortAddr(ThreadControl *message)
{
    switch (message->primitive.value_case)
    {
    case THREAD_PRIMITIVE__VALUE_UINT32:
        printf("shortaddr: 0x%02x\n", message->primitive.uint32);
        break;

    default:
        break;
    }

    return kThreadError_Parse;
}

ThreadError ProcessPrimitiveExtAddr(ThreadControl *message)
{
    switch (message->primitive.value_case)
    {
    case THREAD_PRIMITIVE__VALUE_BYTES:
    {
        uint8_t *bytes = message->primitive.bytes.data;
        printf("extaddr: %02x%02x%02x%02x%02x%02x%02x%02x\n",
               bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], bytes[6], bytes[7]);
        break;
    }

    default:
        break;
    }

    return kThreadError_Parse;
}

ThreadError ProcessPrimitive(ThreadControl *message)
{
    switch (message->primitive.type)
    {
    case THREAD_PRIMITIVE__TYPE__THREAD_KEY:
        ProcessPrimitiveKey(message);
        break;

    case THREAD_PRIMITIVE__TYPE__THREAD_KEY_SEQUENCE:
        ProcessPrimitiveKeySequence(message);
        break;

    case THREAD_PRIMITIVE__TYPE__THREAD_MESH_LOCAL_PREFIX:
        ProcessPrimitiveMeshLocalPrefix(message);
        break;

    case THREAD_PRIMITIVE__TYPE__THREAD_MODE:
        ProcessPrimitiveMode(message);
        break;

    case THREAD_PRIMITIVE__TYPE__THREAD_STATUS:
        ProcessPrimitiveStatus(message);
        break;

    case THREAD_PRIMITIVE__TYPE__THREAD_TIMEOUT:
        ProcessPrimitiveTimeout(message);
        break;

    case THREAD_PRIMITIVE__TYPE__IEEE802154_CHANNEL:
        ProcessPrimitiveChannel(message);
        break;

    case THREAD_PRIMITIVE__TYPE__IEEE802154_PANID:
        ProcessPrimitivePanId(message);
        break;

    case THREAD_PRIMITIVE__TYPE__IEEE802154_EXTENDED_PANID:
        ProcessPrimitiveExtendedPanId(message);
        break;

    case THREAD_PRIMITIVE__TYPE__IEEE802154_NETWORK_NAME:
        ProcessPrimitiveNetworkName(message);
        break;

    case THREAD_PRIMITIVE__TYPE__IEEE802154_SHORT_ADDR:
        ProcessPrimitiveShortAddr(message);
        break;

    case THREAD_PRIMITIVE__TYPE__IEEE802154_EXT_ADDR:
        ProcessPrimitiveExtAddr(message);
        break;

    default:
        break;
    }

    return kThreadError_None;
}

ThreadError ProcessState(ThreadControl *message)
{
    switch (message->state.state)
    {
    case THREAD_STATE__STATE__DETACHED:
        printf("state: detached\n");
        break;

    case THREAD_STATE__STATE__CHILD:
        printf("state: child\n");
        break;

    case THREAD_STATE__STATE__ROUTER:
        printf("state: router\n");
        break;

    case THREAD_STATE__STATE__LEADER:
        printf("state: leader\n");
        break;

    default:
        break;
    }

    return kThreadError_None;
}

ThreadError ProcessWhitelist(ThreadControl *message)
{
    ThreadWhitelist *whitelist = &message->whitelist;

    switch (whitelist->type)
    {
    case THREAD_WHITELIST__TYPE__STATUS:
        switch (whitelist->status)
        {
        case THREAD_WHITELIST__STATUS__DISABLE:
            printf("whitelist disabled\n");
            break;

        case THREAD_WHITELIST__STATUS__ENABLE:
            printf("whitelist enabled\n");
            break;

        default:
            break;
        }

        return kThreadError_None;

    case THREAD_WHITELIST__TYPE__LIST:
        printf("whitelist-get:\n");
        break;

    case THREAD_WHITELIST__TYPE__ADD:
        printf("whitelist-add:\n");
        break;

    case THREAD_WHITELIST__TYPE__DELETE:
        printf("whitelist-delete:\n");
        break;

    case THREAD_WHITELIST__TYPE__CLEAR:
        printf("whitelist-clear:\n");
        break;

    default:
        break;
    }

    for (unsigned i = 0; i < whitelist->n_address; i++)
    {
        for (unsigned j = 0; j < whitelist->address[i].len; j++)
        {
            printf("%02x", whitelist->address[i].data[j]);
        }

        printf("\n");
    }

    return kThreadError_None;
}

ThreadError ProcessThreadControl(uint8_t *buf, uint16_t buf_length)
{
    ThreadError error = kThreadError_None;
    ThreadControl thread_control;

    VerifyOrExit(thread_control__unpack(buf_length, buf, &thread_control) != NULL,
                 printf("protobuf unpack error\n"); error = kThreadError_Parse);

    switch (thread_control.message_case)
    {
    case THREAD_CONTROL__MESSAGE_PRIMITIVE:
        ProcessPrimitive(&thread_control);
        break;

    case THREAD_CONTROL__MESSAGE_STATE:
        ProcessState(&thread_control);
        break;

    case THREAD_CONTROL__MESSAGE_WHITELIST:
        ProcessWhitelist(&thread_control);
        break;

    default:
        break;
    }

exit:
    return error;
}

}  // namespace Thread

int main(int argc, char *argv[])
{
    struct gengetopt_args_info args_info;

    if (cmdline_parser(argc, argv, &args_info) != 0)
    {
        return -1;
    }

    ThreadControl thread_control;

    thread_control__init(&thread_control);

    if (args_info.key_given)
    {
        thread_control.message_case = THREAD_CONTROL__MESSAGE_PRIMITIVE;
        thread_primitive__init(&thread_control.primitive);
        thread_control.primitive.type = THREAD_PRIMITIVE__TYPE__THREAD_KEY;

        if (args_info.key_orig != NULL)
        {
            thread_control.primitive.value_case = THREAD_PRIMITIVE__VALUE_BYTES;
            thread_control.primitive.bytes.len =
                Thread::hex2bin(args_info.key_arg, thread_control.primitive.bytes.data,
                                sizeof(thread_control.primitive.bytes.data));
        }
    }

    if (args_info.key_sequence_given)
    {
        thread_control.message_case = THREAD_CONTROL__MESSAGE_PRIMITIVE;
        thread_primitive__init(&thread_control.primitive);
        thread_control.primitive.type = THREAD_PRIMITIVE__TYPE__THREAD_KEY_SEQUENCE;

        if (args_info.key_sequence_orig != NULL)
        {
            thread_control.primitive.value_case = THREAD_PRIMITIVE__VALUE_UINT32;
            thread_control.primitive.uint32 = args_info.key_sequence_arg;
        }
    }

    if (args_info.prefix_given)
    {
        thread_control.message_case = THREAD_CONTROL__MESSAGE_PRIMITIVE;
        thread_primitive__init(&thread_control.primitive);
        thread_control.primitive.type = THREAD_PRIMITIVE__TYPE__THREAD_MESH_LOCAL_PREFIX;

        if (args_info.prefix_orig != NULL)
        {
            thread_control.primitive.value_case = THREAD_PRIMITIVE__VALUE_BYTES;
            thread_control.primitive.bytes.len = 8;

            if (inet_pton(AF_INET6, args_info.prefix_orig, thread_control.primitive.bytes.data) != 1)
            {
                printf("invalid prefix\n");
                return -1;
            }
        }
    }

    if (args_info.mode_given)
    {
        thread_control.message_case = THREAD_CONTROL__MESSAGE_PRIMITIVE;
        thread_primitive__init(&thread_control.primitive);
        thread_control.primitive.type = THREAD_PRIMITIVE__TYPE__THREAD_MODE;

        if (args_info.mode_orig != NULL)
        {
            thread_control.primitive.value_case = THREAD_PRIMITIVE__VALUE_UINT32;
            thread_control.primitive.uint32 = 0;

            for (char *flag = args_info.mode_arg; *flag != '\0'; flag++)
            {
                switch (*flag)
                {
                case 'r':
                    thread_control.primitive.uint32 |= Thread::kModeRxOnWhenIdle;
                    break;

                case 's':
                    thread_control.primitive.uint32 |= Thread::kModeSecureDataRequest;
                    break;

                case 'd':
                    thread_control.primitive.uint32 |= Thread::kModeFFD;
                    break;

                case 'n':
                    thread_control.primitive.uint32 |= Thread::kModeFullNetworkData;
                    break;

                default:
                    goto parse_error;
                }
            }
        }
    }

    if (args_info.status_given)
    {
        switch (args_info.status_arg)
        {
        case status_arg_up:
            thread_control.message_case = THREAD_CONTROL__MESSAGE_PRIMITIVE;
            thread_primitive__init(&thread_control.primitive);
            thread_control.primitive.type = THREAD_PRIMITIVE__TYPE__THREAD_STATUS;
            thread_control.primitive.value_case = THREAD_PRIMITIVE__VALUE_BOOL;
            thread_control.primitive.bool_ = true;
            break;

        case status_arg_down:
            thread_control.message_case = THREAD_CONTROL__MESSAGE_PRIMITIVE;
            thread_primitive__init(&thread_control.primitive);
            thread_control.primitive.type = THREAD_PRIMITIVE__TYPE__THREAD_STATUS;
            thread_control.primitive.value_case = THREAD_PRIMITIVE__VALUE_BOOL;
            thread_control.primitive.bool_ = false;
            break;

        case status_arg_unspec:
            thread_control.message_case = THREAD_CONTROL__MESSAGE_PRIMITIVE;
            thread_primitive__init(&thread_control.primitive);
            thread_control.primitive.type = THREAD_PRIMITIVE__TYPE__THREAD_STATUS;
            break;

        default:
            break;
        }
    }

    if (args_info.state_given)
    {
        switch (args_info.state_arg)
        {
        case state__NULL:
            break;

        case state_arg_detached:
            thread_control.message_case = THREAD_CONTROL__MESSAGE_STATE;
            thread_state__init(&thread_control.state);
            thread_control.state.has_state = true;
            thread_control.state.state = THREAD_STATE__STATE__DETACHED;
            break;

        case state_arg_child:
            thread_control.message_case = THREAD_CONTROL__MESSAGE_STATE;
            thread_state__init(&thread_control.state);
            thread_control.state.has_state = true;
            thread_control.state.state = THREAD_STATE__STATE__CHILD;
            break;

        case state_arg_router:
            thread_control.message_case = THREAD_CONTROL__MESSAGE_STATE;
            thread_state__init(&thread_control.state);
            thread_control.state.has_state = true;
            thread_control.state.state = THREAD_STATE__STATE__ROUTER;
            break;

        case state_arg_leader:
            thread_control.message_case = THREAD_CONTROL__MESSAGE_STATE;
            thread_state__init(&thread_control.state);
            thread_control.state.has_state = true;
            thread_control.state.state = THREAD_STATE__STATE__LEADER;
            break;

        case state_arg_unspec:
            thread_control.message_case = THREAD_CONTROL__MESSAGE_STATE;
            thread_state__init(&thread_control.state);
            break;
        }
    }

    if (args_info.timeout_given)
    {
        thread_control.message_case = THREAD_CONTROL__MESSAGE_PRIMITIVE;
        thread_primitive__init(&thread_control.primitive);
        thread_control.primitive.type = THREAD_PRIMITIVE__TYPE__THREAD_TIMEOUT;

        if (args_info.timeout_orig != NULL)
        {
            thread_control.primitive.value_case = THREAD_PRIMITIVE__VALUE_UINT32;
            thread_control.primitive.uint32 = args_info.timeout_arg;
        }
    }

    if (args_info.channel_given)
    {
        thread_control.message_case = THREAD_CONTROL__MESSAGE_PRIMITIVE;
        thread_primitive__init(&thread_control.primitive);
        thread_control.primitive.type = THREAD_PRIMITIVE__TYPE__IEEE802154_CHANNEL;

        if (args_info.channel_orig != NULL)
        {
            thread_control.primitive.value_case = THREAD_PRIMITIVE__VALUE_UINT32;
            thread_control.primitive.uint32 = args_info.channel_arg;
        }
    }

    if (args_info.panid_given)
    {
        thread_control.message_case = THREAD_CONTROL__MESSAGE_PRIMITIVE;
        thread_primitive__init(&thread_control.primitive);
        thread_control.primitive.type = THREAD_PRIMITIVE__TYPE__IEEE802154_PANID;

        if (args_info.panid_orig != NULL)
        {
            thread_control.primitive.value_case = THREAD_PRIMITIVE__VALUE_UINT32;
            thread_control.primitive.uint32 = args_info.panid_arg;
        }
    }

    if (args_info.xpanid_given)
    {
        thread_control.message_case = THREAD_CONTROL__MESSAGE_PRIMITIVE;
        thread_primitive__init(&thread_control.primitive);
        thread_control.primitive.type = THREAD_PRIMITIVE__TYPE__IEEE802154_EXTENDED_PANID;

        if (args_info.xpanid_orig != NULL)
        {
            thread_control.primitive.value_case = THREAD_PRIMITIVE__VALUE_BYTES;
            thread_control.primitive.bytes.len =
                Thread::hex2bin(args_info.xpanid_arg, thread_control.primitive.bytes.data,
                                sizeof(thread_control.primitive.bytes.data));
        }
    }

    if (args_info.netname_given)
    {
        thread_control.message_case = THREAD_CONTROL__MESSAGE_PRIMITIVE;
        thread_primitive__init(&thread_control.primitive);
        thread_control.primitive.type = THREAD_PRIMITIVE__TYPE__IEEE802154_NETWORK_NAME;

        if (args_info.netname_orig != NULL)
        {
            thread_control.primitive.value_case = THREAD_PRIMITIVE__VALUE_BYTES;
            thread_control.primitive.bytes.len = strlen(args_info.netname_arg) + 1;
            memcpy(thread_control.primitive.bytes.data, args_info.netname_arg, strlen(args_info.netname_arg));
        }
    }

    if (args_info.shortaddr_given)
    {
        thread_control.message_case = THREAD_CONTROL__MESSAGE_PRIMITIVE;
        thread_primitive__init(&thread_control.primitive);
        thread_control.primitive.type = THREAD_PRIMITIVE__TYPE__IEEE802154_SHORT_ADDR;
    }

    if (args_info.extaddr_given)
    {
        thread_control.message_case = THREAD_CONTROL__MESSAGE_PRIMITIVE;
        thread_primitive__init(&thread_control.primitive);
        thread_control.primitive.type = THREAD_PRIMITIVE__TYPE__IEEE802154_EXT_ADDR;
    }

    if (args_info.whitelist_status_given)
    {
        thread_control.message_case = THREAD_CONTROL__MESSAGE_WHITELIST;
        thread_whitelist__init(&thread_control.whitelist);
        thread_control.whitelist.type = THREAD_WHITELIST__TYPE__STATUS;

        switch (args_info.whitelist_status_arg)
        {
        case whitelist_status_arg_disable:
            thread_control.whitelist.has_status = true;
            thread_control.whitelist.status = THREAD_WHITELIST__STATUS__DISABLE;
            break;

        case whitelist_status_arg_enable:
            thread_control.whitelist.has_status = true;
            thread_control.whitelist.status = THREAD_WHITELIST__STATUS__ENABLE;
            break;

        default:
            break;
        }
    }

    if (args_info.whitelist_add_given)
    {
        thread_control.message_case = THREAD_CONTROL__MESSAGE_WHITELIST;
        thread_whitelist__init(&thread_control.whitelist);
        thread_control.whitelist.type = THREAD_WHITELIST__TYPE__ADD;
        thread_control.whitelist.n_address = 1;
        thread_control.whitelist.address[0].len = 8;
        Thread::hex2bin(args_info.whitelist_add_arg, thread_control.whitelist.address[0].data,
                        sizeof(thread_control.whitelist.address[0].data));
    }

    if (args_info.whitelist_delete_given)
    {
        thread_control.message_case = THREAD_CONTROL__MESSAGE_WHITELIST;
        thread_whitelist__init(&thread_control.whitelist);
        thread_control.whitelist.type = THREAD_WHITELIST__TYPE__DELETE;
        thread_control.whitelist.n_address = 1;
        thread_control.whitelist.address[0].len = 8;
        Thread::hex2bin(args_info.whitelist_delete_arg, thread_control.whitelist.address[0].data,
                        sizeof(thread_control.whitelist.address[0].data));
    }

    if (args_info.whitelist_show_given)
    {
        thread_control.message_case = THREAD_CONTROL__MESSAGE_WHITELIST;
        thread_whitelist__init(&thread_control.whitelist);
        thread_control.whitelist.type = THREAD_WHITELIST__TYPE__LIST;
        thread_control.whitelist.n_address = 0;
    }

    // pack protobuf message
    uint8_t buf[1024];
    int buf_length;
    buf_length = thread_control__pack(&thread_control, buf);
    dump("protobuf", buf, buf_length);

    // open ipc socket
    int ipc_fd;

    if ((ipc_fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
    {
        perror("socket");
        return -1;
    }

    struct sockaddr_un sockaddr_un;

    memset(&sockaddr_un, 0, sizeof(sockaddr_un));

    sockaddr_un.sun_family = AF_UNIX;

    snprintf(sockaddr_un.sun_path, sizeof(sockaddr_un.sun_path), "/tmp/thread-driver-%s", args_info.interface_arg);

    if (connect(ipc_fd, reinterpret_cast<struct sockaddr *>(&sockaddr_un),
                sizeof(sockaddr_un.sun_family) + strlen(sockaddr_un.sun_path) + 1) < 0)
    {
        perror("connect");
        return -1;
    }

    // send request
    write(ipc_fd, buf, buf_length);

    // receive response
    buf_length = read(ipc_fd, buf, sizeof(buf));

    if (buf_length < 0)
    {
        printf("%s\n", strerror(errno));
    }

    dump("response", buf, buf_length);
    Thread::ProcessThreadControl(buf, buf_length);

    close(ipc_fd);

    cmdline_parser_free(&args_info);
    return 0;

parse_error:
    cmdline_parser_free(&args_info);
    cmdline_parser_print_help();
    return 0;
}
