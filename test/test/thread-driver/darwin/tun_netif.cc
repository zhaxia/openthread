/*
 *
 *    Copyright (c) 2015 Nest Labs, Inc.
 *    All rights reserved.
 *
 *    This document is the property of Nest. It is considered
 *    confidential and proprietary information.
 *
 *    This document may not be reproduced or transmitted in any form,
 *    in whole or in part, without the express written permission of
 *    Nest.
 *
 *    @author  Jonathan Hui <jonhui@nestlabs.com>
 *
 */

#include <tun_netif.h>

struct prf_ra
{
    u_char onlink : 1;
    u_char autonomous : 1;
    u_char reserved : 6;
} prf_ra;

#include <sys/types.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <net/bpf.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_utun.h>
#include <net/route.h>
#include <netinet6/in6_var.h>
#include <netinet6/nd6.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/kern_control.h>
#include <sys/sysctl.h>

namespace Thread {

#define ND6_IFF_DISABLED  0x08

void disable_nud()
{
    int fd = socket(AF_INET6, SOCK_DGRAM, 0);

    struct in6_ndireq nd;
    memset(&nd, 0, sizeof(nd));
    strlcpy(nd.ifname, "utun0", sizeof(nd.ifname));
    ioctl(fd, SIOCGIFINFO_IN6, &nd);
    nd.ndi.flags &= ~(ND6_IFF_PERFORMNUD | ND6_IFF_DISABLED);
    ioctl(fd, SIOCSIFINFO_FLAGS, &nd);

    close(fd);
}

ThreadError TunNetif::Open()
{
    ThreadError error = kThreadError_None;

    struct ctl_info ctl_info;
    memset(&ctl_info, 0, sizeof(ctl_info));
    VerifyOrExit(strlcpy(ctl_info.ctl_name, UTUN_CONTROL_NAME, sizeof(ctl_info.ctl_name)) < sizeof(ctl_info.ctl_name),
                 perror("UTUN_CONTORL_NAME"); error = kThreadError_Error);

    VerifyOrExit((tunfd_ = socket(PF_SYSTEM, SOCK_DGRAM, SYSPROTO_CONTROL)) >= 0, perror("SYSPROTO_CONTROL"));
    VerifyOrExit(ioctl(tunfd_, CTLIOCGINFO, &ctl_info) == 0, perror("CTLIOCGINFO"); error = kThreadError_Error);

    struct sockaddr_ctl sockaddr_ctl;
    sockaddr_ctl.sc_id = ctl_info.ctl_id;
    sockaddr_ctl.sc_len = sizeof(sockaddr_ctl);
    sockaddr_ctl.sc_family = AF_SYSTEM;
    sockaddr_ctl.ss_sysaddr = AF_SYS_CONTROL;
    sockaddr_ctl.sc_unit = 0;

    VerifyOrExit(connect(tunfd_, reinterpret_cast<struct sockaddr *>(&sockaddr_ctl), sizeof(sockaddr_ctl)) == 0,
                 perror("CONNECT"); error = kThreadError_Error);

    disable_nud();

    // MacOS likes to add its own link-local address when we add the
    // first IPv6 address.  We don't want the MacOS link-local address,
    // so we first add an arbitrary address and then remove it to bypass
    // this feature.
    ThreadIp6Addresses addresses;
    addresses.n_address = 1;
    addresses.address[0].len = 16;
    inet_pton(AF_INET6, "fe80::1", &addresses.address[0].data);
    SetIp6Addresses(&addresses);

    addresses.n_address = 0;
    SetIp6Addresses(&addresses);

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
    socklen_t socklen = name_length;

    VerifyOrExit(getsockopt(tunfd_, SYSPROTO_CONTROL, UTUN_OPT_IFNAME, name, &socklen) == 0,
                 perror("UTUN_OPT_IFNAME"); error = kThreadError_Error);

exit:
    return error;
}

int TunNetif::GetIndex()
{
    char name[80];
    char *address_string = NULL;
    struct addrinfo *res = NULL;
    int rval = -1;

    SuccessOrExit(GetName(name, sizeof(name)));
    VerifyOrExit(asprintf(&address_string, "fe80::1%%%s", name) >= 0, perror("asprintf"));
    VerifyOrExit(getaddrinfo(address_string, NULL, NULL, &res) == 0, perror("getaddrinfo"));

    struct sockaddr_in6 *sa6;
    sa6 = reinterpret_cast<struct sockaddr_in6 *>(res->ai_addr);
    rval = sa6->sin6_scope_id;

exit:

    if (res != NULL)
    {
        freeaddrinfo(res);
    }

    if (address_string != NULL)
    {
        free(address_string);
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
    size_t rval = read(tunfd_, buf, buf_length);
    rval -= 4;
    memmove(buf, buf + 4, rval);
    return rval;
}

size_t TunNetif::Write(uint8_t *buf, size_t buf_length)
{
    uint8_t tun_buf[1500];
    tun_buf[0] = 0;
    tun_buf[1] = 0;
    tun_buf[2] = AF_INET6 >> 8;
    tun_buf[3] = AF_INET6;
    memcpy(tun_buf + 4, buf, buf_length);
    dump("tun write", buf, buf_length);

    return write(tunfd_, tun_buf, 4 + buf_length);
}

void SetPrefixMask(struct in6_addr *prefix, uint8_t prefix_length)
{
    for (int i = 0; i < prefix_length / 8; i++)
    {
        prefix->s6_addr[i] = 0xff;
    }

    prefix->s6_addr[prefix_length / 8] = 0xff << (8 - (prefix_length % 8));

    for (int i = (prefix_length / 8) + 1; i < sizeof(*prefix); i++)
    {
        prefix->s6_addr[i] = 0x00;
    }
}

ThreadError TunNetif::AddIp6Address(const in6_addr *address, uint8_t prefix_length)
{
    ThreadError error = kThreadError_None;
    struct in6_aliasreq in6_aliasreq = {};
    int reqfd = -1;

    VerifyOrExit(prefix_length <= 128, error = kThreadError_InvalidArgs);

    VerifyOrExit((reqfd = socket(AF_INET6, SOCK_DGRAM, 0)) >= 0, error = kThreadError_Error);
    SuccessOrExit(error = GetName(in6_aliasreq.ifra_name, sizeof(in6_aliasreq.ifra_name)));

    in6_aliasreq.ifra_addr.sin6_family = AF_INET6;
    in6_aliasreq.ifra_addr.sin6_len = sizeof(in6_aliasreq.ifra_addr);
    memcpy(&in6_aliasreq.ifra_addr.sin6_addr, address, sizeof(in6_aliasreq.ifra_addr.sin6_addr));
    memcpy(&in6_aliasreq.ifra_dstaddr.sin6_addr, address, sizeof(in6_aliasreq.ifra_dstaddr.sin6_addr));

    in6_aliasreq.ifra_prefixmask.sin6_family = AF_INET6;
    in6_aliasreq.ifra_prefixmask.sin6_len = sizeof(in6_aliasreq.ifra_prefixmask);
    SetPrefixMask(&in6_aliasreq.ifra_prefixmask.sin6_addr, prefix_length);

    in6_aliasreq.ifra_lifetime.ia6t_vltime = 0xffffffff;
    in6_aliasreq.ifra_lifetime.ia6t_pltime = 0xffffffff;
    in6_aliasreq.ifra_lifetime.ia6t_expire = 0xffffffff;
    in6_aliasreq.ifra_lifetime.ia6t_preferred = 0xffffffff;

    VerifyOrExit(ioctl(reqfd, SIOCAIFADDR_IN6, &in6_aliasreq) == 0,
                 perror("SIOCAIFADDR_IN6"); error = kThreadError_Error);

    if (IN6_IS_ADDR_LINKLOCAL(address))
    {
        AddRoute(address, 64);
    }

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
    struct in6_ifreq in6_ifreq6 = {};
    int reqfd = -1;

    VerifyOrExit((reqfd = socket(AF_INET6, SOCK_DGRAM, 0)) >= 0, error = kThreadError_Error);
    SuccessOrExit(error = GetName(in6_ifreq6.ifr_name, sizeof(in6_ifreq6.ifr_name)));
    memset(&in6_ifreq6.ifr_addr, 0, sizeof(struct sockaddr));
    in6_ifreq6.ifr_addr.sin6_family = AF_INET6;
    in6_ifreq6.ifr_addr.sin6_len = sizeof(in6_ifreq6.ifr_addr);
    in6_ifreq6.ifr_addr.sin6_port = 0;
    memcpy(&in6_ifreq6.ifr_addr.sin6_addr, address, 16);

    VerifyOrExit(ioctl(reqfd, SIOCDIFADDR_IN6, &in6_ifreq6) == 0, perror("SIOCDIFADDR_IN6"); error = kThreadError_Error);

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

        for (int i = 0; i < addresses->n_address; i++)
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

    for (int i = 0; i < addresses->n_address; i++)
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
    ThreadError error = kThreadError_None;

    int ifindex;
    ifindex = GetIndex();

    uint8_t buf[512];
    struct rt_msghdr *rtm;
    rtm = reinterpret_cast<struct rt_msghdr *>(buf);
    rtm->rtm_type = RTM_ADD;
    rtm->rtm_version = RTM_VERSION;
    rtm->rtm_seq = 0;
    rtm->rtm_pid = getpid();
    rtm->rtm_flags = RTF_UP | RTF_GATEWAY;
    rtm->rtm_addrs = RTA_DST | RTA_GATEWAY | RTA_NETMASK;

    struct sockaddr *sa;
    sa = reinterpret_cast<struct sockaddr *>(rtm + 1);

    // destination
    struct sockaddr_in6 *sin6;
    sin6 = reinterpret_cast<struct sockaddr_in6 *>(sa);
    memset(sin6, 0, sizeof(*sin6));
    sin6->sin6_len = sizeof(*sin6);
    sin6->sin6_family = AF_INET6;
    memcpy(&sin6->sin6_addr, prefix, sizeof(sin6->sin6_addr));

    if (IN6_IS_ADDR_LINKLOCAL(&sin6->sin6_addr) ||
        IN6_IS_ADDR_MULTICAST(&sin6->sin6_addr))
    {
        sin6->sin6_addr.s6_addr[2] = ifindex >> 8;
        sin6->sin6_addr.s6_addr[3] = ifindex;
    }

    sa = reinterpret_cast<struct sockaddr *>(reinterpret_cast<uint8_t *>(sa) + sa->sa_len);

    // gateway
    struct sockaddr_dl *sdl;
    sdl = reinterpret_cast<struct sockaddr_dl *>(sa);
    memset(sdl, 0, sizeof(*sdl));
    sdl->sdl_len = sizeof(*sdl);
    sdl->sdl_family = AF_LINK;
    sdl->sdl_index = ifindex;
    sdl->sdl_type = DLT_EN10MB;
    sa = reinterpret_cast<struct sockaddr *>(reinterpret_cast<uint8_t *>(sa) + sa->sa_len);

    // mask
    sin6 = reinterpret_cast<struct sockaddr_in6 *>(sa);
    memset(sin6, 0, sizeof(*sin6));
    sin6->sin6_len = offsetof(struct sockaddr_in6, sin6_addr) + (64 + 7) / 8;
    sin6->sin6_family = AF_INET6;
    SetPrefixMask(&sin6->sin6_addr, 64);
    sa = reinterpret_cast<struct sockaddr *>(reinterpret_cast<uint8_t *>(sa) + sa->sa_len);

    rtm->rtm_msglen = reinterpret_cast<uint8_t *>(sa) - buf;

    int s;
    s = socket(PF_ROUTE, SOCK_RAW, 0);
    write(s, buf, rtm->rtm_msglen);
    close(s);

    return error;
}

ThreadError TunNetif::ClearRoutes()
{
    ThreadError error = kThreadError_Error;
    void *old = NULL;
    size_t oldlen;
    int retry = 0;

    int ifindex;
    VerifyOrExit((ifindex = GetIndex()) >= 0, error = kThreadError_Error);

    do
    {
        int name[] = {CTL_NET, PF_ROUTE, 0, AF_INET6, NET_RT_DUMP, 0};

        if (sysctl(name, sizeof(name) / sizeof(name[0]), NULL, &oldlen, NULL, 0) != 0)
        {
            continue;
        }

        if ((old = malloc(oldlen)) == NULL)
        {
            continue;
        }

        if (sysctl(name, sizeof(name) / sizeof(name[0]), old, &oldlen, NULL, 0) != 0)
        {
            free(old);
            continue;
        }

        error = kThreadError_None;
    }
    while (error != kThreadError_None && retry < 5);

    SuccessOrExit(error);

    struct rt_msghdr *rtm;
    uint8_t *end;

    end = reinterpret_cast<uint8_t *>(old) + oldlen;

    for (uint8_t *cur = reinterpret_cast<uint8_t *>(old); cur < end; cur += rtm->rtm_msglen)
    {
        bool remove_route = false;

        rtm = reinterpret_cast<struct rt_msghdr *>(cur);

        struct sockaddr *sa;
        sa = reinterpret_cast<struct sockaddr *>(rtm + 1);

        if (rtm->rtm_addrs & RTA_DST)
        {
            struct sockaddr_in6 *sa6;
            sa6 = reinterpret_cast<struct sockaddr_in6 *>(sa);

            if ((IN6_IS_ADDR_LINKLOCAL(&sa6->sin6_addr) || IN6_IS_ADDR_MULTICAST(&sa6->sin6_addr)) &&
                (ifindex == (static_cast<uint16_t>(sa6->sin6_addr.s6_addr[2] << 8) | sa6->sin6_addr.s6_addr[3])))
            {
                remove_route = true;
            }

            if (ifindex == sa6->sin6_scope_id)
            {
                remove_route = true;
            }

            sa = reinterpret_cast<struct sockaddr *>(reinterpret_cast<uint8_t *>(sa) + sa->sa_len);
        }

        if (rtm->rtm_addrs & RTA_GATEWAY)
        {
            switch (sa->sa_family)
            {
            case AF_INET6:
            {
                struct sockaddr_in6 *sa6;
                sa6 = reinterpret_cast<struct sockaddr_in6 *>(sa);

                if ((IN6_IS_ADDR_LINKLOCAL(&sa6->sin6_addr) || IN6_IS_ADDR_MULTICAST(&sa6->sin6_addr)) &&
                    (ifindex == (static_cast<uint16_t>(sa6->sin6_addr.s6_addr[2] << 8) | sa6->sin6_addr.s6_addr[3])))
                {
                    remove_route = true;
                }

                if (ifindex == sa6->sin6_scope_id)
                {
                    remove_route = true;
                }

                break;
            }

            case AF_LINK:
            {
                struct sockaddr_dl *sdl;
                sdl = reinterpret_cast<struct sockaddr_dl *>(sa);

                if (sdl->sdl_index == ifindex)
                {
                    remove_route = true;
                }

                break;
            }
            }
        }

        if (!remove_route)
        {
            continue;
        }

        rtm->rtm_type = RTM_DELETE;

        int s = socket(PF_ROUTE, SOCK_RAW, 0);
        write(s, rtm, rtm->rtm_msglen);
        close(s);
    }

exit:

    if (old != NULL)
    {
        free(old);
    }

    return error;
}

}  // namespace Thread
