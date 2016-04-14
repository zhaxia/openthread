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

ED1 = 1
BR1 = 2
LEADER = 3
ROUTER2 = 4
REED = 5
ED2 = 6
ED3 = 7

class Cert_5_2_5_AddressQuery(unittest.TestCase):
    def setUp(self):
        self.nodes = {}
        for i in range(1,8):
            self.nodes[i] = node.Node(i)

        self.nodes[LEADER].set_panid(0xface)
        self.nodes[LEADER].set_mode('rsdn')
        self.nodes[LEADER].add_whitelist(self.nodes[BR1].get_addr64())
        self.nodes[LEADER].add_whitelist(self.nodes[ROUTER2].get_addr64())
        self.nodes[LEADER].add_whitelist(self.nodes[REED].get_addr64())
        self.nodes[LEADER].enable_whitelist()

        self.nodes[BR1].set_panid(0xface)
        self.nodes[BR1].set_mode('rsdn')
        self.nodes[BR1].add_whitelist(self.nodes[LEADER].get_addr64())
        self.nodes[BR1].add_whitelist(self.nodes[ED1].get_addr64())
        self.nodes[BR1].enable_whitelist()

        self.nodes[ED1].set_panid(0xface)
        self.nodes[ED1].set_mode('rsn')
        self.nodes[ED1].add_whitelist(self.nodes[BR1].get_addr64())
        self.nodes[ED1].enable_whitelist()

        self.nodes[REED].set_panid(0xface)
        self.nodes[REED].set_mode('rsdn')
        self.nodes[REED].add_whitelist(self.nodes[LEADER].get_addr64())
        self.nodes[REED].add_whitelist(self.nodes[ROUTER2].get_addr64())
        self.nodes[REED].set_router_upgrade_threshold(0)
        self.nodes[REED].enable_whitelist()

        self.nodes[ROUTER2].set_panid(0xface)
        self.nodes[ROUTER2].set_mode('rsdn')
        self.nodes[ROUTER2].add_whitelist(self.nodes[LEADER].get_addr64())
        self.nodes[ROUTER2].add_whitelist(self.nodes[REED].get_addr64())
        self.nodes[ROUTER2].add_whitelist(self.nodes[ED2].get_addr64())
        self.nodes[ROUTER2].add_whitelist(self.nodes[ED3].get_addr64())
        self.nodes[ROUTER2].enable_whitelist()

        self.nodes[ED2].set_panid(0xface)
        self.nodes[ED2].set_mode('rsn')
        self.nodes[ED2].add_whitelist(self.nodes[ROUTER2].get_addr64())
        self.nodes[ED2].enable_whitelist()

        self.nodes[ED3].set_panid(0xface)
        self.nodes[ED3].set_mode('rsn')
        self.nodes[ED3].add_whitelist(self.nodes[ROUTER2].get_addr64())
        self.nodes[ED3].enable_whitelist()

    def tearDown(self):
        for node in self.nodes.itervalues():
            node.stop()
        del self.nodes

    def test(self):
        self.nodes[LEADER].start()
        self.nodes[LEADER].set_state('leader')
        self.assertEqual(self.nodes[LEADER].get_state(), 'leader')

        self.nodes[BR1].start()
        time.sleep(3)
        self.assertEqual(self.nodes[BR1].get_state(), 'router')

        self.nodes[BR1].add_prefix('2003::/64', 'pvcrs')
        self.nodes[BR1].add_prefix('2004::/64', 'pvcrs')
        self.nodes[BR1].register_netdata()

        self.nodes[ED1].start()
        time.sleep(3)
        self.assertEqual(self.nodes[ED1].get_state(), 'child')

        self.nodes[REED].start()
        time.sleep(3)
        self.assertEqual(self.nodes[REED].get_state(), 'child')

        self.nodes[ROUTER2].start()
        time.sleep(3)
        self.assertEqual(self.nodes[ROUTER2].get_state(), 'router')

        self.nodes[ED2].start()
        time.sleep(3)
        self.assertEqual(self.nodes[ED2].get_state(), 'child')

        self.nodes[ED3].start()
        time.sleep(3)
        self.assertEqual(self.nodes[ED3].get_state(), 'child')

        addrs = self.nodes[REED].get_addrs()
        for addr in addrs:
            if addr[0:4] != 'fe80':
                self.nodes[ED2].ping(addr)
                time.sleep(1)

if __name__ == '__main__':
    unittest.main()
