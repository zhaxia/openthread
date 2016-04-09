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
        cmd = './soc --eui64=%d' % nodeid
        FNULL = open(os.devnull, 'w')
        self.node_process = subprocess.Popen(cmd.split(), stdout = FNULL)
	
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
        #print self.nodeid, ":", cmd
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
        cmd = 'thread mode ' + mode
        self.send_command(cmd)
        self.pexpect.expect('Done')

    def start(self):
        self.send_command('thread start')
        self.pexpect.expect('Done')

    def stop(self):
        self.send_command('thread stop')
        self.pexpect.expect('Done')

    def clear_whitelist(self):
        self.send_command('mac whitelist clear')
        self.pexpect.expect('Done')

    def enable_whitelist(self):
        self.send_command('mac whitelist enable')
        self.pexpect.expect('Done')

    def disable_whitelist(self):
        self.send_command('mac whitelist disable')
        self.pexpect.expect('Done')

    def add_whitelist(self, addr, rssi=None):
        cmd = 'mac whitelist add ' + addr
        if rssi != None:
            cmd += ' ' + str(rssi)
        self.send_command(cmd)
        self.pexpect.expect('Done')

    def remove_whitelist(self, addr):
        cmd = 'mac whitelist remove ' + addr
        self.send_command(cmd)
        self.pexpect.expect('Done')

    def get_addr16(self):
        self.send_command('mac addr16')
        i = self.pexpect.expect('([0-9a-fA-F]{4})')
        if i == 0:
            addr16 = int(self.pexpect.match.groups()[0], 16)
        self.pexpect.expect('Done')
        return addr16

    def get_addr64(self):
        self.send_command('mac addr64')
        i = self.pexpect.expect('([0-9a-fA-F]{16})')
        if i == 0:
            addr64 = self.pexpect.match.groups()[0]
        self.pexpect.expect('Done')
        return addr64

    def set_channel(self, channel):
        cmd = 'mac channel %d' % channel
        self.send_command(cmd)
        self.pexpect.expect('Done')

    def get_key_sequence(self):
        self.send_command('thread key_sequence')
        i = self.pexpect.expect('(\d+)')
        if i == 0:
            key_sequence = int(self.pexpect.match.groups()[0])
        self.pexpect.expect('Done')
        return key_sequence

    def set_key_sequence(self, key_sequence):
        cmd = 'thread key_sequence %d' % key_sequence
        self.send_command(cmd)
        self.pexpect.expect('Done')

    def set_network_id_timeout(self, network_id_timeout):
        cmd = 'thread network_id_timeout %d' % network_id_timeout
        self.send_command(cmd)
        self.pexpect.expect('Done')

    def set_network_name(self, network_name):
        cmd = 'mac name ' + network_name
        self.send_command(cmd)
        self.pexpect.expect('Done')

    def get_panid(self):
        self.send_command('mac panid')
        i = self.pexpect.expect('([0-9a-fA-F]{16})')
        if i == 0:
            panid = self.pexpect.match.groups()[0]
        self.pexpect.expect('Done')

    def set_panid(self, panid):
        cmd = 'mac panid %d' % panid
        self.send_command(cmd)
        self.pexpect.expect('Done')

    def set_router_upgrade_threshold(self, threshold):
        cmd = 'thread router_upgrade_threshold %d' % threshold
        self.send_command(cmd)
        self.pexpect.expect('Done')

    def release_router_id(self, router_id):
        cmd = 'thread release_router %d' % router_id
        self.send_command(cmd)
        self.pexpect.expect('Done')

    def get_state(self):
        states = ['detached', 'child', 'router', 'leader']
        self.send_command('thread state')
        match = self.pexpect.expect(states)
        self.pexpect.expect('Done')
        return states[match]

    def set_state(self, state):
        cmd = 'thread state ' + state
        self.send_command(cmd)
        self.pexpect.expect('Done')

    def get_timeout(self):
        self.send_command('thread timeout')
        i = self.pexpect.expect('(\d+)')
        if i == 0:
            timeout = self.pexpect.match.groups()[0]
        self.pexpect.expect('Done')
        return timeout

    def set_timeout(self, timeout):
        cmd = 'thread timeout %d' % timeout
        self.send_command(cmd)
        self.pexpect.expect('Done')

    def get_weight(self):
        self.send_command('thread weight')
        i = self.pexpect.expect('(\d+)')
        if i == 0:
            weight = self.pexpect.match.groups()[0]
        self.pexpect.expect('Done')
        return weight

    def set_weight(self, weight):
        cmd = 'thread weight %d' % weight
        self.send_command(cmd)
        self.pexpect.expect('Done')

    def add_ipaddr(self, ipaddr):
        cmd = 'ip addr add ' + ipaddr + ' dev thread'
        self.send_command(cmd)
        self.pexpect.expect('Done')

    def get_addrs(self):
        addrs = []
        self.send_command('ifconfig')

        threadif = False
        while True:
            i = self.pexpect.expect(['(\S+)\:', '\s*inet6\s+(\S+)/64', 'Done'])
            if i == 0:
                if self.pexpect.match.groups()[0] == 'thread':
                    threadif = True
                else:
                    threadif = False
            elif i == 1:
                if threadif:
                    addrs.append(self.pexpect.match.groups()[0])
            elif i == 2:
                break
            elif i == 3:
                continue

        return addrs

    def get_cache(self):
        addrs = []
        self.send_command('thread cache')

        while True:
            i = self.pexpect.expect(['(\S+)\s+(\d+)\s+(\d+)\s+(\d+)', 'Total', 'Done'])
            if i == 0:
                addrs.append(self.pexpect.match.groups())
            elif i == 1:
                continue
            elif i == 2:
                break

        return addrs

    def get_context_reuse_delay(self):
        self.send_command('netdata context_reuse_delay')
        i = self.pexpect.expect('(\d+)')
        if i == 0:
            timeout = self.pexpect.match.groups()[0]
        self.pexpect.expect('Done')
        return timeout

    def set_context_reuse_delay(self, delay):
        cmd = 'netdata context_reuse_delay %d' % delay
        self.send_command(cmd)
        self.pexpect.expect('Done')

    def add_prefix(self, prefix, flags, prf = 'med'):
        cmd = 'netdata prefix add ' + prefix + ' ' + flags + ' ' + prf
        self.send_command(cmd)
        self.pexpect.expect('Done')

    def remove_prefix(self, prefix):
        cmd = 'netdata prefix remove ' + prefix
        self.send_command(cmd)
        self.pexpect.expect('Done')

    def add_route(self, prefix, prf = 'med'):
        cmd = 'netdata route add ' + prefix + ' ' + prf
        self.send_command(cmd)
        self.pexpect.expect('Done')

    def remove_route(self, prefix):
        cmd = 'netdata route remove ' + prefix
        self.send_command(cmd)
        self.pexpect.expect('Done')

    def register_netdata(self):
        self.send_command('netdata register')
        self.pexpect.expect('Done')

    def scan(self):
        self.send_command('mac scan')

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
        cmd = 'ping -I thread '
        if size != None:
            cmd += '-s ' + str(size) + ' '
        cmd += ipaddr

        self.send_command(cmd)
        responders = {}
        while len(responders) < num_responses:
            i = self.pexpect.expect(['from (\S+)%thread:'])
            if i == 0:
                responders[self.pexpect.match.groups()[0]] = 1
        self.pexpect.expect('\n')
        return responders

if __name__ == '__main__':
    unittest.main()
