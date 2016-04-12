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

BR = 1
LEADER = 2
ROUTER2 = 3
ROUTER3 = 4
ED2 = 5

class Cert_5_3_3_AddressQuery(unittest.TestCase):
    def setUp(self):
        self.nodes = {}
        for i in range(1,6):
            self.nodes[i] = node.Node(i)

        self.nodes[LEADER].set_panid(0xface)
        self.nodes[LEADER].set_mode('rsdn')
        self.nodes[LEADER].add_whitelist(self.nodes[BR].get_addr64())
        self.nodes[LEADER].add_whitelist(self.nodes[ROUTER2].get_addr64())
        self.nodes[LEADER].add_whitelist(self.nodes[ROUTER3].get_addr64())
        self.nodes[LEADER].enable_whitelist()

        self.nodes[BR].set_panid(0xface)
        self.nodes[BR].set_mode('rsdn')
        self.nodes[BR].add_whitelist(self.nodes[LEADER].get_addr64())
        self.nodes[BR].enable_whitelist()

        self.nodes[ROUTER2].set_panid(0xface)
        self.nodes[ROUTER2].set_mode('rsdn')
        self.nodes[ROUTER2].add_whitelist(self.nodes[LEADER].get_addr64())
        self.nodes[ROUTER2].add_whitelist(self.nodes[ROUTER3].get_addr64())
        self.nodes[ROUTER2].add_whitelist(self.nodes[ED2].get_addr64())
        self.nodes[ROUTER2].enable_whitelist()

        self.nodes[ROUTER3].set_panid(0xface)
        self.nodes[ROUTER3].set_mode('rsdn')
        self.nodes[ROUTER3].add_whitelist(self.nodes[LEADER].get_addr64())
        self.nodes[ROUTER3].add_whitelist(self.nodes[ROUTER2].get_addr64())
        self.nodes[ROUTER3].enable_whitelist()

        self.nodes[ED2].set_panid(0xface)
        self.nodes[ED2].set_mode('rsn')
        self.nodes[ED2].add_whitelist(self.nodes[ROUTER2].get_addr64())
        self.nodes[ED2].set_timeout(3)
        self.nodes[ED2].enable_whitelist()

    def tearDown(self):
        for node in self.nodes.itervalues():
            node.stop()
        del self.nodes

    def test(self):
        self.nodes[LEADER].start()
        self.nodes[LEADER].set_state('leader')
        self.assertEqual(self.nodes[LEADER].get_state(), 'leader')

        self.nodes[BR].start()
        time.sleep(3)
        self.assertEqual(self.nodes[BR].get_state(), 'router')

        self.nodes[BR].add_prefix('2003::/64', 'pvcrs')
        self.nodes[BR].add_prefix('2004::/64', 'pvcrs')
        self.nodes[BR].register_netdata()

        self.nodes[ROUTER2].start()
        time.sleep(3)
        self.assertEqual(self.nodes[ROUTER2].get_state(), 'router')

        self.nodes[ROUTER3].start()
        time.sleep(3)
        self.assertEqual(self.nodes[ROUTER3].get_state(), 'router')

        self.nodes[ED2].start()
        time.sleep(3)
        self.assertEqual(self.nodes[ED2].get_state(), 'child')

        addrs = self.nodes[ROUTER3].get_addrs()
        for addr in addrs:
            if addr[0:4] != 'fe80':
                self.nodes[ED2].ping(addr)
                time.sleep(1)

        addrs = self.nodes[ED2].get_addrs()
        for addr in addrs:
            if addr[0:4] != 'fe80':
                self.nodes[LEADER].ping(addr)
                time.sleep(1)

        addrs = self.nodes[BR].get_addrs()
        for addr in addrs:
            if addr[0:4] != 'fe80':
                self.nodes[ED2].ping(addr)
                time.sleep(1)

        addrs = self.nodes[ROUTER3].get_addrs()
        for addr in addrs:
            if addr[0:4] != 'fe80':
                self.nodes[ED2].ping(addr)
                time.sleep(1)

        addrs = self.nodes[ROUTER3].get_addrs()
        self.nodes[ROUTER3].stop()
        time.sleep(130)

        for addr in addrs:
            if addr[0:4] != 'fe80':
                try:
                    self.nodes[ED2].ping(addr)
                    self.assertFalse()
                except pexpect.TIMEOUT:
                    pass

        addrs = self.nodes[ED2].get_addrs()
        self.nodes[ED2].stop()
        time.sleep(10)
        for addr in addrs:
            if addr[0:4] != 'fe80':
                try:
                    self.nodes[BR].ping(addr)        
                    self.assertFalse()
                except pexpect.TIMEOUT:
                    pass

if __name__ == '__main__':
    unittest.main()
