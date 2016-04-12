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

LEADER = 1
ROUTER1 = 2
ROUTER2 = 3
ED1 = 4
ED2 = 5
ED3 = 6

class Cert_5_3_7_DuplicateAddress(unittest.TestCase):
    def setUp(self):
        self.nodes = {}
        for i in range(1,7):
            self.nodes[i] = node.Node(i)

        self.nodes[LEADER].set_panid(0xface)
        self.nodes[LEADER].set_mode('rsdn')
        self.nodes[LEADER].add_whitelist(self.nodes[ROUTER1].get_addr64())
        self.nodes[LEADER].add_whitelist(self.nodes[ROUTER2].get_addr64())
        self.nodes[LEADER].add_whitelist(self.nodes[ED3].get_addr64())
        self.nodes[LEADER].enable_whitelist()

        self.nodes[ROUTER1].set_panid(0xface)
        self.nodes[ROUTER1].set_mode('rsdn')
        self.nodes[ROUTER1].add_whitelist(self.nodes[LEADER].get_addr64())
        self.nodes[ROUTER1].add_whitelist(self.nodes[ED1].get_addr64())
        self.nodes[ROUTER1].enable_whitelist()

        self.nodes[ROUTER2].set_panid(0xface)
        self.nodes[ROUTER2].set_mode('rsdn')
        self.nodes[ROUTER2].add_whitelist(self.nodes[LEADER].get_addr64())
        self.nodes[ROUTER2].add_whitelist(self.nodes[ED2].get_addr64())
        self.nodes[ROUTER2].enable_whitelist()

        self.nodes[ED1].set_panid(0xface)
        self.nodes[ED1].set_mode('rsn')
        self.nodes[ED1].add_whitelist(self.nodes[ROUTER1].get_addr64())
        self.nodes[ED1].enable_whitelist()

        self.nodes[ED2].set_panid(0xface)
        self.nodes[ED2].set_mode('rsn')
        self.nodes[ED2].add_whitelist(self.nodes[ROUTER2].get_addr64())
        self.nodes[ED2].enable_whitelist()

        self.nodes[ED3].set_panid(0xface)
        self.nodes[ED3].set_mode('rsn')
        self.nodes[ED3].add_whitelist(self.nodes[LEADER].get_addr64())
        self.nodes[ED3].enable_whitelist()

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

        self.nodes[ROUTER2].start()
        time.sleep(3)
        self.assertEqual(self.nodes[ROUTER2].get_state(), 'router')

        self.nodes[ED1].start()
        time.sleep(3)
        self.assertEqual(self.nodes[ED1].get_state(), 'child')

        self.nodes[ED2].start()
        time.sleep(3)
        self.assertEqual(self.nodes[ED2].get_state(), 'child')

        self.nodes[ED3].start()
        time.sleep(3)
        self.assertEqual(self.nodes[ED3].get_state(), 'child')

        self.nodes[ROUTER2].add_prefix('2001::/64', 'pvcrs')
        self.nodes[ROUTER2].register_netdata()

        self.nodes[ED1].add_ipaddr('2001::1')
        self.nodes[ED2].add_ipaddr('2001::1')
        time.sleep(3)

        self.nodes[ED3].ping('2001::1')

if __name__ == '__main__':
    unittest.main()
