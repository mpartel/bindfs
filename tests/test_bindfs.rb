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

require './common.rb'

include Errno

# FileUtils.chown turned out to be quite buggy in Ruby 1.8.7,
# so we'll use File.chown instead.
def chown(user, group, list)
    user = Etc.getpwnam(user).uid if user.is_a? String
    group = Etc.getgrnam(group).gid if group.is_a? String

    list = [list] unless list.is_a? Array
    for file in list
        File.chown(user, group, file)
    end
end

# Some useful shorthands
$nobody_uid = nobody_uid = Etc.getpwnam('nobody').uid
$nogroup_gid = nogroup_gid = Etc.getgrnam('nogroup').gid

$tests_dir = File.dirname(File.realpath(__FILE__))


testenv("") do
    assert { File.basename(pwd) == TESTDIR_NAME }
end

testenv("-u nobody -g nogroup") do
    touch('src/file')

    assert { File.stat('mnt/file').uid == nobody_uid }
    assert { File.stat('mnt/file').gid == nogroup_gid }
end

testenv("-p 0600:u+D") do
    touch('src/file')
    chmod(0777, 'src/file')

    assert { File.stat('mnt/file').mode & 0777 == 0600 }
end

testenv("--chmod-deny") do
    touch('src/file')

    assert_exception(EPERM) { chmod(0777, 'mnt/file') }
end

testenv("-u nobody -m #{Process.uid} -p 0600,u+D") do
    touch('src/file')

    assert { File.stat('mnt/file').uid == Process.uid }
end

root_testenv("", :title => "--create-as-user should be default for root") do
  chmod(0777, 'src')
  `su -c 'touch mnt/file' nobody`
  `su -c 'mkdir mnt/dir' nobody`
  `su -c 'ln -sf /tmp/foo mnt/lnk' nobody`
  
  assert { File.stat('mnt/file').uid == nobody_uid }
  assert { File.stat('mnt/file').gid == nogroup_gid }
  assert { File.stat('src/file').uid == nobody_uid }
  assert { File.stat('src/file').gid == nogroup_gid }
  
  assert { File.stat('mnt/dir').uid == nobody_uid }
  assert { File.stat('mnt/dir').gid == nogroup_gid }
  assert { File.stat('src/dir').uid == nobody_uid }
  assert { File.stat('src/dir').gid == nogroup_gid }
  
  assert { File.lstat('mnt/lnk').uid == nobody_uid }
  assert { File.lstat('mnt/lnk').gid == nogroup_gid }
  assert { File.lstat('src/lnk').uid == nobody_uid }
  assert { File.lstat('src/lnk').gid == nogroup_gid }
end

testenv("--create-with-perms=og=r:ogd+x") do
    touch('src/file')
    mkdir('src/dir')

    assert { File.stat('mnt/file').mode & 0077 == 0044 }
    assert { File.stat('mnt/dir').mode & 0077 == 0055 }
end

testenv("-p 0777 --realistic-permissions", :title => '--realistic-permissions') do
    touch('src/noexecfile')
    touch('src/execfile')
    chmod(0600, 'src/noexecfile')
    chmod(0700, 'src/execfile')
    
    assert { File.stat('mnt/noexecfile').mode & 0777 == 0666 }
    assert { File.stat('mnt/execfile').mode & 0777 == 0777 }
end

testenv("-p 0777", :title => '--realistic-permissions not the default') do
    touch('src/noexecfile')
    chmod(0600, 'src/noexecfile')
    
    assert { File.stat('mnt/noexecfile').mode & 0777 == 0777 }
end

testenv("--ctime-from-mtime") do
    sf = 'src/file'
    mf = 'mnt/file'
    
    touch(sf)
    sleep(1.1)
    chmod(0777, mf)
    
    # to_i gives us prceision of 1 sec
    assert { File.stat(mf).ctime.to_i == File.stat(mf).mtime.to_i }
    assert { File.stat(sf).ctime > File.stat(sf).mtime }
end

testenv("--hide-hard-links") do
  touch('src/one')
  ln('src/one', 'src/two')
  
  assert { File.stat('src/one').nlink == 2 }
  assert { File.stat('mnt/one').nlink == 1 }
end

# Define expectation for changing [uid, gid, both]
# for each combination of chown/chgrp flags.
chown_chgrp_test_cases = {
    :chown_normal => {
        :chgrp_normal => [:uid, :gid, :both],
        :chgrp_ignore => [:uid, nil, :uid],
        :chgrp_deny => [:uid, EPERM, EPERM]
    },
    :chown_ignore => {
        :chgrp_normal => [nil, :gid, :gid],
        :chgrp_ignore => [nil, nil, nil],
        :chgrp_deny => [nil, EPERM, EPERM]
    },
    :chown_deny => {
        :chgrp_normal => [EPERM, :gid, EPERM],
        :chgrp_ignore => [EPERM, nil, EPERM],
        :chgrp_deny => [EPERM, EPERM, EPERM]
    }
}

def run_chown_chgrp_test_case(chown_flag, chgrp_flag, expectations)
    flags = [chown_flag, chgrp_flag].map do |flag|
        '--' + flag.to_s.sub('_', '-')
    end.join ' '

    srcfile = 'src/file'
    mntfile = 'mnt/file'
    tests = [
        lambda { chown('nobody', nil, mntfile) },
        lambda { chown(nil, 'nogroup', mntfile) },
        lambda { chown('nobody', 'nogroup', mntfile) }
    ]

    for testcase, expect in tests.zip expectations
        root_testenv(flags) do
            touch(srcfile)
            if expect.respond_to? :exception
                assert_exception(expect) { testcase.call }
            else
                testcase.call
                uid = File.stat(srcfile).uid
                gid = File.stat(srcfile).gid
                
                case expect                
                when :uid
                    assert { uid == $nobody_uid }
                    assert { gid != $nogroup_gid }
                when :gid
                    assert { uid != $nobody_uid }
                    assert { gid == $nogroup_gid }
                when :both
                    assert { uid == $nobody_uid }
                    assert { gid == $nogroup_gid }
                when nil
                    assert { uid != $nobody_uid }
                    assert { gid != $nogroup_gid }
                end
            end
        end
    end
end

chown_chgrp_test_cases.each do |chown_flag, more|
    more.each do |chgrp_flag, expectations|
        run_chown_chgrp_test_case(chown_flag, chgrp_flag, expectations)
    end
end

root_testenv("--chown-deny") do
    touch('src/file')
    
    assert_exception(EPERM) { chown('nobody', nil, 'mnt/file') }
    assert_exception(EPERM) { chown('nobody', 'nogroup', 'mnt/file') }
    chown(nil, 'nogroup', 'mnt/file')
end

root_testenv("--mirror=root") do
    touch('src/file')
    chown('nobody', 'nogroup', 'src/file')

    assert { File.stat('mnt/file').uid == 0 }
    assert { File.stat('mnt/file').gid == $nogroup_gid }
end

testenv("--chmod-allow-x --chmod-ignore") do 
    touch('src/file')

    chmod(01700, 'src/file') # sticky bit set

    chmod(00077, 'mnt/file') # should change x bits; should not unset sticky bit
    assert { File.stat('src/file').mode & 07777 == 01611 }


    mkdir('src/dir')
    chmod(0700, 'src/dir')
    chmod(0077, 'mnt/dir') # bits on dir should not change
    assert { File.stat('src/dir').mode & 0777 == 0700 }
end

testenv("--chmod-deny --chmod-allow-x") do 
    touch('src/file')

    chmod(0700, 'src/file')

    chmod(0700, 'mnt/file') # no-op chmod should work

    assert_exception(EPERM) { chmod(0777, 'mnt/file') }
    assert_exception(EPERM) { chmod(0000, 'mnt/file') }
    assert_exception(EPERM) { chmod(01700, 'mnt/file') } # sticky bit

    chmod(0611, 'mnt/file') # chmod that only changes x-bits should work
    assert { File.stat('src/file').mode & 07777 == 00611 }


    mkdir('src/dir')
    chmod(0700, 'src/dir')
    assert_exception(EPERM) { chmod(0700, 'mnt/dir') } # chmod on dir should not work
end

testenv("--chmod-perms=g-w,o-rwx") do
    touch('src/file')

    chmod(0666, 'mnt/file')

    assert { File.stat('src/file').mode & 0777 == 0640 }
end

root_testenv("--map=nobody/root:@nogroup/@root") do
    touch('src/file')
    chown('nobody', 'nogroup', 'src/file')
    
    assert { File.stat('mnt/file').uid == 0 }
    assert { File.stat('mnt/file').gid == 0 }
    
    touch('mnt/newfile')
    mkdir('mnt/newdir')
    
    assert { File.stat('src/newfile').uid == $nobody_uid }
    assert { File.stat('src/newfile').gid == $nogroup_gid }
    assert { File.stat('src/newdir').uid == $nobody_uid }
    assert { File.stat('src/newdir').gid == $nogroup_gid }
    
    assert { File.stat('mnt/newfile').uid == 0 }
    assert { File.stat('mnt/newfile').gid == 0 }
    assert { File.stat('mnt/newdir').uid == 0 }
    assert { File.stat('mnt/newdir').gid == 0 }
end

root_testenv("--map=@nogroup/@root") do
    touch('src/file')
    chown('nobody', 'nogroup', 'src/file')
    
    assert { File.stat('mnt/file').gid == 0 }
end

root_testenv("--map=1/2") do
    touch('src/file1')
    touch('src/file2')
    chown(1, nil, 'src/file1')
    chown(3, nil, 'src/file2')
    
    assert { File.stat('mnt/file1').uid == 2 }
    assert { File.stat('mnt/file2').uid == 3 }
end

root_testenv("--map=@1/@2:@2/@1") do
    touch('src/file1')
    touch('src/file2')
    chown(nil, 1, 'src/file1')
    chown(nil, 2, 'src/file2')

    assert { File.stat('mnt/file1').gid == 2 }
    assert { File.stat('mnt/file2').gid == 1 }
end

root_testenv("-u 1 --map=1/2:3/4") do
    touch('src/file1')
    touch('src/file2')
    chown(2, nil, 'src/file1')
    
    assert { File.stat('mnt/file1').uid == 1 }
    assert { File.stat('mnt/file2').uid == 1 }
end

root_testenv("--map=0/1:@0/@1", :title => "--map and chown/chgrp") do
    touch('src/file1')
    chown(2, 2, 'src/file1')
    assert { File.stat('mnt/file1').uid == 2 }
    assert { File.stat('mnt/file1').gid == 2 }
    
    chown(1, 1, 'mnt/file1')
    
    assert { File.stat('src/file1').uid == 0 }
    assert { File.stat('src/file1').gid == 0 }
    assert { File.stat('mnt/file1').uid == 1 }
    assert { File.stat('mnt/file1').gid == 1 }
end

testenv("", :title => "preserves inode numbers") do
    touch('src/file')
    mkdir('src/dir')
    assert { File.stat('mnt/file').ino == File.stat('src/file').ino }
    assert { File.stat('mnt/dir').ino == File.stat('src/dir').ino }
end

testenv("", :title => "has readdir inode numbers") do
    touch('src/file')
    mkdir('src/dir')
    
    inodes = {}
    for line in `#{$tests_dir}/readdir_inode mnt`.split("\n").reject(&:empty?)
        inode, name = line.split(" ")
        inodes[name] = inode.to_i
    end
    
    assert { inodes['file'] == File.stat('src/file').ino }
    assert { inodes['dir'] == File.stat('src/dir').ino }
end

root_testenv("", :title => "setgid directories") do
    mkdir('mnt/dir')
    chmod("g+s", 'mnt/dir')
    chown(nil, $nogroup_gid, 'mnt/dir')
    
    touch('mnt/dir/file')
    
    assert { File.stat('src/dir').mode & 07000 == 02000 }
    assert { File.stat('src/dir/file').gid == $nogroup_gid }
    assert { File.stat('mnt/dir').mode & 07000 == 02000 }
    assert { File.stat('mnt/dir/file').gid == $nogroup_gid }
end

# FIXME: this stuff around testenv is a hax, and testenv may also exit(), which defeats the 'ensure' below.
# the test setup ought to be refactored. It might well use MiniTest or something.
if Process.uid == 0
    begin
        `groupdel bindfs_test_group 2>&1`
        `groupadd -f bindfs_test_group`
        raise "Failed to create test group" if !$?.success?
        testenv("--mirror=@bindfs_test_group", :title => "SIGUSR1 rereads user database") do |bindfs_pid|
            touch('src/file')
            chown('nobody', nil, 'src/file')

            assert { File.stat('mnt/file').uid == $nobody_uid }
            `adduser root bindfs_test_group`
            raise "Failed to add root to test group" if !$?.success?
            
            # Cache not refreshed yet
            assert { File.stat('mnt/file').uid == $nobody_uid }
            
            Process.kill("SIGUSR1", bindfs_pid)
            sleep 0.5
            
            assert { File.stat('mnt/file').uid == 0 }
        end
    ensure
        `groupdel bindfs_test_group 2>&1`
    end
end
