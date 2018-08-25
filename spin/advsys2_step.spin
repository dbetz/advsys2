OBJ

  runtime : "vm_runtime"

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
  runtime.init_serial(p_baudrate, p_rxpin, p_txpin)
  runtime.init(vm_mbox, vm_state, vm_stack, vm_stack_size, @image)
  waitcnt(clkfreq+cnt) ' this is a hack!
  runtime.show_state(vm_state)
  runtime.single_step(vm_mbox, vm_state)
  repeat

DAT

' parameters filled in before downloading hub_loader.binary
p_baudrate          long    115200
p_rxpin             byte    31
p_txpin             byte    30

                    long
image               file    "advsys2.dat"
