all:	advsys2.dat advsys2_run.binary advsys2_step.binary

run:	advsys2_run.binary advsys2.dat
	proploader -s -t advsys2_run.binary

step:	advsys2_step.binary advsys2.dat
	proploader -s -t advsys2_step.binary

test:	../test.adv
	cd ..;./adv2com test.adv
	mv ../test.dat advsys2.dat
	openspin advsys2_run.spin
	proploader -s -t advsys2_run.binary

testd:	../test.adv
	cd ..;./adv2com test.adv
	mv ../test.dat advsys2.dat
	openspin advsys2_step.spin
	proploader -s -t advsys2_step.binary

game:	../game.adv ../game.adi
	cd ..;./adv2com game.adv
	mv ../game.dat advsys2.dat
	openspin advsys2_run.spin
	proploader -s -t advsys2_run.binary

gamed:	../game.adv ../game.adi
	cd ..;./adv2com -d game.adv
	mv ../game.dat advsys2.dat
	openspin advsys2_step.spin
	proploader -s -t advsys2_step.binary

wordfire:	../wordfire_game.adv ../game.adi
	cd ..;./adv2com wordfire_game.adv
	mv ../wordfire_game.dat advsys2.dat
	openspin wordfire.spin
	proploader -s wordfire.binary

wordfire:	../wordfire_game.adv ../game.adi
	cd ..;./adv2com wordfire_game.adv
	mv ../wordfire_game.dat advsys2.dat
	openspin wordfire.spin
	proploader -s wordfire.binary

%.binary:	%.spin vm_runtime.spin advsys2_vm.spin advsys2.dat
	openspin $<

advsys2.dat:	test.adv
	../adv2com test.adv
	mv test.dat advsys2.dat

dump:
	od -Ax -tx4 advsys2.dat

clean:
	rm -f *.dat *.binary
