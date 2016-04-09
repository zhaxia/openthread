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
ED = 2

class Cert_5_8_1_KeySynchronization(unittest.TestCase):
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
        self.assertEqual(self.nodes[LEADER].get_state(), 'leader')

        self.nodes[ED].start()
        time.sleep(3)
        self.assertEqual(self.nodes[ED].get_state(), 'child')

        addrs = self.nodes[LEADER].get_addrs()
        for addr in addrs:
            if 'ff:fe00' not in addr:
                self.nodes[ED].ping(addr)

        key_sequence = self.nodes[ED].get_key_sequence()
        self.nodes[ED].set_key_sequence(key_sequence + 10)

        addrs = self.nodes[LEADER].get_addrs()
        for addr in addrs:
            if 'ff:fe00' not in addr:
                try:
                    self.nodes[ED].ping(addr)
                except pexpect.TIMEOUT:
                    pass

if __name__ == '__main__':
    unittest.main()
