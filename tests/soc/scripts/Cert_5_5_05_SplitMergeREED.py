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
REED2 = 5

class Cert_5_5_5_SplitMergeREED(unittest.TestCase):
    def setUp(self):
        self.nodes = {}
        for i in range(1,6):
            self.nodes[i] = node.Node(i)

        self.nodes[LEADER1].set_panid(0xface)
        self.nodes[LEADER1].set_mode('rsdn')
        self.nodes[LEADER1].add_whitelist(self.nodes[ROUTER2].get_addr64())
        self.nodes[LEADER1].add_whitelist(self.nodes[ROUTER3].get_addr64())
        self.nodes[LEADER1].enable_whitelist()

        self.nodes[ROUTER1].set_panid(0xface)
        self.nodes[ROUTER1].set_mode('rsdn')
        self.nodes[ROUTER1].add_whitelist(self.nodes[ROUTER3].get_addr64())
        self.nodes[ROUTER1].add_whitelist(self.nodes[REED2].get_addr64())
        self.nodes[ROUTER1].enable_whitelist()

        self.nodes[ROUTER2].set_panid(0xface)
        self.nodes[ROUTER2].set_mode('rsdn')
        self.nodes[ROUTER2].add_whitelist(self.nodes[LEADER1].get_addr64())
        self.nodes[ROUTER2].add_whitelist(self.nodes[REED2].get_addr64())
        self.nodes[ROUTER2].enable_whitelist()

        self.nodes[ROUTER3].set_panid(0xface)
        self.nodes[ROUTER3].set_mode('rsdn')
        self.nodes[ROUTER3].add_whitelist(self.nodes[LEADER1].get_addr64())
        self.nodes[ROUTER3].add_whitelist(self.nodes[ROUTER1].get_addr64())
        self.nodes[ROUTER3].enable_whitelist()

        self.nodes[REED2].set_panid(0xface)
        self.nodes[REED2].set_mode('rsdn')
        self.nodes[REED2].add_whitelist(self.nodes[ROUTER1].get_addr64())
        self.nodes[REED2].add_whitelist(self.nodes[ROUTER2].get_addr64())
        self.nodes[REED2].enable_whitelist()
        self.nodes[REED2].set_router_upgrade_threshold(0)

    def tearDown(self):
        for node in self.nodes.itervalues():
            node.stop()
        del self.nodes

    def test(self):
        self.nodes[LEADER1].start()
        self.nodes[LEADER1].set_state('leader')
        self.assertEqual(self.nodes[LEADER1].get_state(), 'leader')

        self.nodes[ROUTER2].start()
        time.sleep(3)
        self.assertEqual(self.nodes[ROUTER2].get_state(), 'router')

        self.nodes[REED2].start()
        time.sleep(3)
        self.assertEqual(self.nodes[REED2].get_state(), 'child')

        self.nodes[ROUTER3].start()
        time.sleep(3)
        self.assertEqual(self.nodes[ROUTER3].get_state(), 'router')

        self.nodes[ROUTER1].start()
        time.sleep(3)
        self.assertEqual(self.nodes[ROUTER1].get_state(), 'router')

        self.nodes[ROUTER3].stop()
        time.sleep(150)

        self.assertEqual(self.nodes[REED2].get_state(), 'router')
        self.assertEqual(self.nodes[ROUTER1].get_state(), 'router')

        addrs = self.nodes[LEADER1].get_addrs()
        for addr in addrs:
            if addr[0:4] != 'fe80':
                self.nodes[ROUTER1].ping(addr)

if __name__ == '__main__':
    unittest.main()
