#!/usr/bin/env ruby
#
#   Copyright 2006,2007,2008,2009,2010,2012 Martin Pärtel <martin.partel@gmail.com>
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

require 'fileutils'
include FileUtils

# Set the default umask for all tests
File.umask 0022

EXECUTABLE_PATH = '../src/bindfs'
TESTDIR_NAME = 'tmp_test_bindfs'

# If set to an array of test names, only those will be run
$only_these_tests = nil

def fail(msg, error = nil, options = {})
    options = {:exit => false}.merge(options)
    $stderr.puts(msg)
    if error.is_a?(Exception)
        $stderr.puts(error.message + "\n  " + error.backtrace.join("\n  "))
    elsif error != nil
        $stderr.puts(error.to_s)
    end
    exit! 1 if options[:exit]
end

def fail!(msg, error = nil, options = {})
    options = {:exit => true}.merge(options)
    fail(msg, error, options)
end

def wait_for(options = {}, &condition)
    options = {
        :initial_sleep => 0.01,
        :sleep_ramp_up => 2,
        :max_sleep => 0.5,
        :max_time => 5
    }.merge(options)
    
    start_time = Time.now
    sleep_time = options[:initial_sleep]
    while !`mount`.include?(`pwd`.strip)
        sleep sleep_time
        sleep_time = [sleep_time * options[:sleep_ramp_up], options[:max_sleep]].min
        if Time.now - start_time > options[:max_time]
            return false
        end
    end
    true
end

def valgrind_options
    opt = ARGV.find {|s| s.start_with?('--valgrind') }
    if opt == nil
        nil
    elsif opt =~ /^--valgrind=(.*)$/
        $1
    else
        ''
    end
end

# Prepares a test environment with a mounted directory
def testenv(bindfs_args, options = {}, &block)

    options = {
        :title => bindfs_args,
        :valgrind => valgrind_options != nil,
        :valgrind_opts => valgrind_options
    }.merge(options)
  
    # todo: less repetitive and more careful error handling and cleanup

    puts "--- #{options[:title]} ---"
    puts "[  #{bindfs_args}  ]"
    
    begin
        Dir.mkdir TESTDIR_NAME
    rescue Exception => ex
        fail!("ERROR creating testdir at #{TESTDIR_NAME}", ex)
    end

    begin
        Dir.chdir TESTDIR_NAME
        Dir.mkdir 'src'
        Dir.mkdir 'mnt'
    rescue Exception => ex
        fail!("ERROR preparing testdir at #{TESTDIR_NAME}", ex)
    end

    bindfs_pid = nil
    begin
        cmd = "../#{EXECUTABLE_PATH} #{bindfs_args} -f src mnt"
        if options[:valgrind]
            cmd = "valgrind #{options[:valgrind_opts]} #{cmd}"
        end
        bindfs_pid = Process.fork do
            exec cmd
            exit! 127
        end
    rescue Exception => ex
        fail!("ERROR running bindfs", ex)
    end

    # Wait for bindfs to be ready.
    if !wait_for { `mount`.include?(Dir.pwd) }
        fail!("ERROR: Mount point did not appear in `mount`")
    end

    testcase_ok = true
    begin
        yield
    rescue Exception => ex
        fail("ERROR: testcase `#{options[:title]}' failed", ex)
        testcase_ok = false
    end

    begin
        unless system(umount_cmd + ' mnt')
            raise Exception.new(umount_cmd + " failed with status #{$?}")
        end
        Process.wait bindfs_pid
    rescue Exception => ex
        fail("ERROR: failed to umount")
        testcase_ok = false
    end
    
    if File.exist?("bindfs.log")
        system("cat bindfs.log")
    end

    begin
        Dir.chdir '..'
    rescue Exception => ex
        fail!("ERROR: failed to exit test env")
    end

    unless system "rm -Rf #{TESTDIR_NAME}"
        fail!("ERROR: failed to clear test directory")
    end

    if testcase_ok
        puts "OK"
    else
        exit! 1
    end
end

# Like testenv but skips the test if not running as root
def root_testenv(bindfs_args, options = {}, &block)
    if Process.uid == 0
        testenv(bindfs_args, options, &block)
    else
        puts "--- #{bindfs_args} ---"
        puts "[  #{bindfs_args}  ]"
        puts "SKIP (requires root)"
    end
end

def umount_cmd
    if `which fusermount`.strip.empty?
    then 'umount'
    else 'fusermount -uz'
    end
end

def assert
    raise Exception.new('test failed') unless yield
end

def assert_exception(ex)
    begin
        yield
    rescue ex
        return
    end
    raise Exception.new('expected exception ' + ex.to_s)
end


