#!/usr/bin/python
#
#    Copyright 2016 Nest Labs Inc. All Rights Reserved.
#
#    Licensed under the Apache License, Version 2.0 (the "License");
#    you may not use this file except in compliance with the License.
#    You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
#    Unless required by applicable law or agreed to in writing, software
#    distributed under the License is distributed on an "AS IS" BASIS,
#    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#    See the License for the specific language governing permissions and
#    limitations under the License.
#

import pexpect
import time
import unittest

import node

LEADER1 = 1
ROUTER1 = 2
ROUTER2 = 3

class Cert_5_5_6_SplitWeight(unittest.TestCase):
    def setUp(self):
        self.nodes = {}
        for i in range(1,4):
            self.nodes[i] = node.Node(i)

        self.nodes[LEADER1].set_panid(0xface)
        self.nodes[LEADER1].set_mode('rsdn')
        self.nodes[LEADER1].add_whitelist(self.nodes[ROUTER1].get_addr64())
        self.nodes[LEADER1].add_whitelist(self.nodes[ROUTER2].get_addr64())
        self.nodes[LEADER1].enable_whitelist()
        self.nodes[LEADER1].set_weight(2)

        self.nodes[ROUTER1].set_panid(0xface)
        self.nodes[ROUTER1].set_mode('rsdn')
        self.nodes[ROUTER1].add_whitelist(self.nodes[LEADER1].get_addr64())
        self.nodes[ROUTER1].enable_whitelist()
        self.nodes[ROUTER1].set_weight(1)

        self.nodes[ROUTER2].set_panid(0xface)
        self.nodes[ROUTER2].set_mode('rsdn')
        self.nodes[ROUTER2].add_whitelist(self.nodes[LEADER1].get_addr64())
        self.nodes[ROUTER2].enable_whitelist()
        self.nodes[ROUTER2].set_weight(0)

    def tearDown(self):
        for node in self.nodes.itervalues():
            node.stop()
        del self.nodes

    def test(self):
        self.nodes[LEADER1].start()
        self.nodes[LEADER1].set_state('leader')
        self.assertEqual(self.nodes[LEADER1].get_state(), 'leader')

        self.nodes[ROUTER1].start()
        time.sleep(3)
        self.assertEqual(self.nodes[ROUTER1].get_state(), 'router')

        self.nodes[ROUTER2].start()
        time.sleep(3)
        self.assertEqual(self.nodes[ROUTER2].get_state(), 'router')

        self.nodes[LEADER1].stop()
        self.nodes[ROUTER1].add_whitelist(self.nodes[ROUTER2].get_addr64())
        self.nodes[ROUTER2].add_whitelist(self.nodes[ROUTER1].get_addr64())
        time.sleep(140)

        self.assertEqual(self.nodes[ROUTER1].get_state(), 'leader')
        self.assertEqual(self.nodes[ROUTER2].get_state(), 'router')

        addrs = self.nodes[ROUTER2].get_addrs()
        for addr in addrs:
            self.nodes[ROUTER1].ping(addr)

if __name__ == '__main__':
    unittest.main()
