#
#
#

MAX=`sysctl kern.sysv.shmmax | awk '{print $2}'`
echo "SHM MAX: $MAX bytes"

sudo sysctl -w kern.sysv.shmall=65536 kern.sysv.shmmax=16777216

