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

import os
import sys
import pexpect
import re
import subprocess
import time
import unittest
import pprint

class Node:
    def __init__(self, nodeid):
        self.nodeid = nodeid
        self.pp = pprint.PrettyPrinter(indent=4)

        self.node_type = os.getenv('NODE_TYPE', 'sim')
        if self.node_type == 'soc':
            self.__init_soc(nodeid)
        else:
            self.__init_sim(nodeid)

        self.verbose = os.getenv('VERBOSE', 0)
        if self.verbose: 
            self.pexpect.logfile_read = sys.stdout

        self.clear_whitelist()
        self.disable_whitelist()
        self.set_timeout(100)

    def __init_sim(self, nodeid):
        """ Initialize a simulation node. """
        if "abs_builddir" in os.environ.keys():
            builddir = os.environ['abs_builddir']
            cmd = '%s/src/soc' % builddir
        else:
            cmd = './soc'
        cmd += ' --nodeid=%d' % nodeid

        FNULL = open(os.devnull, 'w')
        self.node_process = subprocess.Popen(cmd.split(), stdout = FNULL)
        #self.node_process = subprocess.Popen(cmd.split(), stdout = sys.stdout)
	
	time.sleep(0.5)

        cmd = 'nc -u 127.0.0.1 %d' % (8000 + nodeid)
        self.pexpect = pexpect.spawn(cmd, timeout=2)
        #print "\nNode %d : %s" % (nodeid, self.pp.pformat(self.pexpect))

    def __init_soc(self, nodeid):
        """ Initialize a System-on-a-chip node connected via UART. """
        import fdpexpect
        serialPort = '/dev/ttyUSB%d' % ((nodeid-1)*2)
        self.pexpect = fdpexpect.fdspawn(os.open(serialPort, os.O_RDWR|os.O_NONBLOCK|os.O_NOCTTY))
        #print "\nNode %d %s : %s" % (nodeid, serialPort, self.pp.pformat(self.pexpect))

    def __del__(self):
        if self.node_type == 'sim':
            self.send_command('shutdown')
            self.pexpect.expect('Done')
            self.node_process.wait()
        self.pexpect.close()

    def send_command(self, cmd):
        print self.nodeid, ":", cmd
        self.pexpect.sendline(cmd)

    def get_commands(self):
        self.send_command('?')
        self.pexpect.expect('Commands:')
        commands = []
        while True:
            i = self.pexpect.expect(['Done', '(\S+)'])
            if i != 0:
                commands.append(self.pexpect.match.groups()[0])
            else:
                break
        return commands

    def set_mode(self, mode):        
        cmd = 'mode ' + mode
        self.send_command(cmd)
        self.pexpect.expect('Done')

    def start(self):
        self.send_command('start')
        self.pexpect.expect('Done')

    def stop(self):
        self.send_command('stop')
        self.pexpect.expect('Done')

    def clear_whitelist(self):
        self.send_command('whitelist clear')
        self.pexpect.expect('Done')

    def enable_whitelist(self):
        self.send_command('whitelist enable')
        self.pexpect.expect('Done')

    def disable_whitelist(self):
        self.send_command('whitelist disable')
        self.pexpect.expect('Done')

    def add_whitelist(self, addr, rssi=None):
        cmd = 'whitelist add ' + addr
        if rssi != None:
            cmd += ' ' + str(rssi)
        self.send_command(cmd)
        self.pexpect.expect('Done')

    def remove_whitelist(self, addr):
        cmd = 'whitelist remove ' + addr
        self.send_command(cmd)
        self.pexpect.expect('Done')

    def get_addr16(self):
        self.send_command('rloc16')
        i = self.pexpect.expect('([0-9a-fA-F]{4})')
        if i == 0:
            addr16 = int(self.pexpect.match.groups()[0], 16)
        self.pexpect.expect('Done')
        return addr16

    def get_addr64(self):
        self.send_command('extaddr')
        i = self.pexpect.expect('([0-9a-fA-F]{16})')
        if i == 0:
            addr64 = self.pexpect.match.groups()[0]
        self.pexpect.expect('Done')
        return addr64

    def set_channel(self, channel):
        cmd = 'channel %d' % channel
        self.send_command(cmd)
        self.pexpect.expect('Done')

    def get_key_sequence(self):
        self.send_command('keysequence')
        i = self.pexpect.expect('(\d+)')
        if i == 0:
            key_sequence = int(self.pexpect.match.groups()[0])
        self.pexpect.expect('Done')
        return key_sequence

    def set_key_sequence(self, key_sequence):
        cmd = 'keysequence %d' % key_sequence
        self.send_command(cmd)
        self.pexpect.expect('Done')

    def set_network_id_timeout(self, network_id_timeout):
        cmd = 'networkidtimeout %d' % network_id_timeout
        self.send_command(cmd)
        self.pexpect.expect('Done')

    def set_network_name(self, network_name):
        cmd = 'networkname ' + network_name
        self.send_command(cmd)
        self.pexpect.expect('Done')

    def get_panid(self):
        self.send_command('panid')
        i = self.pexpect.expect('([0-9a-fA-F]{16})')
        if i == 0:
            panid = self.pexpect.match.groups()[0]
        self.pexpect.expect('Done')

    def set_panid(self, panid):
        cmd = 'panid %d' % panid
        self.send_command(cmd)
        self.pexpect.expect('Done')

    def set_router_upgrade_threshold(self, threshold):
        cmd = 'routerupgradethreshold %d' % threshold
        self.send_command(cmd)
        self.pexpect.expect('Done')

    def release_router_id(self, router_id):
        cmd = 'releaserouterid %d' % router_id
        self.send_command(cmd)
        self.pexpect.expect('Done')

    def get_state(self):
        states = ['detached', 'child', 'router', 'leader']
        self.send_command('state')
        match = self.pexpect.expect(states)
        self.pexpect.expect('Done')
        return states[match]

    def set_state(self, state):
        cmd = 'state ' + state
        self.send_command(cmd)
        self.pexpect.expect('Done')

    def get_timeout(self):
        self.send_command('childtimeout')
        i = self.pexpect.expect('(\d+)')
        if i == 0:
            timeout = self.pexpect.match.groups()[0]
        self.pexpect.expect('Done')
        return timeout

    def set_timeout(self, timeout):
        cmd = 'childtimeout %d' % timeout
        self.send_command(cmd)
        self.pexpect.expect('Done')

    def get_weight(self):
        self.send_command('leaderweight')
        i = self.pexpect.expect('(\d+)')
        if i == 0:
            weight = self.pexpect.match.groups()[0]
        self.pexpect.expect('Done')
        return weight

    def set_weight(self, weight):
        cmd = 'leaderweight %d' % weight
        self.send_command(cmd)
        self.pexpect.expect('Done')

    def add_ipaddr(self, ipaddr):
        cmd = 'ipaddr add ' + ipaddr
        self.send_command(cmd)
        self.pexpect.expect('Done')

    def get_addrs(self):
        addrs = []
        self.send_command('ipaddr')

        while True:
            i = self.pexpect.expect(['(\S+:\S+)', 'Done'])
            if i == 0:
                addrs.append(self.pexpect.match.groups()[0])
            elif i == 1:
                break

        return addrs

    def get_context_reuse_delay(self):
        self.send_command('contextreusedelay')
        i = self.pexpect.expect('(\d+)')
        if i == 0:
            timeout = self.pexpect.match.groups()[0]
        self.pexpect.expect('Done')
        return timeout

    def set_context_reuse_delay(self, delay):
        cmd = 'contextreusedelay %d' % delay
        self.send_command(cmd)
        self.pexpect.expect('Done')

    def add_prefix(self, prefix, flags, prf = 'med'):
        cmd = 'prefix add ' + prefix + ' ' + flags + ' ' + prf
        self.send_command(cmd)
        self.pexpect.expect('Done')

    def remove_prefix(self, prefix):
        cmd = ' prefix remove ' + prefix
        self.send_command(cmd)
        self.pexpect.expect('Done')

    def add_route(self, prefix, prf = 'med'):
        cmd = 'route add ' + prefix + ' ' + prf
        self.send_command(cmd)
        self.pexpect.expect('Done')

    def remove_route(self, prefix):
        cmd = 'route remove ' + prefix
        self.send_command(cmd)
        self.pexpect.expect('Done')

    def register_netdata(self):
        self.send_command('netdataregister')
        self.pexpect.expect('Done')

    def scan(self):
        self.send_command('scan')

        results = []
        while True:
            i = self.pexpect.expect(['\|\s(\S+)\s+\|\s(\S+)\s+\|\s([0-9a-fA-F]{4})\s\|\s([0-9a-fA-F]{16})\s\|\s(\d+)',
                                     'Done'])
            if i == 0:
                results.append(self.pexpect.match.groups())
            else:
                break

        return results

    def ping(self, ipaddr, num_responses=1, size=None):
        cmd = 'ping ' + ipaddr
        if size != None:
            cmd += ' ' + str(size)

        self.send_command(cmd)
        responders = {}
        while len(responders) < num_responses:
            i = self.pexpect.expect(['from (\S+):'])
            if i == 0:
                responders[self.pexpect.match.groups()[0]] = 1
        self.pexpect.expect('\n')
        return responders

if __name__ == '__main__':
    unittest.main()
