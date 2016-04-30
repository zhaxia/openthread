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
SED = 4

class Cert_5_1_02_ChildAddressTimeout(unittest.TestCase):
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
        self.nodes[ROUTER].add_whitelist(self.nodes[ED].get_addr64())
        self.nodes[ROUTER].add_whitelist(self.nodes[SED].get_addr64())
        self.nodes[ROUTER].enable_whitelist()

        self.nodes[ED].set_panid(0xface)
        self.nodes[ED].set_mode('rsn')
        self.nodes[ED].set_timeout(3)
        self.nodes[ED].add_whitelist(self.nodes[ROUTER].get_addr64())
        self.nodes[ED].enable_whitelist()

        self.nodes[SED].set_panid(0xface)
        self.nodes[SED].set_mode('sn')
        self.nodes[SED].set_timeout(3)
        self.nodes[SED].add_whitelist(self.nodes[ROUTER].get_addr64())
        self.nodes[SED].enable_whitelist()

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

        self.nodes[SED].start()
        time.sleep(3)
        self.assertEqual(self.nodes[SED].get_state(), 'child')

        ed_addrs = self.nodes[ED].get_addrs()
        sed_addrs = self.nodes[SED].get_addrs()

        self.nodes[ED].stop()
        time.sleep(5)
        for addr in ed_addrs:
            if addr[0:4] != 'fe80':
                try:
                    self.nodes[LEADER].ping(addr)
                    self.assertFalse()
                except pexpect.TIMEOUT:
                    pass

        self.nodes[SED].stop()
        time.sleep(5)
        for addr in sed_addrs:
            if addr[0:4] != 'fe80':
                try:
                    self.nodes[LEADER].ping(addr)
                    self.assertFalse()
                except pexpect.TIMEOUT:
                    pass

if __name__ == '__main__':
    unittest.main()
