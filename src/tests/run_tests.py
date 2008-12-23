#!/usr/bin/env python
import os
os.environ['PATH'] = ':'.join(['.', os.environ['PATH']])

tests = ['wtest',
         'rtest > rtest.dat && cmp rtest.dat rtestok.dat',
         'ftest > ftest.dat && cmp ftest.dat ftestok.dat',
         'halftest']


failed = 0
for test in tests:
    print 'Running:', test
    status = os.system(test)
    if status != 0:
        print 'FAILED'
        failed += 1
    else:
        print 'Passed'
    print

print 'Finished', len(tests), 'tests,'
if failed == 0:
    print 'All tests passed'
else:
    print failed, 'tests FAILED'
    exit(1)
