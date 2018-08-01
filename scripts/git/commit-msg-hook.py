#!/usr/bin/python
# -*- coding: utf-8 -*-

import sys


#   format: \033[type;fg;bgm
#
#     fg                bg              color
#   -------------------------------------------
#     30                40              black
#     31                41              red
#     32                42              green
#     33                43              yellow
#     34                44              blue
#     35                45              purple
#     36                46              cyan
#     37                47              white
#
#     type
#   -------------------------
#      0               normal
#      1               bold
#      4               underline
#      5               blink
#      7               invert
#      8               hide
#
#   examples:
#   \033[1;31;40m    <!--1-bold 31-red fg 40-black bg-->
#   \033[0m          <!--back to normal-->


STYLE = {
        'fore':
        {
            'black'    : 30,
            'red'      : 31,
            'green'    : 32,
            'yellow'   : 33,
            'blue'     : 34,
            'purple'   : 35,
            'cyan'     : 36,
            'white'    : 37,
        },

        'back':
        {
            'black'     : 40,
            'red'       : 41,
            'green'     : 42,
            'yellow'    : 43,
            'blue'      : 44,
            'purple'    : 45,
            'cyan'      : 46,
            'white'     : 47,
        },

        'mode':
        {
            'normal'    : 0,
            'bold'      : 1,
            'underline' : 4,
            'blink'     : 5,
            'invert'    : 7,
            'hide'      : 8,
        },

        'default':
        {
            'end': 0,
        },
}


def style(string, mode='', fore='', back=''):

    mode = '%s' % STYLE['mode'][mode] if STYLE['mode'].has_key(mode) else ''

    fore = '%s' % STYLE['fore'][fore] if STYLE['fore'].has_key(fore) else ''

    back = '%s' % STYLE['back'][back] if STYLE['back'].has_key(back) else ''

    style = ';'.join([s for s in [mode, fore, back] if s])

    style = '\033[%sm' % style if style else ''

    end = '\033[%sm' % STYLE['default']['end'] if style else ''

    return '%s%s%s' % (style, string, end)


def check_subject(subject_line):
    types = ['Feat', 'Fix', 'Refactor', 'Style', 'Docs', 'Test', 'Chore']

    if subject_line.startswith(' '):
        print style('Error: Subject line starts with whitespace\n', fore='red')
        return 1

    if len(subject_line) > 50:
        print style('Error: Subject line should be limited to 50 chars\n', fore='red')
        return 1

    ll = subject_line.split(':')
    if len(ll) < 2:
        print style('Error: Subject line should have a type\n', fore='red')
        return 1

    type = ll[0]
    if type not in types:
        print style('Error: Subject line starts with unknown type\n', fore='red')
        return 1

    return 0


contents = []
ret = 0
subject = True

with open(sys.argv[1], 'r') as commit_msg:
    contents = commit_msg.readlines()

for line in contents:
    dup = line.lstrip()
    if dup.startswith('#') or dup.startswith("Change-Id") or dup.startswith("Signed-of-by"):
        continue
    if subject is True:
        ret = check_subject(line)
        subject = False
    else:
        if len(line) > 72:
            print style('Error: Body line should be limited to 72 chars\n', fore='red')
            ret = 1

exit(ret)
