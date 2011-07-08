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
require 'common.rb'

raise "Please run this as root" unless Process.uid == 0

raise "Give two users as parameters" unless ARGV.length == 2

user1 = ARGV[0]
user2 = ARGV[1]

raise "Give two _different_ users as parameters" unless user1 != user2

testenv "--mirror=#{user1},#{user2}" do
    touch('src/file')
    
    count = 0
    10.times do |i|
        out1 = `su #{user1} -c "stat --format=%U mnt/file"`
        out2 = `su #{user2} -c "stat --format=%U mnt/file"`
        
        out1.strip!
        out2.strip!
        puts "#{i+1}: #{out1} #{out2}"
        raise "FAIL: #{user1} saw #{out1} on iter #{i}" unless out1 == user1
        raise "FAIL: #{user2} saw #{out2} on iter #{i}" unless out2 == user2
    end
end
