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

import time
import unittest

import node

LEADER = 1

class Cert_Cli(unittest.TestCase):
    def setUp(self):
        self.nodes = {}
        self.nodes[LEADER] = node.Node(LEADER)

    def tearDown(self):
        for node in self.nodes.itervalues():
            node.stop()
        del self.nodes

    def test(self):
        commands = self.nodes[LEADER].get_commands()

        for command in commands:
            self.nodes[LEADER].send_command(command + ' -h')
            self.nodes[LEADER].pexpect.expect('Done')

if __name__ == '__main__':
    unittest.main()
