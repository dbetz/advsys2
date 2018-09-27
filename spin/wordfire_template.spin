OBJ

  runtime : "wordfire_runtime"

CON

  _clkmode = XTAL1 + PLL16X
  _clkfreq = 80000000

  hub_memory_size = 32 * 1024
  
  vm_mbox_size = runtime#_MBOX_SIZE * 4
  vm_state_size = runtime#_STATE_SIZE * 4

  vm_mbox = hub_memory_size - vm_mbox_size
  vm_state = vm_mbox - vm_state_size
  
  vm_stack_size = 1024
  vm_stack = vm_state - vm_stack_size
  
PUB start
  runtime.init_video_and_keyboard
  runtime.init(vm_mbox, vm_state, vm_stack, vm_stack_size, p_image)
  waitcnt(clkfreq+cnt) ' this is a hack!
  'runtime.show_state(vm_state)
  runtime.run(vm_mbox, vm_state)
  repeat

DAT

' parameters filled in before downloading
p_image             word    0
