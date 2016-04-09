#!/usr/bin/python

#
#    Copyright (c) 2015 Nest Labs, Inc.
#    All rights reserved.
#
#    This document is the property of Nest. It is considered
#    confidential and proprietary information.
#
#    This document may not be reproduced or transmitted in any form,
#    in whole or in part, without the express written permission of
#    Nest.
#
#    @author  Jonathan Hui <jonhui@nestlabs.com>
#

import pexpect
import time
import unittest

import node

LEADER = 1
ROUTER = 2
ED1 = 3
SED1 = 4

class Cert_5_6_5_NetworkDataRegisterAfterAttachRouter(unittest.TestCase):
    def setUp(self):
        self.nodes = {}
        for i in range(1,5):
            self.nodes[i] = node.Node(i)

        self.nodes[LEADER].set_panid(0xface)
        self.nodes[LEADER].set_mode('rsdn')
        self.nodes[LEADER].add_whitelist(self.nodes[ROUTER].get_addr64())
        self.nodes[LEADER].enable_whitelist()

        self.nodes[ROUTER].set_panid(0xface)
        self.nodes[ROUTER].set_mode('rsdn')
        self.nodes[ROUTER].add_whitelist(self.nodes[LEADER].get_addr64())
        self.nodes[ROUTER].add_whitelist(self.nodes[ED1].get_addr64())
        self.nodes[ROUTER].add_whitelist(self.nodes[SED1].get_addr64())
        self.nodes[ROUTER].enable_whitelist()

        self.nodes[ED1].set_panid(0xface)
        self.nodes[ED1].set_mode('rsn')
        self.nodes[ED1].add_whitelist(self.nodes[ROUTER].get_addr64())
        self.nodes[ED1].enable_whitelist()

        self.nodes[SED1].set_panid(0xface)
        self.nodes[SED1].set_mode('s')
        self.nodes[SED1].add_whitelist(self.nodes[ROUTER].get_addr64())
        self.nodes[SED1].enable_whitelist()
        self.nodes[SED1].set_timeout(3)

    def tearDown(self):
        for node in self.nodes.itervalues():
            node.stop()
        del self.nodes

    def test(self):
        self.nodes[LEADER].start()
        self.nodes[LEADER].set_state('leader')
        self.assertEqual(self.nodes[LEADER].get_state(), 'leader')

        self.nodes[ROUTER].start()
        time.sleep(3)
        self.assertEqual(self.nodes[ROUTER].get_state(), 'router')

        self.nodes[ED1].start()
        time.sleep(3)
        self.assertEqual(self.nodes[ED1].get_state(), 'child')

        self.nodes[SED1].start()
        time.sleep(3)
        self.assertEqual(self.nodes[SED1].get_state(), 'child')

        self.nodes[ROUTER].add_prefix('2001::/64', 'pvcrs')
        self.nodes[ROUTER].add_prefix('2002::/64', 'pvcr')
        self.nodes[ROUTER].register_netdata()

        time.sleep(10)

        addrs = self.nodes[ED1].get_addrs()
        for addr in addrs:
            if addr[0:3] == '200':
                self.nodes[LEADER].ping(addr)

        addrs = self.nodes[SED1].get_addrs()
        for addr in addrs:
            if addr[0:3] == '200':
                self.nodes[LEADER].ping(addr)

        self.nodes[ROUTER].add_prefix('2003::/64', 'pvcs')
        self.nodes[ROUTER].register_netdata()

        time.sleep(10)

        addrs = self.nodes[ED1].get_addrs()
        for addr in addrs:
            if addr[0:3] == '200':
                self.nodes[LEADER].ping(addr)

        addrs = self.nodes[SED1].get_addrs()
        for addr in addrs:
            if addr[0:3] == '200':
                self.nodes[LEADER].ping(addr)

if __name__ == '__main__':
    unittest.main()
