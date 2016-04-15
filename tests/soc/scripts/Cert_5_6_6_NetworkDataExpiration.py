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
ED1 = 3
SED1 = 4

class Cert_5_6_6_NetworkDataExpiration(unittest.TestCase):
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
        self.nodes[ROUTER].add_whitelist(self.nodes[ED1].get_addr64())
        self.nodes[ROUTER].add_whitelist(self.nodes[SED1].get_addr64())
        self.nodes[ROUTER].enable_whitelist()

        self.nodes[ED1].set_panid(0xface)
        self.nodes[ED1].set_mode('rsn')
        self.nodes[ED1].add_whitelist(self.nodes[ROUTER].get_addr64())
        self.nodes[ED1].enable_whitelist()

        self.nodes[SED1].set_panid(0xface)
        self.nodes[SED1].set_mode('s')
        self.nodes[SED1].add_whitelist(self.nodes[ROUTER].get_addr64())
        self.nodes[SED1].enable_whitelist()
        self.nodes[SED1].set_timeout(3)

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

        self.nodes[ED1].start()
        time.sleep(3)
        self.assertEqual(self.nodes[ED1].get_state(), 'child')

        self.nodes[SED1].start()
        time.sleep(3)
        self.assertEqual(self.nodes[SED1].get_state(), 'child')

        self.nodes[ROUTER].add_prefix('2001::/64', 'pvcrs')
        self.nodes[ROUTER].add_prefix('2002::/64', 'pvcr')
        self.nodes[ROUTER].register_netdata()

        time.sleep(10)

        addrs = self.nodes[ED1].get_addrs()
        for addr in addrs:
            if addr[0:3] == '200':
                self.nodes[LEADER].ping(addr)

        addrs = self.nodes[SED1].get_addrs()
        for addr in addrs:
            if addr[0:3] == '200':
                self.nodes[LEADER].ping(addr)

        self.nodes[ROUTER].add_prefix('2003::/64', 'pvcs')
        self.nodes[ROUTER].register_netdata()

        time.sleep(10)

        addrs = self.nodes[ED1].get_addrs()
        for addr in addrs:
            if addr[0:3] == '200':
                self.nodes[LEADER].ping(addr)

        addrs = self.nodes[SED1].get_addrs()
        for addr in addrs:
            if addr[0:3] == '200':
                self.nodes[LEADER].ping(addr)

        self.nodes[ROUTER].remove_prefix('2003::/64')
        self.nodes[ROUTER].register_netdata()
        time.sleep(10)

        addrs = self.nodes[ED1].get_addrs()
        for addr in addrs:
            if addr[0:3] == '200':
                self.nodes[LEADER].ping(addr)

        addrs = self.nodes[SED1].get_addrs()
        for addr in addrs:
            if addr[0:3] == '200':
                self.nodes[LEADER].ping(addr)

        self.nodes[ROUTER].stop()

if __name__ == '__main__':
    unittest.main()
