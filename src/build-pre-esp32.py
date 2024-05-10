"""
@file build-pre-esp32.py
@author Daniel Starke
@date 2024-04-20
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

import configparser
import re

def fromString(val):
	"""! Convert INI string value to escaped string.
	@param val - string value to convert
	@return escaped string value
	"""
	val = val.strip()
	if val.startswith('"') and val.endswith('"'):
		val = val[1:-1]
	elif val.startswith("'") and val.endswith("'"):
		val = val[1:-1]
	return val.replace('"', '\\"')

if __name__ == 'SCons.Script':
	Import('env')

	config = configparser.ConfigParser(
		delimiters = ('='),
		comment_prefixes = ('#'),
		inline_comment_prefixes = ('#')
	)
	config.optionxform = str
	config.read('data/config.ini')

	# validate configuration file
	assert len(fromString(config['WIFI']['SSID'])) > 0
	assert len(fromString(config['WIFI']['SSID'])) <= 255
	assert len(fromString(config['WIFI']['PASS'])) > 0
	assert len(fromString(config['WIFI']['PASS'])) <= 255
	assert re.match(r'^[a-zA-Z]([0-9a-zA-Z-]{0,61}[0-9a-zA-Z])?$', fromString(config['MDNS']['HOST']), re.ASCII)
	assert len(fromString(config['OTA']['PASS'])) > 0
	assert len(fromString(config['OTA']['PASS'])) <= 255
	assert int(config['NTP']['TIMEOUT'], 0) > 0
	assert int(config['NTP']['TIMEOUT'], 0) <= 65535
	assert len(fromString(config['NTP']['SERVER'])) <= 255
	assert re.match(r'^(([a-zA-Z]([0-9a-zA-Z-]{0,61}[0-9a-zA-Z])?(\.[a-zA-Z]([0-9a-zA-Z-]{0,61}[0-9a-zA-Z])?)*)|(((25[0-5]|(2[0-4]|1\d|[1-9]|)\d)\.?\b){4}))$', fromString(config['NTP']['SERVER']), re.ASCII)
	assert int(config['CLOCK']['PASS_COLOR'], 0) > 0
	assert int(config['CLOCK']['PASS_COLOR'], 0) <= 65535
	assert int(config['CLOCK']['PASS_COLOR'], 0) > 0
	assert int(config['CLOCK']['PASS_COLOR'], 0) <= 65535
	assert re.match(r'^([01][0-9]|2[0-3]):([0-5][0-9]|60)$', fromString(config['CLOCK']['PASS_FROM']), re.ASCII)
	assert re.match(r'^([01][0-9]|2[0-3]):([0-5][0-9]|60)$', fromString(config['CLOCK']['PASS_TO']), re.ASCII)

	# set OTA parameters from config
	if env['PIOENV'] == 'ttgo-t4-v13-ota':
		env.Replace(UPLOAD_PORT = fromString(config['MDNS']['HOST']))
		env.Replace(UPLOAD_FLAGS = [
			'--progress',
			'--host_port=3242',
			'--auth=' + fromString(config['OTA']['PASS'])
		])
