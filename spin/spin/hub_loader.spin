CON

  _clkmode = XTAL1 + PLL16X
  _clkfreq = 80000000

  hub_memory_size = 32 * 1024
  vm_mbox_size = runtime#_MBOX_SIZE * 4
  vm_state_size = runtime#_STATE_SIZE * 4

  vm_mbox = hub_memory_size - vm_mbox_size
  vm_state = vm_mbox - vm_state_size
  
  max_image_size = 18 * 1024

OBJ

  runtime : "vm_runtime"

PUB start
  runtime.init(vm_mbox, vm_state, @vm_code, @image, 0, 0)
  runtime.load(vm_mbox, vm_state, runtime#HUB_BASE, @image + max_image_size)
  runtime.init_serial(p_baudrate, p_rxpin, p_txpin)
  waitcnt(clkfreq+cnt) ' this is a hack!
  runtime.run(vm_mbox, vm_state)

DAT

' parameters filled in before downloading flash_loader.binary
params
p_image_size        long    0
p_baudrate          long    0
p_rxpin             byte    0
p_txpin             byte    0
p_unused            word    0

' pointers to code images
vm_code_off         long    @vm_code - @params
image_off           long    @image - @params
max_image_size_val  long    max_image_size

vm_code             long    0[496]
image               byte    0[max_image_size]
