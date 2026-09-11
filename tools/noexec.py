#!/usr/bin/env python3
"""The game library opens no socket and starts no process.

WHY.  Exactly one outbound thing is allowed: `autolaunchbspc`,
which is a LIBVAR handed to the bot brain -- the brain launches BSPC, this
library does not, and the cvar that enables it is empty by default so the libvar
is never pushed.  Everything else is forbidden, and "forbidden" in a tree with
six donors in it has to mean "checked", because RA2 arrived with a UDP event
forwarder (`netlog`) that resolved a hostname, opened a socket and connected --
three functions deep inside `gslog.c`, each of which called `exit(1)` on
failure, from inside a game library, on a server that was otherwise healthy.

WHAT IS A FINDING.  Any call to a process-spawning or socket function, by name.
The list is what libc and winsock actually offer; a wrapper around one of them
is caught at the wrapper.

WHAT IS NOT.  `fopen`/`fread`/`fwrite` -- the stats writers, the config readers
and the round log are all local files and are not covered.
Comments and string literals are stripped first, so this file's own prose and
`osp_stats.h`'s description of the ngStatsQ2T uploader it does NOT do are not
findings.

USAGE
    tools/noexec.py [--tree src]
    tools/noexec.py --selftest
"""
import argparse
import os
import re
import shutil
import sys
import tempfile

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

BANNED = {
    # process
    'system': 'runs a shell command',
    'popen': 'runs a shell command',
    'fork': 'forks',
    'vfork': 'forks',
    'execl': 'replaces the process image', 'execlp': 'replaces the process image',
    'execle': 'replaces the process image', 'execv': 'replaces the process image',
    'execvp': 'replaces the process image', 'execvpe': 'replaces the process image',
    'execve': 'replaces the process image',
    'posix_spawn': 'starts a process', 'posix_spawnp': 'starts a process',
    'CreateProcessA': 'starts a process', 'CreateProcessW': 'starts a process',
    'ShellExecuteA': 'starts a process', 'ShellExecuteW': 'starts a process',
    'WinExec': 'starts a process',
    # network
    'socket': 'opens a socket', 'socketpair': 'opens a socket',
    'connect': 'connects a socket', 'bind': 'binds a socket',
    'listen': 'listens on a socket', 'accept': 'accepts a connection',
    'send': 'writes to a socket', 'sendto': 'writes to a socket',
    'sendmsg': 'writes to a socket', 'recv': 'reads from a socket',
    'recvfrom': 'reads from a socket', 'recvmsg': 'reads from a socket',
    'gethostbyname': 'resolves a host', 'getaddrinfo': 'resolves a host',
    'inet_addr': 'parses an address for a socket call',
    'inet_aton': 'parses an address for a socket call',
    'WSAStartup': 'initialises winsock',
    # and the one that turns any of the above into a dead server
    'exit': 'ends the SERVER process from inside the game library',
    '_exit': 'ends the SERVER process from inside the game library',
    'abort': 'ends the SERVER process from inside the game library',
}

CALL = re.compile(r'(?<![\w.>$])(' + '|'.join(sorted(BANNED, key=len, reverse=True)) + r')\s*\(')


def strip_comments(t):
    def blank(m):
        n = m.group(0).count('\n')
        return '\n' * n + ' ' * (len(m.group(0)) - n)
    t = re.sub(r'/\*.*?\*/', blank, t, flags=re.S)
    t = re.sub(r'//[^\n]*', lambda m: ' ' * len(m.group(0)), t)
    t = re.sub(r'"(?:\\.|[^"\\\n])*"', lambda m: ' ' * len(m.group(0)), t)
    t = re.sub(r"'(?:\\.|[^'\\\n])*'", lambda m: ' ' * len(m.group(0)), t)
    return t


def sources(tree):
    out = []
    for root, dirs, files in os.walk(tree):
        dirs[:] = [d for d in dirs if not d.startswith(('debug', 'release', '__'))]
        for f in sorted(files):
            if f.endswith(('.c', '.h')):
                out.append(os.path.join(root, f))
    return out


def scan(tree):
    hits = []
    for path in sources(tree):
        code = strip_comments(open(path, encoding='utf-8').read())
        for m in CALL.finditer(code):
            line = code.count('\n', 0, m.start()) + 1
            hits.append((os.path.relpath(path, REPO), line, m.group(1)))
    return hits


SELFTEST_CLEAN = {
    'ok.c': '/* system() and socket() are named here on purpose */\n'
            'void f(void) { FILE *fp = fopen("x", "r"); fclose(fp); }\n'
            'void g(void) { gi.dprintf("no exit() here\\n"); }\n',
    # a MEMBER named send, and a local variable, are not libc's
    'member.c': 'void h(net_t *n) { n->send(n); }\n',
}

SELFTEST_MUTANTS = [
    ('system()', 'void f(void) { system("ls"); }'),
    ('socket()', 'void f(void) { int s = socket(2, 2, 0); (void)s; }'),
    ('gethostbyname()', 'void f(char *n) { gethostbyname(n); }'),
    ('exit() from a game library', 'void f(void) { exit(1); }'),
    ('CreateProcessA()', 'void f(void) { CreateProcessA(0,0,0,0,0,0,0,0,0,0); }'),
]


def selftest():
    ok = True
    tmp = tempfile.mkdtemp(prefix='noexec-selftest-')
    try:
        for name, body in SELFTEST_CLEAN.items():
            open(os.path.join(tmp, name), 'w').write(body)
        base = scan(tmp)
        if base:
            print('!! selftest: the clean tree reported %s' % (base,))
            ok = False
        else:
            print('   [control] fopen, a member called send, prose: 0 findings')
        for why, body in SELFTEST_MUTANTS:
            p = os.path.join(tmp, 'bad.c')
            open(p, 'w').write(body + '\n')
            got = scan(tmp)
            os.remove(p)
            if not got:
                print('!! selftest: %s was NOT caught' % why)
                ok = False
            else:
                print('   [control] %s -> caught' % why)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    return ok


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--tree', default=os.path.join(REPO, 'src'))
    ap.add_argument('--selftest', action='store_true')
    a = ap.parse_args()
    if a.selftest:
        return 0 if selftest() else 1
    hits = scan(os.path.abspath(a.tree))
    for path, line, name in hits:
        print('!! %s:%d: %s() %s' % (path, line, name, BANNED[name]))
    print('noexec: %d process/network call(s)' % len(hits))
    return 1 if hits else 0


if __name__ == '__main__':
    sys.exit(main())
