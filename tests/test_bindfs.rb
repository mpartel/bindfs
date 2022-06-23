#!/usr/bin/env ruby
#
#   Copyright 2006,2007,2008,2009,2010,2012,2019 Martin Pärtel <martin.partel@gmail.com>
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

Dir.chdir(File.dirname(__FILE__))

# if we are being run by make check it will set srcdir and we should use it
$LOAD_PATH << (ENV['srcdir'] || '.')

require 'common.rb'
require 'etc'
require 'tempfile'

include Errno

$have_fuse_3 = Proc.new do
  system("pkg-config --exists fuse3")
  $?.success?
end.call
$have_fuse_3_readdir_bug = $have_fuse_3 && Proc.new do
  system("pkg-config --max-version=3.10.1 fuse3")
  $?.success?
end.call

$have_fuse_29 = !$have_fuse_3 && Proc.new do
  v = `pkg-config --modversion fuse`.split('.')
  raise "failed to get FUSE version with pkg-config" if v.size < 2
  v = v.map(&:to_i)
  v = [2, 8, 0]
  v[0] > 2 || (v[0] == 2 && v[1] >= 9)
end.call

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
$nobody_gid = nobody_gid = Etc.getpwnam('nobody').gid
$nobody_group = nobody_group = Etc.getgrgid(nobody_gid).name
$root_group = root_group = Etc.getgrgid(0).name

$tests_dir = File.realpath('.')


testenv("") do
    assert { File.basename(pwd) == TESTDIR_NAME }
end

testenv("-u nobody -g #{nobody_group}") do
    touch('src/file')

    assert { File.stat('mnt/file').uid == nobody_uid }
    assert { File.stat('mnt/file').gid == nobody_gid }
end

testenv("-u #{nobody_uid} -g #{nobody_gid}", :title => "numeric UID and GID") do
    touch('src/file')

    assert { File.stat('mnt/file').uid == nobody_uid }
    assert { File.stat('mnt/file').gid == nobody_gid }
end

testenv("-u 55544 -g 55566", :title => "nonexistent UID and GID") do
    touch('src/file')

    assert { File.stat('mnt/file').uid == 55544 }
    assert { File.stat('mnt/file').gid == 55566 }
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
  `sudo -u nobody -g #{nobody_group} touch mnt/file`
  `sudo -u nobody -g #{nobody_group} mkdir mnt/dir`
  `sudo -u nobody -g #{nobody_group} ln -sf /tmp/foo mnt/lnk`

  assert { File.stat('mnt/file').uid == nobody_uid }
  assert { File.stat('mnt/file').gid == nobody_gid }
  assert { File.stat('src/file').uid == nobody_uid }
  assert { File.stat('src/file').gid == nobody_gid }

  assert { File.stat('mnt/dir').uid == nobody_uid }
  assert { File.stat('mnt/dir').gid == nobody_gid }
  assert { File.stat('src/dir').uid == nobody_uid }
  assert { File.stat('src/dir').gid == nobody_gid }

  assert { File.lstat('mnt/lnk').uid == nobody_uid }
  assert { File.lstat('mnt/lnk').gid == nobody_gid }
  assert { File.lstat('src/lnk').uid == nobody_uid }
  assert { File.lstat('src/lnk').gid == nobody_gid }
end

testenv("--create-with-perms=og=r:ogd+x") do
    with_umask(0077) do
        touch('mnt/file')
        mkdir('mnt/dir')
    end

    assert { File.stat('mnt/file').mode & 0777 == 0644 }
    assert { File.stat('mnt/dir').mode & 0777 == 0755 }
end

testenv("--create-with-perms=g+rD") do
    with_umask(0077) do
        touch('mnt/file')
        mkdir('mnt/dir')
    end

    assert { File.stat('src/file').mode & 0777 == 0640 }
    assert { File.stat('src/dir').mode & 0777 == 0750 }
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
        lambda { chown(nil, $nobody_group, mntfile) },
        lambda { chown('nobody', $nobody_group, mntfile) }
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
                    assert { gid != $nobody_gid }
                when :gid
                    assert { uid != $nobody_uid }
                    assert { gid == $nobody_gid }
                when :both
                    assert { uid == $nobody_uid }
                    assert { gid == $nobody_gid }
                when nil
                    assert { uid != $nobody_uid }
                    assert { gid != $nobody_gid }
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
    assert_exception(EPERM) { chown('nobody', nobody_group, 'mnt/file') }
    chown(nil, nobody_group, 'mnt/file')
end

root_testenv("--mirror=root") do
    touch('src/file')
    chown('nobody', nobody_group, 'src/file')

    assert { File.stat('mnt/file').uid == 0 }
    assert { File.stat('mnt/file').gid == $nobody_gid }
end

if `uname`.strip != 'FreeBSD'  # FreeBSD doesn't let us set the sticky bit on files
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
end

testenv("--chmod-deny --chmod-allow-x") do
    touch('src/file')

    chmod(0700, 'src/file')

    chmod(0700, 'mnt/file') # no-op chmod should work

    assert_exception(EPERM) { chmod(0777, 'mnt/file') }
    assert_exception(EPERM) { chmod(0000, 'mnt/file') }
    if `uname`.strip != 'FreeBSD'  # FreeBSD doesn't let us set the sticky bit on files
      assert_exception(EPERM) { chmod(01700, 'mnt/file') } # sticky bit
    end

    chmod(0611, 'mnt/file') # chmod that only changes x-bits should work
    assert { File.stat('src/file').mode & 07777 == 00611 }


    mkdir('src/dir')
    chmod(0700, 'src/dir')
    assert_exception(EPERM) { chmod(0700, 'mnt/dir') } # chmod on dir should not work
end

testenv("--chmod-filter=g-w,o-rwx") do
    touch('src/file')

    chmod(0666, 'mnt/file')

    assert { File.stat('src/file').mode & 0777 == 0640 }
end

testenv("--delete-deny") do
    touch('src/file')
    mkdir('src/dir')
    assert_exception(EPERM) { rm('mnt/file') }
    assert_exception(EPERM) { rmdir('mnt/dir') }
end

testenv("--rename-deny") do
    touch('src/file')
    mkdir('src/dir')
    # We don't use FileUtils.mv because it was changed in Ruby 2.7.1 to
    # fall back to copying on EPERM:
    # https://github.com/ruby/ruby/commit/7d3d8e79fe9cc9f21cd4341f0a6fb2e6306688fd
    assert_exception(EPERM) { File.rename('mnt/file', 'mnt/file2') }
    assert_exception(EPERM) { File.rename('mnt/dir', 'mnt/dir2') }
end

root_testenv("--map=nobody/root:@#{nobody_group}/@#{root_group}") do
    touch('src/file')
    chown('nobody', nobody_group, 'src/file')

    assert { File.stat('mnt/file').uid == 0 }
    assert { File.stat('mnt/file').gid == 0 }

    touch('mnt/newfile')
    mkdir('mnt/newdir')

    assert { File.stat('src/newfile').uid == $nobody_uid }
    assert { File.stat('src/newfile').gid == $nobody_gid }
    assert { File.stat('src/newdir').uid == $nobody_uid }
    assert { File.stat('src/newdir').gid == $nobody_gid }

    assert { File.stat('mnt/newfile').uid == 0 }
    assert { File.stat('mnt/newfile').gid == 0 }
    assert { File.stat('mnt/newdir').uid == 0 }
    assert { File.stat('mnt/newdir').gid == 0 }
end

root_testenv("--map=@#{nobody_group}/@#{root_group}") do
    touch('src/file')
    chown('nobody', nobody_group, 'src/file')

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

Tempfile.open('passwdfile') do |passwd_file|
    Tempfile.open('groupfile') do |group_file|
        passwd_file.puts("nobody:x:123:456:,,,:/tmp:/bin/false")
        passwd_file.flush
        group_file.write("#{nobody_group}:x:789")
        group_file.flush

        root_testenv("--map-passwd=#{Shellwords.escape(passwd_file.path)} --map-group=#{Shellwords.escape(group_file.path)}") do
            touch('src/file1')
            chown(123, 789, 'src/file1')
            assert { File.stat('mnt/file1').uid == nobody_uid }
            assert { File.stat('mnt/file1').gid == nobody_gid }

            touch('src/file2')
            chown(nobody_uid, nobody_gid, 'mnt/file2')
            assert { File.stat('src/file2').uid == 123 }
            assert { File.stat('src/file2').gid == 789 }
        end

        # Pull Request 113
        root_testenv("--map-passwd-rev=#{Shellwords.escape(passwd_file.path)} --map-group-rev=#{Shellwords.escape(group_file.path)}") do
            touch('src/file1')
            chown(nobody_uid, nobody_gid, 'src/file1')
            assert { File.stat('mnt/file1').uid == 123 }
            assert { File.stat('mnt/file1').gid == 789 }

            touch('src/file2')
            chown(123, 789, 'mnt/file2')
            assert { File.stat('src/file2').uid == nobody_uid }
            assert { File.stat('src/file2').gid == nobody_gid }
        end
    end
end

root_testenv("--uid-offset=2") do
    touch('src/file')
    chown(1, nil, 'src/file')

    assert { File.stat('mnt/file').uid == 3 }
end

root_testenv("--gid-offset=2") do
    touch('src/file')
    chown(nil, 1, 'src/file')

    assert { File.stat('mnt/file').gid == 3 }
end

root_testenv("--uid-offset=2 --gid-offset=20", :title => "file creation with --uid-offset and --gid-offset") do
    touch('mnt/file')

    assert { File.stat('src/file').uid == 0 }
    assert { File.stat('mnt/file').uid == 2 }
    # Note: BSDs tend to inherit group from parent dir while Linux uses the effective GID.
    # This check works for both.
    assert { File.stat('mnt/file').gid == File.stat('src/file').gid + 20 }
end

root_testenv("--uid-offset=2 --gid-offset=20", :title => "chown/chgrp with --uid-offset and --gid-offset") do
    touch('src/file')
    chown(6, 25, 'mnt/file')

    assert { File.stat('src/file').uid == 4 }
    assert { File.stat('src/file').gid == 5 }
    assert { File.stat('mnt/file').uid == 6 }
    assert { File.stat('mnt/file').gid == 25 }
end

testenv("", :title => "preserves inode numbers") do
    touch('src/file')
    mkdir('src/dir')
    assert { File.stat('mnt/file').ino == File.stat('src/file').ino }
    assert { File.stat('mnt/dir').ino == File.stat('src/dir').ino }
end

unless $have_fuse_3_readdir_bug  # https://github.com/libfuse/libfuse/issues/583
    testenv("", :title => "preserves readdir inode numbers") do
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
end

root_testenv("", :title => "setgid directories") do
    mkdir('mnt/dir')
    chmod("g+s", 'mnt/dir')
    chown(nil, $nobody_gid, 'mnt/dir')

    touch('mnt/dir/file')

    assert { File.stat('src/dir').mode & 07000 == 02000 }
    assert { File.stat('src/dir/file').gid == $nobody_gid }
    assert { File.stat('mnt/dir').mode & 07000 == 02000 }
    assert { File.stat('mnt/dir/file').gid == $nobody_gid }
end

testenv("", :title => "utimens on symlinks") do
    touch('mnt/file')
    Dir.chdir "mnt" do
      symlink('file', 'link')
    end

    sleep 1  # Not sure why this is needed, but something seems to overwrite the atime right after we set it, at least on Bionic.
    system("#{$tests_dir}/utimens_nofollow mnt/link 12 34 56 78")
    raise "Failed to run utimens_nofollow: #{$?.inspect}" unless $?.success?

    assert { File.lstat('mnt/link').atime.to_i < 50 }
    assert { File.lstat('mnt/link').mtime.to_i < 100 }
    assert { File.lstat('mnt/file').atime.to_i > 100 }
    assert { File.lstat('mnt/file').mtime.to_i > 100 }
end

testenv("--resolve-symlinks", :title => "resolving symlinks") do
  mkdir('src/dir')
  File.write('src/dir/file', 'hello')
  Dir.chdir 'src' do
    symlink('dir', 'dirlink')
    symlink('dir/file', 'filelink')
    symlink('dirlink/file', 'filelink2')
  end

  assert { !File.lstat('mnt/dirlink').symlink? }
  assert { File.lstat('mnt/dirlink').directory? }
  assert { !File.lstat('mnt/dirlink/file').symlink? }
  assert { File.lstat('mnt/dirlink/file').file? }
  assert { File.lstat('mnt/filelink').file? }
  assert { File.read('mnt/filelink') == 'hello' }
end

testenv("--resolve-symlinks", :title => "attributes of resolved symlinks") do
  Dir.chdir 'src' do
    touch('file')
    symlink('file', 'link')
    chmod(0654, 'file')
  end

  assert { File.lstat('mnt/link').mode & 0777 == 0654 }
end

testenv("--resolve-symlinks", :title => "writing through resolved symlinks") do
  Dir.chdir 'src' do
    File.write('file', 'initial_content')
    symlink('file', 'link')
  end

  File.write('mnt/link', 'new_content')
  assert { File.read('src/file') == 'new_content' }
  assert { File.read('src/link') == 'new_content' }
  assert { File.symlink?('src/link') }
end

testenv("--resolve-symlinks", :title => "moving over resolved symlinks") do
  Dir.chdir 'src' do
    File.write('file', 'initial_content')
    File.write('newfile', 'new_content')
    symlink('file', 'link')
  end

  Dir.chdir 'mnt' do
    system("mv newfile link")
  end
  assert { File.symlink?('src/link') }
  assert { File.read('src/file') == 'new_content' }
  assert { !File.exist?('src/newfile') }
end

testenv("--resolve-symlinks", :title => "moving resolved symlinks") do
  Dir.chdir 'src' do
    touch('file')
    symlink('file', 'link')
  end

  Dir.chdir 'mnt' do
    system("mv link lonk")
  end
  assert { !File.symlink?('src/link') }
  assert { File.lstat('src/lonk').symlink? }
  assert { File.readlink('src/lonk') == 'file' }
end

testenv("--resolve-symlinks", :title => "--resolve-symlinks disallows new symlinks") do
  touch('mnt/file')
  Dir.chdir "mnt" do
    begin
      File.symlink("file", "link")
    rescue Errno::EPERM => exception
    end
    assert { exception != nil }
  end
end

testenv("--resolve-symlinks", :title => "deleting a resolved symlink deletes the underlying symlink only by default") do
  Dir.chdir 'src' do
    touch('file')
    symlink('file', 'link')
    symlink('broken', 'broken_link')
  end

  File.unlink('mnt/link')
  assert { !File.symlink?('src/link') }
  assert { File.exist?('src/file') }

  File.unlink('mnt/broken_link')
  assert { !File.symlink?('src/broken_link') }
end

testenv("--resolve-symlinks --resolved-symlink-deletion=deny") do
  Dir.chdir 'src' do
    touch('file')
    symlink('file', 'link')
  end

  begin
    File.unlink('mnt/link')
  rescue Errno::EPERM => exception
  end
  assert { exception != nil }
  assert { File.symlink?('src/link') }
  assert { File.exist?('src/file') }
end

# TODO: make all tests runnable as root. This is nonroot because we can't
# easily prevent a bindfs running as root from deleting a file.
nonroot_testenv("--resolve-symlinks --resolved-symlink-deletion=symlink-first") do
  begin
    Dir.chdir 'src' do
      mkdir('dir')
      touch('deletable_file')
      touch('dir/undeletable_file')
      chmod(0555, 'dir')
      symlink('deletable_file', 'link1')
      symlink('dir/undeletable_file', 'link2')
      symlink('broken', 'link3')
    end

    File.unlink('mnt/link1')
    assert { !File.symlink?('src/link1') }
    assert { !File.exist?('src/dir/deletable_file') }

    File.unlink('mnt/link2')
    assert { !File.symlink?('src/link2') }
    assert { File.exist?('src/dir/undeletable_file') }

    File.unlink('mnt/link3')
    assert { !File.symlink?('src/link3') }
  ensure
    chmod(0777, 'src/dir')  # So the cleanup code can delete dir/*
  end
end

# TODO: make all tests runnable as root. This is nonroot because we can't
# easily prevent a bindfs running as root from deleting a file.
nonroot_testenv("--resolve-symlinks --resolved-symlink-deletion=target-first -p a+w") do
  begin
    Dir.chdir 'src' do
      mkdir('dir')
      touch('file1')
      touch('file2')
      symlink('file1', 'deletable_link')
      Dir.chdir('dir') do
        symlink('../file2', 'undeletable_link')
      end
      chmod(0555, 'dir')
      symlink('broken', 'broken_link')
    end

    File.unlink('mnt/deletable_link')
    assert { !File.symlink?('src/deletable_link') }
    assert { !File.exist?('src/file1') }

    begin
      File.unlink('mnt/dir/undeletable_link')
    rescue Errno::EACCES => exception
    end
    assert { exception != nil }
    assert { File.symlink?('src/dir/undeletable_link') }
    assert { !File.exist?('src/file2') }

    File.unlink('mnt/broken_link')
    assert { !File.symlink?('src/broken_link') }
  ensure
    chmod(0777, 'src/dir')  # So the cleanup code can delete dir/*
  end
end

testenv("--resolve-symlinks", :title => "resolving broken symlinks") do
  Dir.chdir 'src' do
    symlink('dir', 'dirlink')
    symlink('dir/file', 'filelink')
  end

  assert { File.lstat('mnt/dirlink').symlink? }
  assert { File.lstat('mnt/filelink').symlink? }

  File.unlink('mnt/filelink')
  assert { !File.exist?('mnt/filelink') }

  File.unlink('mnt/dirlink')
  assert { !File.exist?('mnt/dirlink') }
end

# Issue #28 reproduction attempt.
testenv("", :title => "many files in a directory") do
  mkdir('src/dir')
  expected_entries = ['.', '..']
  10000.times do |i|
    touch("src/dir/#{i}")
    expected_entries << i.to_s
  end

  assert { Dir.entries('mnt/dir').sort == expected_entries.sort }
end

if $have_fuse_29 || $have_fuse_3
  testenv("--enable-lock-forwarding --multithreaded", :title => "lock forwarding") do
    File.write('src/file', 'some contents for fcntl lockng')
    # (this test passes with an empty file as well, but this way is clearer)

    # flock
    File.open('mnt/file') do |f1|
      File.open('src/file') do |f2|
        assert { f1.flock(File::LOCK_EX | File::LOCK_NB)  }
        assert { !f2.flock(File::LOCK_EX | File::LOCK_NB)  }
        assert { f1.flock(File::LOCK_UN)  }

        assert { f2.flock(File::LOCK_EX | File::LOCK_NB)  }
        assert { !f1.flock(File::LOCK_EX | File::LOCK_NB)  }
      end
      assert { f1.flock(File::LOCK_EX | File::LOCK_NB)  }
    end

    # fcntl locking
    system("#{$tests_dir}/fcntl_locker src/file mnt/file")
    raise "fcntl lock sharing test failed" unless $?.success?
  end

  testenv("--disable-lock-forwarding", :title => "no lock forwarding") do
    File.write('src/file', 'some contents for fcntl lockng')

    # flock
    File.open('mnt/file') do |f1|
      File.open('src/file') do |f2|
        assert { f1.flock(File::LOCK_EX | File::LOCK_NB)  }
        assert { f2.flock(File::LOCK_EX | File::LOCK_NB)  }
      end
      File.open('mnt/file') do |f2|
        assert { !f2.flock(File::LOCK_EX | File::LOCK_NB)  }
      end
    end

    # fcntl locking
    system("#{$tests_dir}/fcntl_locker src/file mnt/file")
    raise "fcntl lock sharing test failed" unless $?.exitstatus == 1
  end
end # have_fuse_29

# Issue #37
#
# Valgrind is disabled for ioctl tests since it seems to give a false negative
# about a null parameter to ioctl.
#
# This test is also disabled for old (FUSE < 2.9) systems.
# TODO: figure out why it doesn't work.
if $have_fuse_29 || $have_fuse_3
  root_testenv("--enable-ioctl", :title => "append-only ioctl", :valgrind => false) do
    touch('mnt/file')
    system('chattr +a mnt/file')
    raise 'chattr +a failed' unless $?.success?
    begin
      File.open('mnt/file', 'a') do |f|
        f.write('stuff')
      end
      assert { File.read('mnt/file') == 'stuff' }
      assert_exception(EPERM) { File.write('mnt/file', 'newstuff') }
    ensure
      system('chattr -a mnt/file')
    end
  end
end

root_testenv("", :title => "ioctl not enabled by default", :valgrind => false) do
  touch('mnt/file')
  assert { `chattr +a mnt/file 2>&1`; !$?.success? }
end

# Issue #41
testenv("", :title => "reading directory with rewind") do
  touch('mnt/file1')
  touch('mnt/file2')
  mkdir('mnt/subdir')

  Dir.chdir 'mnt' do
    system("#{$tests_dir}/test_dir_rewind")
    assert { $?.success? }
  end
end

# Issue 47
testenv("", :title => "srcdir with comma", :srcdir_name => "a,b") do
  touch('a,b/f')
  assert { File.exist?('mnt/f') }
end

testenv("", :title => "mntdir with comma", :mntdir_name => "a,b") do
  touch('src/f')
  assert { File.exist?('a,b/f') }
end

testenv("", :title => "srcdir with space", :srcdir_name => "a b") do
  touch('a b/f')
  assert { File.exist?('mnt/f') }
end

testenv("", :title => "mntdir with space", :mntdir_name => "a b") do
  touch('src/f')
  assert { File.exist?('a b/f') }
end

testenv("", :title => "srcdir with newline", :srcdir_name => "a\nb") do
  touch("a\nb/f")
  assert { File.exist?('mnt/f') }
end

testenv("", :title => "mntdir with newline", :mntdir_name => "a\nb") do
  touch('src/f')
  assert { File.exist?("a\nb/f") }
end

# Pull Request #73
if `uname`.strip == 'Linux' &&
    `uname -r`.strip =~ /^[456789]/ &&  # 3.x kernels used by CentOS 7 and older don't support all `unshare` options despite the userspace binary supporting them
    `which unshare` != '' &&
    `unshare --help`.include?("--map-root-user") &&
    `unshare --help`.include?("--user")
  root_testenv("--gid-offset=10000", :title => "setgid and gid-offset") do
    system("chmod g+s src")
    if system("unshare --map-root-user --user mkdir mnt/dir")
      assert { File.stat("src/dir").gid == 0 }
      assert { File.stat("mnt/dir").gid == 10000 }
    end
  end
end

# Pull Request #74
if `uname`.strip == 'Linux'
  def odirect_data
    ('abc' * 10000)[0...8192]
  end

  testenv("", :title => "O_DIRECT reads with O_DIRECT ignored") do
    File.write("src/f", odirect_data)
    read_data = `#{$tests_dir}/odirect_read mnt/f`
    assert { $?.success? }
    assert { read_data == odirect_data }
  end

  testenv("", :title => "O_DIRECT writes with O_DIRECT ignored") do
    IO.popen("#{$tests_dir}/odirect_write mnt/f", "w") do |pipe|
      pipe.write(odirect_data)
    end
    assert { $?.success? }
    assert { File.read("src/f") == odirect_data }
  end

  testenv("--forward-odirect=512", :title => "O_DIRECT reads with O_DIRECT forwarded") do
    File.write("src/f", odirect_data)
    read_data = `#{$tests_dir}/odirect_read mnt/f`
    assert { $?.success? }
    assert { read_data == odirect_data }
  end

  testenv("--forward-odirect=512", :title => "O_DIRECT writes with O_DIRECT forwarded") do
    IO.popen("#{$tests_dir}/odirect_write mnt/f", "w") do |pipe|
      pipe.write(odirect_data)
    end
    assert { $?.success? }
    assert { File.read("src/f") == odirect_data }
  end
end

# Issue 94
if `uname`.strip != 'FreeBSD'  # -o fsname is not supported on FreeBSD
  testenv("-o fsname=_bindfs_test_123_", :title => "fsname") do
    assert { `mount` =~ /^_bindfs_test_123_\s+on\s+/m }
  end
end

if `uname`.strip != 'FreeBSD'  # -o dev is not supported on FreeBSD
  root_testenv("-odev") do
    system("mknod mnt/zero c 1 5")
    data = File.read("mnt/zero", 3)
    assert { data.length == 3 }
    assert { data.chars.to_a[0] == "\0" }
    assert { data.chars.to_a[1] == "\0" }
    assert { data.chars.to_a[2] == "\0" }
  end
end

# PR #95
testenv("-ouser -onofail,nouser,,,delete-deny -o users -o auto,rename-deny,noauto -o chmod-deny,_netdev,,", :title => "filtering special options") do
  touch('src/file')
  assert_exception(EPERM) { rm('mnt/file') }
  assert_exception(EPERM) { File.rename('mnt/file', 'mnt/file2') }
  assert_exception(EPERM) { chmod(0777, 'mnt/file') }
end

# FIXME: this stuff around testenv is a hax, and testenv may also exit(), which defeats the 'ensure' below.
# the test setup ought to be refactored. It might well use MiniTest or something.
# TODO: support FreeBSD in this test (different group management commands)
if Process.uid == 0 && `uname`.strip == 'Linux'
    begin
        `groupdel bindfs_test_group 2>&1`
        `groupadd -f bindfs_test_group`
        raise "Failed to create test group" if !$?.success?
        testenv("--mirror=@bindfs_test_group", :title => "SIGUSR1 rereads user database") do |bindfs_pid|
            touch('src/file')
            chown('nobody', nil, 'src/file')

            assert { File.stat('mnt/file').uid == $nobody_uid }
            `usermod -G bindfs_test_group -a root`
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
