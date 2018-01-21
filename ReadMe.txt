StdOut/StdErr Logging Utility
Created by LoRd_MuldeR <mulder2@gmx.de>
http://www.muldersoft.com/


Synopsis
========

Usage Mode #1:
  LoggingUtil.exe SomeProgram.exe [program parameters]
  LoggingUtil.exe [logging options] : SomeProgram.exe [program parameters]

Usage Mode #2:
  SomeProgram.exe [parameters] | LoggingUtil.exe [options] : #STDIN#
  SomeProgram.exe [parameters] 2>&1 | LoggingUtil.exe [options] : #STDIN#

Logging Options:
  --logfile <logfile>  Specifies the output log file (appends if file exists)
  --only-stdout        Capture only output from STDOUT, ignores STDERR
  --only-stderr        Capture only output from STDERR, ignores STDOUT
  --no-simplify        Do NOT simplify/trimm the logged strings (default: on)
  --no-append          Do NOT append, i.e. any existing log content is lost
  --plain-output       Create less verbose logging output
  --html-output        Create HTML logging output, implies NO append
  --regexp-keep <exp>  Keep ONLY strings that match the given RegExp
  --regexp-skip <exp>  Skip all the strings that match the given RegExp
  --codec-in <name>    Setup the input text encoding (default: "UTF-8")
  --codec-out <name>   Setup the output text encoding (default: "UTF-8")

Examples:
  LoggingUtil.exe --logfile x264_log.txt : x264.exe -o output.mkv input.avs
  x264.exe -o output.mkv input.avs 2>&1 | LoggingUtil.exe : #STDIN#

License
=======

Copyright (C) 2010-2013 LoRd_MuldeR <MuldeR2@GMX.de>
http://www.muldersoft.com/

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

http://www.gnu.org/licenses/gpl-2.0.txt
