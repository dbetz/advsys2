run:
	openspin advsys2_run.spin
	proploader -s -t advsys2_run.binary

step:
	openspin advsys2_step.spin
	proploader -s -t advsys2_step.binary

dump:
	od -Ax -tx4 advsys2.dat

clean:
	rm *.binary
