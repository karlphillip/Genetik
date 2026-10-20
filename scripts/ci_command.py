#!/usr/bin/env python3
"""Run a CI command, retain its log and expose bounded diagnostics as an annotation."""
import argparse
from collections import deque
from pathlib import Path
import subprocess
import sys


def escape(text):
    return text.replace('%', '%25').replace('\r', '%0D').replace('\n', '%0A')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--log', required=True, type=Path)
    parser.add_argument('--title', required=True)
    parser.add_argument('--report', action='store_true', help='also annotate successful output')
    parser.add_argument('command', nargs=argparse.REMAINDER)
    args = parser.parse_args()
    command = args.command[1:] if args.command[:1] == ['--'] else args.command
    if not command:
        parser.error('a command is required')
    args.log.parent.mkdir(parents=True, exist_ok=True)
    tail = deque(maxlen=100)
    with args.log.open('w') as log:
        with subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                              text=True, errors='replace') as process:
            for line in process.stdout:
                log.write(line)
                print(line, end='', flush=True)
                tail.append(line)
            status = process.wait()
    if status or args.report:
        message = ''.join(tail)[-24000:]
        kind = 'error' if status else 'notice'
        print(f'::{kind} title={escape(args.title)}::{escape(message)}', flush=True)
    return status if status >= 0 else 128-status


if __name__ == '__main__':
    sys.exit(main())
