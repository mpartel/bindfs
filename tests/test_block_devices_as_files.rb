#!/usr/bin/env ruby
#
#   Copyright 2006,2007,2008,2009,2010 Martin Pärtel <martin.partel@gmail.com>
#
#   This file is part of bindfs.
#
#   bindfs is free software: you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation, either version 2 of the License, or
#   (at your option) any later version.
#
#   bindfs is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with bindfs.  If not, see <http://www.gnu.org/licenses/>.
#

# if we are being run by make check it will set srcdir and we should use it
$LOAD_PATH << (ENV['srcdir'] || '.')
require 'common.rb'

raise "Please run this as root" unless Process.uid == 0

if ARGV.include?("-h") || ARGV.include?("--help") || ARGV.length != 1
  puts
  puts "Usage: #{$0} /dev/some-block-device"
  puts
  puts "This test will attempt to read the block device through bindfs."
  puts "This is a separate test file because it's nontrivial to set up a"
  puts "block device for testing in a non-intrusive and portable way."
  puts
  exit(1)
end

device = ARGV[0]

root_testenv("--block-devices-as-files --resolve-symlinks") do
  symlink(device, 'src/devicelink')

  size_through_bindfs = File.size('mnt/devicelink')
  measured_size = File.open(device, "r") do |f|
    f.seek(0, IO::SEEK_END)
    f.tell
  end
  assert { size_through_bindfs == measured_size }
  assert { measured_size >= 512 }
  puts "Device size: #{size_through_bindfs}"

  data_read_through_bindfs = File.read('mnt/devicelink', 512)
  data_read_directly= File.read(device, 512)
  assert { data_read_through_bindfs == data_read_directly }
end
