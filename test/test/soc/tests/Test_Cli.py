#!/usr/bin/python

#
#    Copyright (c) 2015 Nest Labs, Inc.
#    All rights reserved.
#
#    This document is the property of Nest. It is considered
#    confidential and proprietary information.
#
#    This document may not be reproduced or transmitted in any form,
#    in whole or in part, without the express written permission of
#    Nest.
#
#    @author  Jonathan Hui <jonhui@nestlabs.com>
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
