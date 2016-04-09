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
ROUTER = 2
ED = 3

class Cert_5_1_07_MaxChildCount(unittest.TestCase):
    def setUp(self):
        self.nodes = {}
        for i in range(1,9):
            self.nodes[i] = node.Node(i)

        self.nodes[LEADER].set_panid(0xface)
        self.nodes[LEADER].set_mode('rsdn')
        self.nodes[LEADER].add_whitelist(self.nodes[ROUTER].get_addr64())
        self.nodes[LEADER].enable_whitelist()

        self.nodes[ROUTER].set_panid(0xface)
        self.nodes[ROUTER].set_mode('rsdn')
        self.nodes[ROUTER].add_whitelist(self.nodes[LEADER].get_addr64())
        self.nodes[ROUTER].enable_whitelist()

        self.nodes[ED].set_panid(0xface)
        self.nodes[ED].set_mode('rsn')
        self.nodes[ED].add_whitelist(self.nodes[ROUTER].get_addr64())
        self.nodes[ROUTER].add_whitelist(self.nodes[ED].get_addr64())
        self.nodes[ED].enable_whitelist()

        for i in range(4, 9):
            self.nodes[i].set_panid(0xface)
            self.nodes[i].set_mode('rsn')
            self.nodes[i].add_whitelist(self.nodes[ROUTER].get_addr64())
            self.nodes[ROUTER].add_whitelist(self.nodes[i].get_addr64())
            self.nodes[i].enable_whitelist()
            self.nodes[i].set_timeout(3)

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

        for i in range(4, 9):
            self.nodes[i].start()
            time.sleep(3)
            self.assertEqual(self.nodes[i].get_state(), 'child')

        self.nodes[ED].start()
        time.sleep(3)
        self.assertEqual(self.nodes[ED].get_state(), 'detached')

        self.nodes[ED].stop()
        for i in range(4, 9):
            self.nodes[i].stop()
        time.sleep(3)
        
        self.nodes[LEADER].stop()
        time.sleep(105)
        
        self.nodes[ED].start()
        time.sleep(3)
        self.assertEqual(self.nodes[ED].get_state(), 'detached')
        
if __name__ == '__main__':
    unittest.main()
