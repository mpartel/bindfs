#!/usr/bin/env ruby

# if we are being run by make check it will set srcdir and we should use it
$LOAD_PATH << (ENV['srcdir'] || '.')
require 'common.rb'
require 'shellwords'

if ARGV.length == 0 || ['-h', '--help'].include?(ARGV[0])
  puts "Usage: stress_test.rb workdir"
  puts
  exit!(if ARGV.length == 0 then 1 else 0 end)
end

WORK_DIR=ARGV[0]
SRC_DIR="#{WORK_DIR}/src"
MNT_DIR="#{WORK_DIR}/mnt"

module INFINITY
  def self.times(&block)
    while true
      block.call
    end
  end
end

class ChildProcess
  def start
    @pid = Process.fork do
      run
    end
  end

  attr_reader :pid

  def run
    raise "undefined"
  end

  def interrupt
    ensure_started
    Process.kill("INT", @pid)
  end

  def wait
    ensure_started
    Process.waitpid @pid
    @pid = nil
    $?
  end

  def ensure_started
    raise "Not started" if !@pid
  end
end

class BindfsRunner < ChildProcess
  def initialize(options = {})
    @valgrind = options[:valgrind]
    @valgrind_tool = options[:valgrind_tool] || 'memcheck'

    if @valgrind && Process.uid != 0
      raise "This must be run as root due to valgrind's limitations"
    end
  end

  def run
    cmd = [
      EXECUTABLE_PATH,
      '-f',
      SRC_DIR,
      MNT_DIR
    ]

    if @valgrind
      cmd = [
        'valgrind',
        '--tool=' + @valgrind_tool,
        '--trace-children=yes',
        '--trace-children-skip=/bin/mount,/bin/umount',
        '--'
      ] + cmd
    end

    cmd_str = Shellwords.join(cmd) + ' > stress_test.log 2>&1'
    puts "Starting #{cmd_str}"
    Process.exec(cmd_str)
  end
end

class FileCopier < ChildProcess
  def initialize(options = {})
    @options = {
      :prefix => "file",
      :num_copies => 2,
      :size => "100M",
      :iterations => 10
    }.merge(options)

    puts "Creating template file, size = #{@options[:size]}"
    src_dir_src_file = "#{SRC_DIR}/#{@options[:prefix]}_src"
    output = `dd if=/dev/urandom of='#{src_dir_src_file}' bs=#{@options[:size]} count=1 2>&1`
    raise "#{$?.inspect} with output:\n#{output}" unless $?.success?
  end

  def mnt_prefix
    "#{MNT_DIR}/#{@options[:prefix]}"
  end

  def src_file
    "#{mnt_prefix}_src"
  end

  def run
    task_desc = "#{@options[:num_copies]} copies of #{@options[:size]} in #{@options[:iterations]} iterations"
    puts "Making #{task_desc}"
    @options[:iterations].times do
      dest_files = (1...@options[:num_copies]).map {|i| "#{mnt_prefix}#{i}"}
      for dest_file in dest_files
        FileUtils.cp(src_file, dest_file)
      end
      for dest_file in dest_files
        FileUtils.rm(dest_file)
      end
    end
    puts "Finished making #{task_desc}"
  end
end

FileUtils.rm_rf SRC_DIR
FileUtils.rm_rf MNT_DIR
FileUtils.mkdir_p SRC_DIR
FileUtils.mkdir_p MNT_DIR

bindfs_runner = BindfsRunner.new
bindfs_runner.start

copiers = [
  FileCopier.new(:prefix => "big", :num_copies => 4, :size => "500M", :iterations => 100),
  FileCopier.new(:prefix => "small", :num_copies => 10000, :size => "1k", :iterations => 100)
]

copiers.each(&:start)
copiers.each(&:wait)

bindfs_runner.interrupt
bindfs_runner.wait

