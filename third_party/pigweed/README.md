
git clone https://pigweed.googlesource.com/pigweed/pigweed repo

$ cd third_party/pigweed/repo
$ source ./bootstrap.sh
$ cd ../../../
$ ./script/cmake-build posix
$ ./script/cmake-build simulation
$ ./build/posix/src/posix/ot-cli-rpc 'spinel+hdlc+forkpty://build/simulation/examples/apps/ncp/ot-rcp-server?forkpty-arg=1'

$ vim /var/log/syslog
