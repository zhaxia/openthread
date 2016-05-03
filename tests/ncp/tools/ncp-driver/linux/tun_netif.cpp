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
#include <fcntl.h>
#include <ifaddrs.h>
#include <linux/if_tun.h>
#include <netdb.h>
#include <net/if.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <tun_netif.hpp>

namespace Thread {

struct in6_ifreq
{
    struct in6_addr ifr6_addr;
    __u32 ifr6_prefixlen;
    unsigned int ifr6_ifindex;
};

ThreadError TunNetif::Open()
{
    ThreadError error = kThreadError_None;

    VerifyOrExit((tunfd_ = open("/dev/net/tun", O_RDWR)) >= 0, perror("tun open"); error = kThreadError_Error);

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
    VerifyOrExit(ioctl(tunfd_, TUNSETIFF, &ifr) == 0, perror("TUNSETIFF"); error = kThreadError_Error);

    Down();

exit:

    if (error != kThreadError_None)
    {
        close(tunfd_);
        tunfd_ = -1;
    }

    return error;
}

ThreadError TunNetif::GetName(char *name, int name_length)
{
    ThreadError error = kThreadError_None;

    struct ifreq ifr;
    VerifyOrExit(ioctl(tunfd_, TUNGETIFF, &ifr) == 0, perror("TUNGETIFF"); error = kThreadError_Error);
    strncpy(name, ifr.ifr_name, name_length);

exit:
    return error;
}

int TunNetif::GetIndex()
{
    int rval = -1;
    int reqfd = -1;

    struct ifreq ifr;
    VerifyOrExit(ioctl(tunfd_, TUNGETIFF, &ifr) == 0, perror("TUNGETIFF"));
    VerifyOrExit((reqfd = socket(AF_INET6, SOCK_DGRAM, 0)) >= 0, perror("socket"));
    VerifyOrExit(ioctl(reqfd, SIOGIFINDEX, &ifr) == 0, perror("SIOGIFINDEX"));

    rval = ifr.ifr_ifindex;

exit:

    if (reqfd >= 0)
    {
        close(reqfd);
    }

    return rval;
}

ThreadError TunNetif::Down()
{
    ThreadError error = kThreadError_None;
    struct ifreq ifreq = {};
    int reqfd = -1;

    SuccessOrExit(error = GetName(ifreq.ifr_name, sizeof(ifreq.ifr_name)));

    VerifyOrExit((reqfd = socket(AF_INET6, SOCK_DGRAM, 0)) >= 0, error = kThreadError_Error);
    VerifyOrExit(ioctl(reqfd, SIOCGIFFLAGS, &ifreq) == 0, error = kThreadError_Error);

    ifreq.ifr_flags &= ~(IFF_UP);
    VerifyOrExit(ioctl(reqfd, SIOCSIFFLAGS, &ifreq) == 0, error = kThreadError_Error);

    ClearRoutes();

exit:

    if (reqfd >= 0)
    {
        close(reqfd);
    }

    return error;
}

ThreadError TunNetif::Up()
{
    ThreadError error = kThreadError_None;
    struct ifreq ifreq = {};
    int reqfd = -1;

    SuccessOrExit(error = GetName(ifreq.ifr_name, sizeof(ifreq.ifr_name)));

    VerifyOrExit((reqfd = socket(AF_INET6, SOCK_DGRAM, 0)) >= 0, error = kThreadError_Error);
    VerifyOrExit(ioctl(reqfd, SIOCGIFFLAGS, &ifreq) == 0, error = kThreadError_Error);

    ifreq.ifr_flags |= IFF_UP;
    VerifyOrExit(ioctl(reqfd, SIOCSIFFLAGS, &ifreq) == 0, error = kThreadError_Error);

exit:

    if (reqfd >= 0)
    {
        close(reqfd);
    }

    return error;
}

int TunNetif::GetFileDescriptor()
{
    return tunfd_;
}

size_t TunNetif::Read(uint8_t *buf, size_t buf_length)
{
    ssize_t rval = read(tunfd_, buf, buf_length);

    if (rval == -1)
    {
        perror("tun read");
    }

    return rval;
}

size_t TunNetif::Write(uint8_t *buf, size_t buf_length)
{
    return write(tunfd_, buf, buf_length);
}

void SetPrefixMask(struct in6_addr *prefix, uint8_t prefix_length)
{
    for (unsigned i = 0; i < prefix_length / 8; i++)
    {
        prefix->s6_addr[i] = 0xff;
    }

    prefix->s6_addr[prefix_length / 8] = 0xff << (8 - (prefix_length % 8));

    for (unsigned i = (prefix_length / 8) + 1; i < sizeof(*prefix); i++)
    {
        prefix->s6_addr[i] = 0x00;
    }
}

ThreadError TunNetif::AddIp6Address(const in6_addr *address, uint8_t prefix_length)
{
    ThreadError error = kThreadError_None;

    struct in6_ifreq in6_ifreq;
    memset(&in6_ifreq, 0, sizeof(in6_ifreq));
    memcpy(&in6_ifreq.ifr6_addr, address, sizeof(in6_ifreq.ifr6_addr));
    in6_ifreq.ifr6_prefixlen = 64;
    in6_ifreq.ifr6_ifindex = GetIndex();

    int reqfd = -1;
    VerifyOrExit((reqfd = socket(AF_INET6, SOCK_DGRAM, 0)) >= 0, perror("socket"); error = kThreadError_Error);
    VerifyOrExit(ioctl(reqfd, SIOCSIFADDR, &in6_ifreq) == 0, perror("SIOCSIFADDR"); error = kThreadError_Error);

exit:

    if (reqfd >= 0)
    {
        close(reqfd);
    }

    return error;
}

ThreadError TunNetif::RemoveIp6Address(const in6_addr *address)
{
    ThreadError error = kThreadError_None;

    struct in6_ifreq in6_ifreq;
    memset(&in6_ifreq, 0, sizeof(in6_ifreq));
    memcpy(&in6_ifreq.ifr6_addr, address, sizeof(in6_ifreq.ifr6_addr));
    in6_ifreq.ifr6_prefixlen = 64;
    in6_ifreq.ifr6_ifindex = GetIndex();

    int reqfd = -1;
    VerifyOrExit((reqfd = socket(AF_INET6, SOCK_DGRAM, 0)) >= 0, perror("socket"); error = kThreadError_Error);
    VerifyOrExit(ioctl(reqfd, SIOCDIFADDR, &in6_ifreq) == 0, perror("SIOCDIFADDR"); error = kThreadError_Error);

exit:

    if (reqfd >= 0)
    {
        close(reqfd);
    }

    return error;
}

ThreadError TunNetif::SetIp6Addresses(ThreadIp6Addresses *addresses)
{
    ThreadError error = kThreadError_None;
    struct ifaddrs *ifaddrs;
    VerifyOrExit(getifaddrs(&ifaddrs) == 0, perror("getifaddrs"); error = kThreadError_Error);

    char ifname[80];
    GetName(ifname, sizeof(ifname));

    for (struct ifaddrs *ifa = ifaddrs; ifa != NULL; ifa = ifa->ifa_next)
    {
        if (ifa->ifa_addr == NULL ||
            ifa->ifa_addr->sa_family != AF_INET6 ||
            strcmp(ifa->ifa_name, ifname) != 0)
        {
            continue;
        }

        struct sockaddr_in6 *sin6_addr = (struct sockaddr_in6 *)ifa->ifa_addr;

        for (unsigned i = 0; i < addresses->n_address; i++)
        {
            if (memcmp(&sin6_addr->sin6_addr, addresses->address[i].data, sizeof(sin6_addr->sin6_addr)) == 0)
            {
                addresses->address[i].len = 0;
                goto next;
            }
        }

        RemoveIp6Address(&sin6_addr->sin6_addr);

next:
        {}
    }

    freeifaddrs(ifaddrs);

    for (unsigned i = 0; i < addresses->n_address; i++)
    {
        if (addresses->address[addresses->n_address - 1 - i].len == 0)
        {
            continue;
        }

        AddIp6Address((struct in6_addr *)addresses->address[addresses->n_address - 1 - i].data, 64);
    }

exit:
    return error;
}

ThreadError TunNetif::AddRoute(const struct in6_addr *prefix, uint8_t prefix_length)
{
    return kThreadError_Error;
}

ThreadError TunNetif::ClearRoutes()
{
    return kThreadError_Error;
}

}  // namespace Thread
