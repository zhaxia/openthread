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
ROUTER3 = 4
ROUTER4 = 5
ED1 = 6

class Cert_5_5_4_SplitMergeRouters(unittest.TestCase):
    def setUp(self):
        self.nodes = {}
        for i in range(1,7):
            self.nodes[i] = node.Node(i)

        self.nodes[LEADER1].set_panid(0xface)
        self.nodes[LEADER1].set_mode('rsdn')
        self.nodes[LEADER1].add_whitelist(self.nodes[ROUTER1].get_addr64())
        self.nodes[LEADER1].add_whitelist(self.nodes[ROUTER2].get_addr64())
        self.nodes[LEADER1].add_whitelist(self.nodes[ED1].get_addr64())
        self.nodes[LEADER1].enable_whitelist()

        self.nodes[ROUTER1].set_panid(0xface)
        self.nodes[ROUTER1].set_mode('rsdn')
        self.nodes[ROUTER1].add_whitelist(self.nodes[LEADER1].get_addr64())
        self.nodes[ROUTER1].add_whitelist(self.nodes[ROUTER3].get_addr64())
        self.nodes[ROUTER1].enable_whitelist()

        self.nodes[ROUTER2].set_panid(0xface)
        self.nodes[ROUTER2].set_mode('rsdn')
        self.nodes[ROUTER2].add_whitelist(self.nodes[LEADER1].get_addr64())
        self.nodes[ROUTER2].add_whitelist(self.nodes[ROUTER4].get_addr64())
        self.nodes[ROUTER2].enable_whitelist()

        self.nodes[ROUTER3].set_panid(0xface)
        self.nodes[ROUTER3].set_mode('rsdn')
        self.nodes[ROUTER3].add_whitelist(self.nodes[ROUTER1].get_addr64())
        self.nodes[ROUTER3].enable_whitelist()
        self.nodes[ROUTER3].set_network_id_timeout(110)

        self.nodes[ROUTER4].set_panid(0xface)
        self.nodes[ROUTER4].set_mode('rsdn')
        self.nodes[ROUTER4].add_whitelist(self.nodes[ROUTER2].get_addr64())
        self.nodes[ROUTER4].enable_whitelist()

        self.nodes[ED1].set_panid(0xface)
        self.nodes[ED1].set_mode('rsn')
        self.nodes[ED1].add_whitelist(self.nodes[LEADER1].get_addr64())
        self.nodes[ED1].enable_whitelist()

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

        self.nodes[ROUTER3].start()
        time.sleep(3)
        self.assertEqual(self.nodes[ROUTER3].get_state(), 'router')

        self.nodes[ROUTER4].start()
        time.sleep(3)
        self.assertEqual(self.nodes[ROUTER4].get_state(), 'router')

        self.nodes[ED1].start()
        time.sleep(3)
        self.assertEqual(self.nodes[ED1].get_state(), 'child')

        self.nodes[LEADER1].stop()

        self.nodes[ED1].add_whitelist(self.nodes[ROUTER1].get_addr64())
        self.nodes[ROUTER1].add_whitelist(self.nodes[ED1].get_addr64())

        time.sleep(130)
        #self.assertEqual(self.nodes[ROUTER3].get_state(), 'leader')
        #self.assertEqual(self.nodes[ROUTER1].get_state(), 'router')

        self.nodes[LEADER1].start()
        time.sleep(5)
        self.assertEqual(self.nodes[LEADER1].get_state(), 'router')

        time.sleep(60)

        addrs = self.nodes[ED1].get_addrs()
        for addr in addrs:
            if addr[0:4] != 'fe80':
                self.nodes[ROUTER2].ping(addr)

if __name__ == '__main__':
    unittest.main()
