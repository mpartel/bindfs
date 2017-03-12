SUBDIRS = src tests

vagrant-clean:
	-for i in vagrant/*/Vagrantfile; do cd `dirname $$i` && (vagrant destroy -f || true) ; cd $$OLDPWD; done
	-rm -Rf vagrant/*/.vagrant vagrant/*/*.log

vagrant-test:
	-vagrant/test.rb
