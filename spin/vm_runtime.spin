OBJ
  ser : "FullDuplexSerial"
  vm : "advsys2_vm"

CON

  _INIT_SIZE = vm#_INIT_SIZE
  _MBOX_SIZE = vm#_MBOX_SIZE
  _STATE_SIZE = vm#_STATE_SIZE
  
  ' character codes
  CR = $0d
  LF = $0a
  
VAR
  ' this should be a local variable but there is a bug in Spin that prevents
  ' using vm$_INIT_SIZE as the size of a local array
  long initParams[vm#_INIT_SIZE]
  long codeBase

PUB init_serial(baudrate, rxpin, txpin)
  ser.start(rxpin, txpin, 0, baudrate)

PUB init(mbox, state, stack, stack_size, image)
  codeBase := image + long[image][vm#IMAGE_CodeOffset]
  initParams[vm#INIT_IMAGE] := image
  initParams[vm#INIT_STATE] := state
  initParams[vm#INIT_MBOX] := mbox
  initParams[vm#INIT_STACK] := stack
  initParams[vm#INIT_STACK_SIZE] := stack_size
  vm.start(@initParams)

PUB single_step(mbox, state)
  state_header(state)
  process_requests(mbox, state, vm#STS_Step)

PUB run(mbox, state)
  vm.run(mbox, state)
  process_requests(mbox, state, vm.poll(mbox))

PRI process_requests(mbox, state, sts)
  repeat
    case sts
      vm#STS_Step:
        do_step(mbox, state)
      vm#STS_Trap:
        do_trap(mbox, state)
      vm#STS_Halt:
        'enable this for debugging
        halt(mbox, state, string("HALT"))
      vm#STS_StackOver:
        halt(mbox, state, string("STACK OVERFLOW"))
      vm#STS_DivideZero:
        halt(mbox, state, string("DIVIDE BY ZERO"))
      vm#STS_IllegalOpcode:
        halt(mbox, state, string("ILLEGAL OPCODE"))
      other:
        ser.str(string("sts: "))
        ser.hex(sts, 8)
        halt2(mbox, state)
    sts := vm.poll(mbox)

PRI halt(mbox, state, reason)
  ser.str(reason)
  ser.str(string(": "))
  halt2(mbox, state)

PRI halt2(mbox, state)
  state_header(state)
  show_status(mbox, state)
  repeat

PRI state_header(state) | stack
  stack := long[state][vm#STATE_STACK]
  ser.str(string("STACK "))
  ser.hex(stack, 8)
  ser.str(string(", STACK_TOP "))
  ser.hex(long[state][vm#STATE_STACK_TOP], 8)
  ser.crlf
  ser.str(string("PC       OP FP       SP       TOS      SP[0]    SP[1]    SP[2]    SP[3]", $d, $a))

PRI do_step(mbox, state)
  show_status(mbox, state)
  repeat while ser.rx <> " "
  vm.single_step(mbox, state)

PRI do_trap(mbox, state) | p, len, ch
  case long[mbox][vm#MBOX_ARG2_FCN]
    vm#TRAP_GetChar:
	  push_tos(state)
      long[state][vm#STATE_TOS] := ser.rx
    vm#TRAP_PutChar:
      ser.tx(long[state][vm#STATE_TOS])
      pop_tos(state)
    vm#TRAP_PrintStr:
      ser.str(long[state][vm#STATE_TOS])
      pop_tos(state)
    vm#TRAP_PrintInt:
      ser.dec(long[state][vm#STATE_TOS])
      pop_tos(state)
    vm#TRAP_PrintTab:
      ser.tx(8)
    vm#TRAP_PrintNL:
      ser.crlf
    vm#TRAP_PrintFlush:
    other:
        ser.str(string("UNKNOWN TRAP:"))
        ser.hex(long[mbox][vm#MBOX_ARG2_FCN], 8)
        ser.crlf
  if long[state][vm#STATE_STEPPING]
    do_step(mbox, state)
  else
    vm.continue(mbox)

PRI push_tos(state) | sp
  sp := long[state][vm#STATE_SP] - 4
  long[sp] := long[state][vm#STATE_TOS]
  long[state][vm#STATE_SP] := sp

PRI pop_tos(state) | sp
  sp := long[state][vm#STATE_SP]
  long[state][vm#STATE_TOS] := long[sp]
  long[state][vm#STATE_SP] := sp + 4

PRI show_status(mbox, state) | pc, sp, i
  pc := long[state][vm#STATE_PC]
  ser.hex(pc - codeBase, 8)
  ser.tx(" ")
  ser.hex(vm.read_byte(mbox, pc), 2)
  ser.tx(" ")
  ser.hex(long[state][vm#STATE_FP], 8)
  ser.tx(" ")
  sp := long[state][vm#STATE_SP]
  ser.hex(sp, 8)
  ser.tx(" ")
  ser.hex(long[state][vm#STATE_TOS], 8)
  repeat i from 0 to 3 
    ser.tx(" ")
    ser.hex(long[sp][i], 8)
  ser.crlf

PUB show_state(state)
  ser.str(string("fp:"))
  ser.hex(long[state][vm#STATE_FP], 8)
  ser.str(string(" sp:"))
  ser.hex(long[state][vm#STATE_SP], 8)
  ser.str(string(" tos:"))
  ser.hex(long[state][vm#STATE_TOS], 8)
  ser.str(string(" pc:"))
  ser.hex(long[state][vm#STATE_PC], 8)
  ser.str(string(" stepping:"))
  ser.hex(long[state][vm#STATE_STEPPING], 8)
  ser.str(string(" stack:"))
  ser.hex(long[state][vm#STATE_STACK], 8)
  ser.str(string(" stackTop:"))
  ser.hex(long[state][vm#STATE_STACK_TOP], 8)
  ser.crlf
