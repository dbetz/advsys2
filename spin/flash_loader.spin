CON

  _clkmode = XTAL1 + PLL16X
  _clkfreq = 80000000

  hub_memory_size = 32 * 1024
  vm_mbox_size = runtime#_MBOX_SIZE * 4
  vm_state_size = runtime#_STATE_SIZE * 4
  cache_mbox_size = cacheint#_MBOX_SIZE * 4

  loader_stack_size = 64 * 4

OBJ

  runtime : "vm_runtime"
  cacheint : "cache_interface"

PUB start | cache, cache_mbox, vm_mbox, vm_state, data, data_end, cache_line_mask
  cache := hub_memory_size - p_cache_size
  cache_mbox := cache - cache_mbox_size
  vm_mbox := cache_mbox - vm_mbox_size
  vm_state := vm_mbox - vm_state_size
  data_end := vm_state
  data := @result + loader_stack_size
  cache_line_mask := cacheint.start(@cache_code, cache_mbox, cache, 0, 0)
  runtime.init(vm_mbox, vm_state, @vm_code, data, cache_mbox, cache_line_mask)
  runtime.load(vm_mbox, vm_state, runtime#FLASH_BASE, data_end)
  runtime.init_serial(p_baudrate, p_rxpin, p_txpin)
  waitcnt(clkfreq+cnt)
  runtime.run(vm_mbox, vm_state)

DAT

' parameters filled in before downloading flash_loader.binary
params
p_cache_size        long    0
p_baudrate          long    0
p_rxpin             byte    0
p_txpin             byte    0
p_unused            word    0

' pointers to code images
vm_code_off         long    @vm_code - @params
cache_code_off      long    @cache_code - @params

vm_code             long    0[496]
cache_code          long    0[496]
