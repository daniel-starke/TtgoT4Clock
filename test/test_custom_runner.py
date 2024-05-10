"""
@file test_custom_runner.py
@author Daniel Starke
@date 2024-04-26
@version 2024-05-05

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
import subprocess
import click

from platformio.exception import ReturnErrorCode
from platformio.public import UnityTestRunner

class CustomTestRunner(UnityTestRunner):
	def teardown(self):
		super().teardown()
		click.secho('Coverage Report...', bold = True)
		src_dir = self.project_config.get('platformio', 'src_dir')
		build_dir = self.project_config.get('platformio', 'build_dir')
		work_dir = self.project_config.get('platformio', 'workspace_dir')
		folder = os.path.join(work_dir, 'coverage')
		os.makedirs(folder, exist_ok = True)
		file = os.path.join(folder, 'index.html')
		res = subprocess.run(['gcovr', '--root', src_dir, '--no-color', '--decisions', '--html-details', file, build_dir])
		if res.returncode != 0:
			raise ReturnErrorCode(res.returncode)
