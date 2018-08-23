' select debugging output to a TV
'#define TV_DEBUG

OBJ
  pkt : "packet_driver"
#ifdef TV_DEBUG
  tv   : "TV_Text"
#endif
  runtime : "vm_runtime"
  vm : "vm_interface"

CON
  ' these will get overwritten with the correct values before loading
  _clkmode    = xtal1 + pll16x
  _xinfreq    = 5_000_000

  TYPE_VM_INIT = 1
  TYPE_HUB_WRITE = 2
  TYPE_DATA = 3
  TYPE_EOF = 4
  TYPE_RUN = 5

  ' serial helper stack size
  helper_stack_size = 128 * 4

  ' character codes
  CR = $0d
  LF = $0a

  WRITE_NONE = 0
  WRITE_HUB = 1

' hub memory layout
'   vm_mbox
'   vm_state
'   vm_params - not used after VM starts
'   vm_data

  hub_memory_size = 32 * 1024
  vm_mbox_size = vm#_MBOX_SIZE * 4
  vm_state_size = vm#_STATE_SIZE * 4

  def_vm_mbox = hub_memory_size - vm_mbox_size
  def_vm_state = def_vm_mbox - vm_state_size
  def_data_end = def_vm_state

VAR

  long write_mode
  long load_address
  long image_address

PUB start | type, packet, len, ok

  ' setup the base of data space just beyond the helper stack
  mm_data := @result + helper_stack_size

  ' start the packet driver
  pkt.start(p_rxpin, p_txpin, 0, p_baudrate)

#ifdef TV_DEBUG
  tv.start(p_tvpin)
  tv.str(string("xbasic loader v0.2 (data:"))
  tv.hex(mm_data, 4)
  tv.str(string(")"))
  crlf
#endif

  ' initialize
  write_mode := WRITE_NONE

  ' handle packets
  repeat

    ' get the next packet from the PC
    ok := pkt.rcv_packet(@type, @packet, @len)
#ifdef TV_DEBUG
    if not ok
      tv.str(string("Receive packet error", CR))
#endif

    if ok
      case type
        TYPE_VM_INIT:           VM_INIT_handler
        TYPE_HUB_WRITE:         HUB_WRITE_handler
        TYPE_DATA:              DATA_handler(packet, len)
        TYPE_EOF:               EOF_handler
        TYPE_RUN:               RUN_handler(packet)
        other:
#ifdef TV_DEBUG
          tv.str(string("Bad packet type: "))
          tv.hex(type, 2)
          crlf
#endif
      pkt.release_packet

PRI VM_INIT_handler
#ifdef TV_DEBUG
  tv.str(string("VM_INIT", CR))
#endif
  runtime.init(mm_vm_mbox, mm_vm_state, mm_data, mm_data)

PRI HUB_WRITE_handler
#ifdef TV_DEBUG
  tv.str(string("HUB_WRITE", CR))
#endif
  write_mode := WRITE_HUB
  image_address := runtime#HUB_BASE
  load_address := $00000000

PRI DATA_handler(packet, len) | i, val
#ifdef TV_DEBUG
  tv.str(string("DATA: "))
  tv.hex(load_address, 8)
  tv.out(" ")
  tv.dec(len)
  crlf
#endif
  case write_mode
    WRITE_HUB:
      repeat i from 0 to len - 1 step 4
        long[mm_data + load_address] := long[packet+i]
        load_address += 4
    other:
#ifdef TV_DEBUG
      tv.str(string("Bad write_mode: "))
      tv.hex(write_mode, 8)
      crlf
#endif

PRI EOF_handler
#ifdef TV_DEBUG
  tv.str(string("EOF", CR))
#endif
  case write_mode
    WRITE_HUB:
    other:
#ifdef TV_DEBUG
      tv.str(string("Bad write_mode: "))
      tv.hex(write_mode, 8)
      crlf
#endif

PRI RUN_handler(packet) | main, stack, stack_size, count, p, i, base, offset, size

#ifdef TV_DEBUG
  tv.str(string("RUN", CR))
#endif

  runtime.load(mm_vm_mbox, mm_vm_state, image_address, mm_data_end)

  ' stop all COGs
  pkt.stop
#ifdef TV_DEBUG
  tv.stop
#endif

  runtime.init_serial(p_baudrate, p_rxpin, p_txpin)

  if (long[packet] & 2) <> 0
    waitcnt(clkfreq + cnt)

  if (long[packet] & 1) == 0
    runtime.run(mm_vm_mbox, mm_vm_state)
  else
    runtime.single_step(mm_vm_mbox, mm_vm_state)

#ifdef TV_DEBUG
PRI crlf
  tv.out(CR)
#endif

#ifdef TV_DEBUG
PRI taghex8(tag, val)
  tv.str(tag)
  tv.hex(val, 8)
  crlf
#endif

DAT

' parameters filled in before downloading serial_helper.binary
p_baudrate    long      0
p_rxpin       byte      0
p_txpin       byte      0
p_tvpin       byte      0

mm_vm_mbox    long      def_vm_mbox
mm_vm_state   long      def_vm_state
mm_data_end   long      def_data_end
mm_data       long      0

