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
ED = 4
SED = 5

class Cert_5_6_9_NetworkDataForwarding(unittest.TestCase):
    def setUp(self):
        self.nodes = {}
        for i in range(1,6):
            self.nodes[i] = node.Node(i)

        self.nodes[LEADER].set_panid(0xface)
        self.nodes[LEADER].set_mode('rsdn')
        self.nodes[LEADER].add_whitelist(self.nodes[ROUTER1].get_addr64())
        self.nodes[LEADER].add_whitelist(self.nodes[ROUTER2].get_addr64())
        self.nodes[LEADER].enable_whitelist()

        self.nodes[ROUTER1].set_panid(0xface)
        self.nodes[ROUTER1].set_mode('rsdn')
        self.nodes[ROUTER1].add_whitelist(self.nodes[LEADER].get_addr64())
        self.nodes[ROUTER1].add_whitelist(self.nodes[ED].get_addr64())
        self.nodes[ROUTER1].add_whitelist(self.nodes[SED].get_addr64())
        self.nodes[ROUTER1].enable_whitelist()

        self.nodes[ROUTER2].set_panid(0xface)
        self.nodes[ROUTER2].set_mode('rsdn')
        self.nodes[ROUTER2].add_whitelist(self.nodes[LEADER].get_addr64())
        self.nodes[ROUTER2].enable_whitelist()

        self.nodes[ED].set_panid(0xface)
        self.nodes[ED].set_mode('rsn')
        self.nodes[ED].add_whitelist(self.nodes[ROUTER1].get_addr64())
        self.nodes[ED].enable_whitelist()

        self.nodes[SED].set_panid(0xface)
        self.nodes[SED].set_mode('s')
        self.nodes[SED].add_whitelist(self.nodes[ROUTER1].get_addr64())
        self.nodes[SED].enable_whitelist()
        self.nodes[SED].set_timeout(3)

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

        self.nodes[ED].start()
        time.sleep(3)
        self.assertEqual(self.nodes[ED].get_state(), 'child')

        self.nodes[SED].start()
        time.sleep(3)
        self.assertEqual(self.nodes[SED].get_state(), 'child')

        self.nodes[LEADER].add_prefix('2001::/64', 'pvcrs', 'med')
        self.nodes[LEADER].add_route('2002::/64', 'med')
        self.nodes[LEADER].register_netdata()
        time.sleep(10)

        self.nodes[ROUTER2].add_prefix('2001::/64', 'pvcrs', 'low')
        self.nodes[ROUTER2].add_route('2002::/64', 'high')
        self.nodes[ROUTER2].register_netdata()
        time.sleep(10)

        try:
            self.nodes[SED].ping('2002::1')
        except pexpect.TIMEOUT:
            pass

        try:
            self.nodes[SED].ping('2007::1')
        except pexpect.TIMEOUT:
            pass

        self.nodes[ROUTER2].remove_prefix('2001::/64')
        self.nodes[ROUTER2].add_prefix('2001::/64', 'pvcrs', 'high')
        self.nodes[ROUTER2].register_netdata()
        time.sleep(10)

        try:
            self.nodes[SED].ping('2007::1')
        except pexpect.TIMEOUT:
            pass

        self.nodes[ROUTER2].remove_prefix('2001::/64')
        self.nodes[ROUTER2].add_prefix('2001::/64', 'pvcrs', 'med')
        self.nodes[ROUTER2].register_netdata()
        time.sleep(10)

        try:
            self.nodes[SED].ping('2007::1')
        except pexpect.TIMEOUT:
            pass

if __name__ == '__main__':
    unittest.main()
