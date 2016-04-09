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

import time
import unittest

import node

LEADER = 1
ED = 2

class Cert_6_6_1_KeyIncrement(unittest.TestCase):
    def setUp(self):
        self.nodes = {}
        for i in range(1,3):
            self.nodes[i] = node.Node(i)

        self.nodes[LEADER].set_panid(0xface)
        self.nodes[LEADER].set_mode('rsdn')
        self.nodes[LEADER].add_whitelist(self.nodes[ED].get_addr64())
        self.nodes[LEADER].enable_whitelist()

        self.nodes[ED].set_panid(0xface)
        self.nodes[ED].set_mode('rsn')
        self.nodes[ED].add_whitelist(self.nodes[LEADER].get_addr64())
        self.nodes[ED].enable_whitelist()

    def tearDown(self):
        for node in self.nodes.itervalues():
            node.stop()
        del self.nodes

    def test(self):
        self.nodes[LEADER].start()
        self.nodes[LEADER].set_state('leader')

        self.nodes[ED].start()
        time.sleep(3)
        self.assertEqual(self.nodes[ED].get_state(), "child")

        addrs = self.nodes[ED].get_addrs()
        for addr in addrs:
            self.nodes[LEADER].ping(addr)

        key_sequence = self.nodes[LEADER].get_key_sequence()
        self.nodes[LEADER].set_key_sequence(key_sequence + 1)

        addrs = self.nodes[ED].get_addrs()
        for addr in addrs:
            self.nodes[LEADER].ping(addr)

if __name__ == '__main__':
    unittest.main()
