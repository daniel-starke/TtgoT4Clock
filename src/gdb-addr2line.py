"""
@file gdb-addr2line.py
@author Daniel Starke
@date 2024-05-09
@version 2024-05-10

Copyright (c) 2024 Daniel Starke

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
"""

import os
import re
import argparse
import subprocess
from platformio.package.manager.tool import ToolPackageManager
from platformio.package.commands.exec import inject_pkg_to_environ

def app():
	parser = argparse.ArgumentParser(prog = 'gdb-addr2line', description = 'Convert ESP32 memory addresses to lines using GDB.')
	parser.add_argument('-e', '--exec', help = 'Set the input ELF file.')
	parser.add_argument('addresses', nargs = '+', help = 'List of ESP32 memory addresses.')
	args = parser.parse_args()
	pm = ToolPackageManager()
	pkg = pm.get_package('toolchain-xtensa-esp32')
	assert pkg
	inject_pkg_to_environ(pkg)
	pattern = re.compile(r'0x(?:(?!0x)[0-9a-f])+', re.ASCII | re.IGNORECASE)
	gdbArgs = ['xtensa-esp32-elf-gdb', '--nx', '--batch', args.exec, '-ex', 'set listsize 1']
	for addr in re.findall(pattern, ' '.join(args.addresses)):
		gdbArgs.extend(['-ex', f'l *{addr}'])
	subprocess.run(gdbArgs + ['-ex', 'q'], env = os.environ, check = False)

if __name__ == '__main__':
	app()
