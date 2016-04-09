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
ED1 = 2
ED2 = 3
ED3 = 4
ED4 = 5

class Cert_5_3_8_ChildAddressSet(unittest.TestCase):
    def setUp(self):
        self.nodes = {}
        for i in range(1,6):
            self.nodes[i] = node.Node(i)

        self.nodes[LEADER].set_panid(0xface)
        self.nodes[LEADER].set_mode('rsdn')
        self.nodes[LEADER].add_whitelist(self.nodes[ED1].get_addr64())
        self.nodes[LEADER].add_whitelist(self.nodes[ED2].get_addr64())
        self.nodes[LEADER].add_whitelist(self.nodes[ED3].get_addr64())
        self.nodes[LEADER].add_whitelist(self.nodes[ED4].get_addr64())
        self.nodes[LEADER].enable_whitelist()

        self.nodes[ED1].set_panid(0xface)
        self.nodes[ED1].set_mode('rsn')
        self.nodes[ED1].add_whitelist(self.nodes[LEADER].get_addr64())
        self.nodes[ED1].enable_whitelist()

        self.nodes[ED2].set_panid(0xface)
        self.nodes[ED2].set_mode('rsn')
        self.nodes[ED2].add_whitelist(self.nodes[LEADER].get_addr64())
        self.nodes[ED2].enable_whitelist()

        self.nodes[ED3].set_panid(0xface)
        self.nodes[ED3].set_mode('rsn')
        self.nodes[ED3].add_whitelist(self.nodes[LEADER].get_addr64())
        self.nodes[ED3].enable_whitelist()

        self.nodes[ED4].set_panid(0xface)
        self.nodes[ED4].set_mode('rsn')
        self.nodes[ED4].add_whitelist(self.nodes[LEADER].get_addr64())
        self.nodes[ED4].enable_whitelist()

    def tearDown(self):
        for node in self.nodes.itervalues():
            node.stop()
        del self.nodes

    def test(self):
        self.nodes[LEADER].start()
        self.nodes[LEADER].set_state('leader')
        self.assertEqual(self.nodes[LEADER].get_state(), 'leader')

        self.nodes[ED1].start()
        time.sleep(3)
        self.assertEqual(self.nodes[ED1].get_state(), 'child')

        self.nodes[ED2].start()
        time.sleep(3)
        self.assertEqual(self.nodes[ED2].get_state(), 'child')

        self.nodes[ED3].start()
        time.sleep(3)
        self.assertEqual(self.nodes[ED3].get_state(), 'child')

        self.nodes[ED4].start()
        time.sleep(3)
        self.assertEqual(self.nodes[ED4].get_state(), 'child')

        for i in range(2,6):
            addrs = self.nodes[i].get_addrs()
            for addr in addrs:
                if addr[0:4] != 'fe80':
                    self.nodes[LEADER].ping(addr)

if __name__ == '__main__':
    unittest.main()
