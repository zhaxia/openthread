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
ED = 3

class Cert_5_6_8_ContextManagement(unittest.TestCase):
    def setUp(self):
        self.nodes = {}
        for i in range(1,4):
            self.nodes[i] = node.Node(i)

        self.nodes[LEADER].set_panid(0xface)
        self.nodes[LEADER].set_mode('rsdn')
        self.nodes[LEADER].add_whitelist(self.nodes[ROUTER].get_addr64())
        self.nodes[LEADER].add_whitelist(self.nodes[ED].get_addr64())
        self.nodes[LEADER].enable_whitelist()
        self.nodes[LEADER].set_context_reuse_delay(10)

        self.nodes[ROUTER].set_panid(0xface)
        self.nodes[ROUTER].set_mode('rsdn')
        self.nodes[ROUTER].add_whitelist(self.nodes[LEADER].get_addr64())
        self.nodes[ROUTER].enable_whitelist()

        self.nodes[ED].set_panid(0xface)
        self.nodes[ED].set_mode('rsn')
        self.nodes[ED].add_whitelist(self.nodes[LEADER].get_addr64())
        self.nodes[ED].enable_whitelist()

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

        self.nodes[ED].start()
        time.sleep(3)
        self.assertEqual(self.nodes[ED].get_state(), 'child')

        self.nodes[ROUTER].add_prefix('2001::/64', 'pvcrs')
        self.nodes[ROUTER].register_netdata()
        time.sleep(2)

        addrs = self.nodes[LEADER].get_addrs()
        for addr in addrs:
            if addr[0:3] == '200':
                self.nodes[ED].ping(addr)

        self.nodes[ROUTER].remove_prefix('2001::/64')
        self.nodes[ROUTER].register_netdata()
        time.sleep(5)

        addrs = self.nodes[LEADER].get_addrs()
        for addr in addrs:
            if addr[0:3] == '200':
                self.nodes[ED].ping(addr)

        self.nodes[ROUTER].add_prefix('2002::/64', 'pvcrs')
        self.nodes[ROUTER].register_netdata()
        time.sleep(5)

        addrs = self.nodes[LEADER].get_addrs()
        for addr in addrs:
            if addr[0:3] == '200':
                self.nodes[ED].ping(addr)

        time.sleep(5)
        self.nodes[ROUTER].add_prefix('2003::/64', 'pvcrs')
        self.nodes[ROUTER].register_netdata()
        time.sleep(5)

        addrs = self.nodes[LEADER].get_addrs()
        for addr in addrs:
            if addr[0:3] == '200':
                self.nodes[ED].ping(addr)

if __name__ == '__main__':
    unittest.main()
