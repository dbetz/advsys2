OBJ
  vga[4] : "advsys2_vga"              'VGA interface to VGA driver
  kbd    : "QuadKeyboard"             'Quad keyboard driver (JRG)
  vm     : "advsys2_vm"

CON

  _INIT_SIZE = vm#_INIT_SIZE
  _MBOX_SIZE = vm#_MBOX_SIZE
  _STATE_SIZE = vm#_STATE_SIZE
  
  ' character codes
  CR = $0d
  LF = $0a
  
  VOL = 20       'volume level control pin
  SND = 21       'audio/sound output pin  

VAR
  ' this should be a local variable but there is a bug in Spin that prevents
  ' using vm$_INIT_SIZE as the size of a local array
  long initParams[vm#_INIT_SIZE]
  long codeBase, stringBase
  long device

PUB init_video_and_keyboard | lock, s

  'config audio pins
  dira[SND]~~                   'audio output pin
  dira[VOL]~~                   'volume control pin  

  'set speaker volume
  ctra :=  %00110_000 << 23 + 1 << 9 + VOL   'counter mode = duty cycle for d/a using rc circuit (B pin is unused)
  frqa := $2000_0000                         'Vout = 3.3 * (frqa/2^32) = 3.3 * 0.5 = 1.65V max when frqa = $8000_0000 (can omit this line) 

  'configure video pins
  lock := ((cnt >> 16 + 2) | 1) << 16   
  repeat s from 0 to 3 
    vga[s].init(lock|vconfig[s])                'initialize vid drv with vgrp/vpins 
    
  'clear all of the screens
  repeat s from 0 to 3
    vga[s].clear 
    
  'write something on each screen
  'repeat s from 1 to 3
  '  vga[s].str(string("Hello, world!"))

  'start quad keyboard driver
  kbd.start
  repeat s from 0 to 3
    kbd.clearKeyBuffer(s)
    
  'select screen 0 as the default device
  device := 0

PUB init(mbox, state, stack, stack_size, image)
  codeBase := image + long[image][vm#IMAGE_CodeOffset]
  stringBase := image + long[image][vm#IMAGE_StringOffset]
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
      vm#STS_PropNotFound:
        halt(mbox, state, string("PROPERTY NOT FOUND"))
      vm#STS_UncaughtThrow:
        halt(mbox, state, string("UNCAUGHT THROW"))
      other:
        vga[0].str(string("sts: "))
        vga[0].hex(sts, 8)
        halt2(mbox, state)
    sts := vm.poll(mbox)

PRI halt(mbox, state, reason)
  vga[0].str(reason)
  vga[0].str(string(": "))
  halt2(mbox, state)

PRI halt2(mbox, state)
  state_header(state)
  show_status(mbox, state)
  repeat

PRI do_step(mbox, state)
  show_status(mbox, state)
  repeat while kbd.getKey(0) <> " "
  vm.single_step(mbox, state)

PRI do_trap(mbox, state) | p, len, ch
  case long[mbox][vm#MBOX_ARG2_FCN]
    vm#TRAP_GetChar:
	  push_tos(state)
	  ch := kbd.getKey(device)
      long[state][vm#STATE_TOS] := ch
    vm#TRAP_PutChar:
      vga[device].tx(long[state][vm#STATE_TOS])
      pop_tos(state)
    vm#TRAP_PrintStr:
      vga[device].str(stringBase + long[state][vm#STATE_TOS])
      pop_tos(state)
    vm#TRAP_PrintInt:
      vga[device].dec(long[state][vm#STATE_TOS])
      pop_tos(state)
    vm#TRAP_SetDevice:
      device := long[state][vm#STATE_TOS]
      pop_tos(state)
    other:
        vga[0].str(string("UNKNOWN TRAP:"))
        vga[0].hex(long[mbox][vm#MBOX_ARG2_FCN], 8)
        vga[0].crlf
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

PRI state_header(state) | stack
  stack := long[state][vm#STATE_STACK]
  vga[0].str(string("STACK "))
  vga[0].hex(stack, 8)
  vga[0].str(string(", STACK_TOP "))
  vga[0].hex(long[state][vm#STATE_STACK_TOP], 8)
  vga[0].crlf
  vga[0].str(string("PC       OP FP       SP       EFP        TOS      SP[0]...", $d, $a))

PRI show_status(mbox, state) | pc, sp, fp, stackTop, i
  pc := long[state][vm#STATE_PC]
  vga[0].hex(pc - codeBase, 8)
  vga[0].tx(" ")
  vga[0].hex(byte[codeBase][pc], 2)
  vga[0].tx(" ")
  fp := long[state][vm#STATE_FP]
  vga[0].hex(fp, 8)
  vga[0].tx(" ")
  sp := long[state][vm#STATE_SP]
  vga[0].hex(sp, 8)
  vga[0].tx(" ")
  vga[0].hex(long[state][vm#STATE_EFP], 8)
  vga[0].str(string("   "))
  vga[0].hex(long[state][vm#STATE_TOS], 8)
  stackTop := long[state][vm#STATE_STACK_TOP]
  repeat while sp < stackTop
    if fp == sp
      vga[0].str(string(" <fp>"))
    vga[0].tx(" ")
    vga[0].hex(long[sp], 8)
    sp += 4
  if fp == stackTop
    vga[0].str(string(" <fp>"))
  vga[0].crlf

PUB show_state(state)
  vga[0].str(string("fp:"))
  vga[0].hex(long[state][vm#STATE_FP], 8)
  vga[0].str(string(" sp:"))
  vga[0].hex(long[state][vm#STATE_SP], 8)
  vga[0].str(string(" tos:"))
  vga[0].hex(long[state][vm#STATE_TOS], 8)
  vga[0].str(string(" pc:"))
  vga[0].hex(long[state][vm#STATE_PC], 8)
  vga[0].str(string(" efp:"))
  vga[0].hex(long[state][vm#STATE_EFP], 8)
  vga[0].str(string(" stepping:"))
  vga[0].hex(long[state][vm#STATE_STEPPING], 8)
  vga[0].str(string(" stack:"))
  vga[0].hex(long[state][vm#STATE_STACK], 8)
  vga[0].str(string(" stackTop:"))
  vga[0].hex(long[state][vm#STATE_STACK_TOP], 8)
  vga[0].crlf
  
DAT 'vconfig
vconfig long    0 << 9 | %%103_3     'with sync   %010011_11  
        long    1 << 9 | %%103_0     'no sync     %010011_00      
        long    2 << 9 | %%103_0     '   
        long    3 << 9 | %%103_0     '        
