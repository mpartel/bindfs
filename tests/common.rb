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

require 'ruby18_hacks.rb'
require 'shellwords' unless $ruby_18_hacks_enabled  # Ruby 1.8 doesn't have shellwords
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

def with_umask(umask, &block)
    old_umask = File.umask(umask)
    begin
        block.call
    ensure
        File.umask(old_umask)
    end
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
        :valgrind_opts => valgrind_options,
        :srcdir_name => 'src',
        :mntdir_name => 'mnt'
    }.merge(options)

    # todo: less repetitive and more careful error handling and cleanup

    puts "--- #{options[:title]} ---"
    puts "[  #{bindfs_args}  ]"

    srcdir = options[:srcdir_name]
    mntdir = options[:mntdir_name]

    begin
        FileUtils.mkdir_p TESTDIR_NAME
    rescue Exception => ex
        fail!("ERROR creating testdir at #{TESTDIR_NAME}", ex)
    end

    begin
        Dir.chdir TESTDIR_NAME
        FileUtils.mkdir_p srcdir
        FileUtils.mkdir_p mntdir
    rescue Exception => ex
        fail!("ERROR preparing testdir at #{TESTDIR_NAME}", ex)
    end

    bindfs_pid = nil
    begin
        extra_args = "-f"
        # Don't rely on user_allow_other in /etc/fuse.conf.
        # On FreeBSD it isn't even possible to set that.
        extra_args += " --no-allow-other" if Process.uid != 0

        cmd = "../#{EXECUTABLE_PATH} #{bindfs_args} #{extra_args} #{Shellwords.escape(srcdir)} #{Shellwords.escape(mntdir)}"
        if options[:valgrind]
            cmd = "valgrind --error-exitcode=100 #{options[:valgrind_opts]} #{cmd}"
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
        block.call(bindfs_pid)
    rescue Exception => ex
        fail("ERROR: testcase `#{options[:title]}' failed", ex)
        testcase_ok = false
    end

    if File.exist?("bindfs.log")
        system("cat bindfs.log")
    end

    begin
        unless system(umount_cmd + ' ' + Shellwords.escape(mntdir))
            raise Exception.new(umount_cmd + " failed with status #{$?}")
        end
        Process.wait bindfs_pid
    rescue Exception => ex
        fail("ERROR: failed to umount")
        testcase_ok = false
    end

    if !$?.success?
        fail("exit status: #{$?}")
        testcase_ok = false
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

# Like testenv but skips the test if not running as non-root.
# TODO: make all tests runnable as root
def nonroot_testenv(bindfs_args, options = {}, &block)
    if Process.uid != 0
        testenv(bindfs_args, options, &block)
    else
        puts "--- #{bindfs_args} ---"
        puts "[  #{bindfs_args}  ]"
        puts "SKIP (requires running as non-root)"
    end
end

def umount_cmd
    if !`which fusermount3`.strip.empty?
        'fusermount3 -uz'
    elsif !`which fusermount`.strip.empty?
        'fusermount -uz'
    else
        'umount'
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


