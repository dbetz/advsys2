all:	advsys2.dat advsys2_run.binary advsys2_step.binary

run:	advsys2_run.binary advsys2.dat
	proploader -s -t advsys2_run.binary

step:	advsys2_step.binary advsys2.dat
	proploader -s -t advsys2_step.binary

%.binary:	%.spin vm_runtime.spin advsys2_vm.spin advsys2.dat
	openspin $<

advsys2.dat:	test.adv
	../adv2com test.adv
	mv test.dat advsys2.dat

dump:
	od -Ax -tx4 advsys2.dat

clean:
	rm -f *.dat *.binary
