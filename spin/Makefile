run:	advsys2.dat
	openspin advsys2_run.spin
	proploader -s -t advsys2_run.binary

step:	advsys2.dat
	openspin advsys2_step.spin
	proploader -s -t advsys2_step.binary

advsys2.dat:	../test2.adv
	../adv2com ../test2.adv
	mv ../test2.dat advsys2.dat

dump:
	od -Ax -tx4 advsys2.dat

clean:
	rm -f *.dat *.binary
