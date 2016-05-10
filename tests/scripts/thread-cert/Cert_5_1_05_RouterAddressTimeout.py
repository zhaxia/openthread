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

class Cert_5_1_05_RouterAddressTimeout(unittest.TestCase):
    def setUp(self):
        self.nodes = {}
        for i in range(1,3):
            self.nodes[i] = node.Node(i)

        self.nodes[LEADER].set_panid(0xface)
        self.nodes[LEADER].set_mode('rsdn')
        self.nodes[LEADER].add_whitelist(self.nodes[ROUTER1].get_addr64())
        self.nodes[LEADER].enable_whitelist()

        self.nodes[ROUTER1].set_panid(0xface)
        self.nodes[ROUTER1].set_mode('rsdn')
        self.nodes[ROUTER1].add_whitelist(self.nodes[LEADER].get_addr64())
        self.nodes[ROUTER1].enable_whitelist()

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

        rloc16 = self.nodes[ROUTER1].get_addr16()

        self.nodes[ROUTER1].stop()
        time.sleep(200)
        self.nodes[ROUTER1].start()
        time.sleep(5)
        self.assertEqual(self.nodes[ROUTER1].get_state(), 'router')
        self.assertNotEqual(self.nodes[ROUTER1].get_addr16(), rloc16)

        rloc16 = self.nodes[ROUTER1].get_addr16()

        self.nodes[ROUTER1].stop()
        time.sleep(300)
        self.nodes[ROUTER1].start()
        time.sleep(5)
        self.assertEqual(self.nodes[ROUTER1].get_state(), 'router')
        self.assertEqual(self.nodes[ROUTER1].get_addr16(), rloc16)

if __name__ == '__main__':
    unittest.main()
