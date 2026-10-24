import json
import os
import sys
import time

mode = sys.argv[1]
if mode == 'echo':
    sys.stderr.write('diagnostic:' + 'x' * 70000)
    sys.stderr.flush()
    print(json.dumps({'argv': sys.argv[2:], 'secret': os.getenv('NV_TEST_PRIVATE'), 'safe': os.getenv('NV_TEST_ALLOWED')}), flush=True)
    for line in sys.stdin:
        print(line.rstrip('\n'), flush=True)
elif mode == 'sleep':
    time.sleep(10)
elif mode == 'huge':
    sys.stdout.write('x' * 2000000)
    sys.stdout.flush()
elif mode == 'closed':
    os.close(0)
    print('ready', flush=True)
    time.sleep(10)
elif mode == 'fds':
    path = '/proc/self/fd/' + sys.argv[2]
    print(os.readlink(path) if os.path.exists(path) else 'closed', flush=True)
