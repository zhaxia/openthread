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
ROUTER1 = 2
REED0 = 3
REED1 = 4
ED = 5

class Cert_6_1_4_REEDAttachConnectivity(unittest.TestCase):
    def setUp(self):
        self.nodes = {}
        for i in range(1,6):
            self.nodes[i] = node.Node(i)

        self.nodes[LEADER].set_panid(0xface)
        self.nodes[LEADER].set_mode('rsdn')
        self.nodes[LEADER].add_whitelist(self.nodes[ROUTER1].get_addr64())
        self.nodes[LEADER].add_whitelist(self.nodes[REED0].get_addr64())
        self.nodes[LEADER].add_whitelist(self.nodes[REED1].get_addr64())
        self.nodes[LEADER].enable_whitelist()

        self.nodes[ROUTER1].set_panid(0xface)
        self.nodes[ROUTER1].set_mode('rsdn')
        self.nodes[ROUTER1].add_whitelist(self.nodes[LEADER].get_addr64())
        self.nodes[ROUTER1].add_whitelist(self.nodes[REED1].get_addr64())
        self.nodes[ROUTER1].enable_whitelist()

        self.nodes[REED0].set_panid(0xface)
        self.nodes[REED0].set_mode('rsdn')
        self.nodes[REED0].add_whitelist(self.nodes[LEADER].get_addr64())
        self.nodes[REED0].add_whitelist(self.nodes[ED].get_addr64())
        self.nodes[REED0].set_router_upgrade_threshold(0)
        self.nodes[REED0].enable_whitelist()

        self.nodes[REED1].set_panid(0xface)
        self.nodes[REED1].set_mode('rsdn')
        self.nodes[REED1].add_whitelist(self.nodes[LEADER].get_addr64())
        self.nodes[REED1].add_whitelist(self.nodes[ROUTER1].get_addr64())
        self.nodes[REED1].add_whitelist(self.nodes[ED].get_addr64())
        self.nodes[REED1].set_router_upgrade_threshold(0)
        self.nodes[REED1].enable_whitelist()

        self.nodes[ED].set_panid(0xface)
        self.nodes[ED].set_mode('rsn')
        self.nodes[ED].add_whitelist(self.nodes[REED0].get_addr64())
        self.nodes[ED].add_whitelist(self.nodes[REED1].get_addr64())
        self.nodes[ED].enable_whitelist()

    def tearDown(self):
        for node in self.nodes.itervalues():
            node.stop()
        del self.nodes

    def test(self):
        self.nodes[LEADER].start()
        self.nodes[LEADER].set_state('leader')
        self.assertEqual(self.nodes[LEADER].get_state(), 'leader')

        self.nodes[ROUTER1].start()
        time.sleep(3)
        self.assertEqual(self.nodes[ROUTER1].get_state(), 'router')

        self.nodes[REED0].start()
        time.sleep(3)
        self.assertEqual(self.nodes[REED0].get_state(), 'child')

        self.nodes[REED1].start()
        time.sleep(3)
        self.assertEqual(self.nodes[REED1].get_state(), 'child')

        time.sleep(10)

        self.nodes[ED].start()
        time.sleep(10)
        self.assertEqual(self.nodes[ED].get_state(), 'child')
        self.assertEqual(self.nodes[REED1].get_state(), 'router')

if __name__ == '__main__':
    unittest.main()
