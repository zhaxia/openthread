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
DUT = 33

class Cert_5_2_2_LeaderReject1Hop(unittest.TestCase):
    def setUp(self):
        self.nodes = {}

        self.nodes[LEADER] = node.Node(LEADER)
        self.nodes[LEADER].set_panid(0xface)
        self.nodes[LEADER].set_mode('rsdn')
        self.nodes[LEADER].enable_whitelist()

        for i in range(2,34):
            self.nodes[i] = node.Node(i)
            self.nodes[i].set_panid(0xface)
            self.nodes[i].set_mode('rsdn')
            self.nodes[i].add_whitelist(self.nodes[LEADER].get_addr64())
            self.nodes[LEADER].add_whitelist(self.nodes[i].get_addr64())
            self.nodes[i].enable_whitelist()
            self.nodes[i].set_router_upgrade_threshold(33)

    def tearDown(self):
        for node in self.nodes.itervalues():
            node.stop()
        del self.nodes

    def test(self):
        self.nodes[LEADER].start()
        self.nodes[LEADER].set_state('leader')
        self.assertEqual(self.nodes[LEADER].get_state(), 'leader')

        for i in range(2, 33):
            self.nodes[i].start()
            time.sleep(5)
            self.assertEqual(self.nodes[i].get_state(), 'router')

        self.nodes[DUT].start()
        time.sleep(5)
        self.assertEqual(self.nodes[DUT].get_state(), 'child')

if __name__ == '__main__':
    unittest.main()
