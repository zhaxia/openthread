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
BR = 2
ROUTER2 = 3
ROUTER3 = 4
SED2 = 5

class Cert_5_3_9_AddressQuery(unittest.TestCase):
    def setUp(self):
        self.nodes = {}
        for i in range(1,6):
            self.nodes[i] = node.Node(i)

        self.nodes[LEADER].set_panid(0xface)
        self.nodes[LEADER].set_mode('rsdn')
        self.nodes[LEADER].add_whitelist(self.nodes[BR].get_addr64())
        self.nodes[LEADER].add_whitelist(self.nodes[ROUTER2].get_addr64())
        self.nodes[LEADER].add_whitelist(self.nodes[ROUTER3].get_addr64())
        self.nodes[LEADER].enable_whitelist()

        self.nodes[BR].set_panid(0xface)
        self.nodes[BR].set_mode('rsdn')
        self.nodes[BR].add_whitelist(self.nodes[LEADER].get_addr64())
        self.nodes[BR].enable_whitelist()

        self.nodes[ROUTER2].set_panid(0xface)
        self.nodes[ROUTER2].set_mode('rsdn')
        self.nodes[ROUTER2].add_whitelist(self.nodes[LEADER].get_addr64())
        self.nodes[ROUTER2].add_whitelist(self.nodes[SED2].get_addr64())
        self.nodes[ROUTER2].enable_whitelist()

        self.nodes[ROUTER3].set_panid(0xface)
        self.nodes[ROUTER3].set_mode('rsdn')
        self.nodes[ROUTER3].add_whitelist(self.nodes[LEADER].get_addr64())
        self.nodes[ROUTER3].add_whitelist(self.nodes[ROUTER2].get_addr64())
        self.nodes[ROUTER3].enable_whitelist()

        self.nodes[SED2].set_panid(0xface)
        self.nodes[SED2].set_mode('sn')
        self.nodes[SED2].add_whitelist(self.nodes[ROUTER2].get_addr64())
        self.nodes[SED2].set_timeout(3)
        self.nodes[SED2].enable_whitelist()

    def tearDown(self):
        for node in self.nodes.itervalues():
            node.stop()
        del self.nodes

    def test(self):
        self.nodes[LEADER].start()
        self.nodes[LEADER].set_state('leader')
        self.assertEqual(self.nodes[LEADER].get_state(), 'leader')

        self.nodes[BR].start()
        time.sleep(3)
        self.assertEqual(self.nodes[BR].get_state(), 'router')

        self.nodes[BR].add_prefix('2003::/64', 'pvcrs')
        self.nodes[BR].add_prefix('2004::/64', 'pvcrs')
        self.nodes[BR].register_netdata()

        self.nodes[ROUTER2].start()
        time.sleep(3)
        self.assertEqual(self.nodes[ROUTER2].get_state(), 'router')

        self.nodes[ROUTER3].start()
        time.sleep(3)
        self.assertEqual(self.nodes[ROUTER3].get_state(), 'router')

        self.nodes[SED2].start()
        time.sleep(3)
        self.assertEqual(self.nodes[SED2].get_state(), 'child')

        addrs = self.nodes[BR].get_addrs()
        for addr in addrs:
            if addr[0:4] != 'fe80':
                self.nodes[SED2].ping(addr)

        addrs = self.nodes[SED2].get_addrs()
        for addr in addrs:
            if addr[0:4] != 'fe80':
                self.nodes[LEADER].ping(addr)

        addrs = self.nodes[BR].get_addrs()
        for addr in addrs:
            if addr[0:4] != 'fe80':
                self.nodes[SED2].ping(addr)

        self.nodes[ROUTER3].stop()
        time.sleep(300)
        
        addrs = self.nodes[ROUTER3].get_addrs()
        for addr in addrs:
            if addr[0:4] != 'fe80':
                self.nodes[SED2].ping(addr)

        self.nodes[SED2].stop()
        time.sleep(10)

        addrs = self.nodes[SED2].get_addrs()
        for addr in addrs:
            if addr[0:4] != 'fe80':
                self.nodes[BR].ping(addr)

if __name__ == '__main__':
    unittest.main()
