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
ROUTER = 2
ED2 = 3
SED2 = 4

class Cert_7_1_4_BorderRouterAsRouter(unittest.TestCase):
    def setUp(self):
        self.nodes = {}
        for i in range(1,5):
            self.nodes[i] = node.Node(i)

        self.nodes[LEADER].set_panid(0xface)
        self.nodes[LEADER].set_mode('rsdn')
        self.nodes[LEADER].add_whitelist(self.nodes[ROUTER].get_addr64())
        self.nodes[LEADER].enable_whitelist()

        self.nodes[ROUTER].set_panid(0xface)
        self.nodes[ROUTER].set_mode('rsdn')
        self.nodes[ROUTER].add_whitelist(self.nodes[LEADER].get_addr64())
        self.nodes[ROUTER].add_whitelist(self.nodes[ED2].get_addr64())
        self.nodes[ROUTER].add_whitelist(self.nodes[SED2].get_addr64())
        self.nodes[ROUTER].enable_whitelist()

        self.nodes[ED2].set_panid(0xface)
        self.nodes[ED2].set_mode('rsn')
        self.nodes[ED2].add_whitelist(self.nodes[ROUTER].get_addr64())
        self.nodes[ED2].enable_whitelist()

        self.nodes[SED2].set_panid(0xface)
        self.nodes[SED2].set_mode('s')
        self.nodes[SED2].add_whitelist(self.nodes[ROUTER].get_addr64())
        self.nodes[SED2].enable_whitelist()
        self.nodes[SED2].set_timeout(3)

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

        self.nodes[ED2].start()
        time.sleep(3)
        self.assertEqual(self.nodes[ED2].get_state(), 'child')

        self.nodes[SED2].start()
        time.sleep(3)
        self.assertEqual(self.nodes[SED2].get_state(), 'child')

        self.nodes[ROUTER].add_prefix('2001::/64', 'pvcrs')
        self.nodes[ROUTER].add_prefix('2002::/64', 'pvcr')
        self.nodes[ROUTER].register_netdata()
        time.sleep(3)

        addrs = self.nodes[ED2].get_addrs()
        for addr in addrs:
            if addr[0:4] == '2001' or addr[0:4] == '2002':
                self.nodes[LEADER].ping(addr)

        addrs = self.nodes[SED2].get_addrs()
        for addr in addrs:
            if addr[0:4] == '2001' or addr[0:4] == '2002':
                self.nodes[LEADER].ping(addr)

if __name__ == '__main__':
    unittest.main()
