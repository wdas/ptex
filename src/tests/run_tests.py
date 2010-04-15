#!/usr/bin/env python
import os
os.environ['PATH'] = ':'.join(['.', os.environ['PATH']])

def CompareFiles(a, b):
    'Compare files using Universal Newline Support for portability'
    return open(a, 'rU').readlines() != open(b, 'rU').readlines()

tests = ['wtest',
         ('rtest', 'rtest.dat', 'rtestok.dat'),
         ('ftest', 'ftest.dat', 'ftestok.dat'),
         'halftest']

failed = 0
for test in tests:
    if type(test) is tuple:
        cmd, output, ref = test
        cmd = cmd + ' > ' + output
    else:
        cmd = test
        output = ref = None
    print 'Running:', cmd
    status = os.system(cmd)
    if status == 0 and output and ref:
        print 'Comparing:', output, ref
        status = CompareFiles(output, ref)
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
