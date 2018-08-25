CON

INIT_IMAGE        = 0
INIT_STATE        = 1
INIT_MBOX         = 2
INIT_STACK        = 3
INIT_STACK_SIZE   = 4
_INIT_SIZE        = 5

MBOX_CMD          = 0
MBOX_ARG_STS      = 1
MBOX_ARG2_FCN     = 2
_MBOX_SIZE        = 3

IMAGE_DataOffset  = 0
IMAGE_DataSize    = 1
IMAGE_CodeOffset  = 2
IMAGE_CodeSize    = 3
IMAGE_StringOffset= 4
IMAGE_StringSize  = 5
IMAGE_MainFunction= 6
_IMAGE_SIZE       = 7

STATE_FP          = 0
STATE_SP          = 1
STATE_TOS         = 2
STATE_PC          = 3
STATE_STEPPING    = 4
STATE_STACK       = 5
STATE_STACK_TOP   = 6
_STATE_SIZE       = 7

VM_Continue       = 1
VM_ReadLong       = 2
VM_WriteLong      = 3
VM_ReadByte       = 4
_VM_Last          = 4

STS_Fail          = 0
STS_Halt          = 1
STS_Step          = 2
STS_Trap          = 3
STS_Success       = 4
STS_StackOver     = 5
STS_DivideZero    = 6
STS_IllegalOpcode = 7

TRAP_GetChar      = 0
TRAP_PutChar      = 1
TRAP_PrintStr     = 2
TRAP_PrintInt     = 3
TRAP_PrintTab     = 4
TRAP_PrintNL      = 5
TRAP_PrintFlush   = 6

' must match memory base addresses in db_config.h
HUB_BASE		= $00000000	' must be zero
COG_BASE		= $10000000

' virtual machine opcodes
OP_HALT         = $00    ' halt
OP_BRT          = $01    ' branch on true
OP_BRTSC        = $02    ' branch on true (for short circuit booleans)
OP_BRF          = $03    ' branch on false
OP_BRFSC        = $04    ' branch on false (for short circuit booleans)
OP_BR           = $05    ' branch unconditionally
OP_NOT          = $06    ' logical negate top of stack
OP_NEG          = $07    ' negate
OP_ADD          = $08    ' add two numeric expressions
OP_SUB          = $09    ' subtract two numeric expressions
OP_MUL          = $0a    ' multiply two numeric expressions
OP_DIV          = $0b    ' divide two numeric expressions
OP_REM          = $0c    ' remainder of two numeric expressions
OP_BNOT         = $0d    ' bitwise not of two numeric expressions
OP_BAND         = $0e    ' bitwise and of two numeric expressions
OP_BOR          = $0f    ' bitwise or of two numeric expressions
OP_BXOR         = $10    ' bitwise exclusive or
OP_SHL          = $11    ' shift left
OP_SHR          = $12    ' shift right
OP_LT           = $13    ' less than
OP_LE           = $14    ' less than or equal to
OP_EQ           = $15    ' equal to
OP_NE           = $16    ' not equal to
OP_GE           = $17    ' greater than or equal to
OP_GT           = $18    ' greater than
OP_LIT          = $19    ' load literal
OP_SLIT         = $1a    ' load a short literal (-128 to 127)
OP_LOAD         = $1b    ' load a long from memory
OP_LOADB        = $1c    ' load a byte from memory
OP_STORE        = $1d    ' store a long in memory
OP_STOREB       = $1e    ' store a byte in memory
OP_LADDR        = $1f    ' load the address of a local variable
OP_INDEX        = $20    ' index into a vector
OP_CALL         = $21    ' call a function */
OP_FRAME        = $22    ' create a stack frame */
OP_RETURN       = $23    ' from a function */
OP_DROP         = $24    ' drop the top element of the stack
OP_DUP          = $25    ' duplicate the top element of the stack
OP_TUCK         = $26    ' a b -> b a b
OP_SWAP         = $27    ' swap the top to elements of the stack
OP_TRAP         = $28    ' invoke a trap handler
OP_SEND         = $29    ' send a message to an object
OP_DADDR        = $2a    ' load the address of something in data space
OP_PADDR        = $2b    ' load the address of a property value
OP_CLASS        = $2c    ' get the class of an object
OP_TRY          = $2d    ' enter a try block
OP_TRYEXIT      = $2e    ' exit a try block
OP_THROW        = $2f    ' throw an exception
OP_LAST         = $2f

DIV_OP          = 0
REM_OP          = 1

PUB start(params) | mbox, cog
  mbox := long[params][INIT_MBOX]
  long[mbox][MBOX_CMD] := 1
  cog := cognew(@_init, params)
  return poll(mbox)

PUB run(mbox, state)
  long[state][STATE_STEPPING] := 0
  long[mbox][MBOX_CMD] := VM_Continue

PUB single_step(mbox, state)
  long[state][STATE_STEPPING] := 1
  long[mbox][MBOX_CMD] := VM_Continue

PUB continue(mbox)
  long[mbox][MBOX_CMD] := VM_Continue

PUB poll(mbox)
  repeat while long[mbox][MBOX_CMD] <> 0
  return long[mbox][MBOX_ARG_STS]

PUB read_long(mbox, p_address)
  long[mbox][MBOX_ARG_STS] := p_address
  long[mbox][MBOX_CMD] := VM_ReadLong
  repeat while long[mbox][MBOX_CMD] <> 0
  return long[mbox][MBOX_ARG2_FCN]

PUB write_long(mbox, p_address, value)
  long[mbox][MBOX_ARG_STS] := p_address
  long[mbox][MBOX_ARG2_FCN] := value
  long[mbox][MBOX_CMD] := VM_WriteLong
  repeat while long[mbox][MBOX_CMD] <> 0

PUB read_byte(mbox, p_address)
  long[mbox][MBOX_ARG_STS] := p_address
  long[mbox][MBOX_CMD] := VM_ReadByte
  repeat while long[mbox][MBOX_CMD] <> 0
  return long[mbox][MBOX_ARG2_FCN]

DAT

        org 0
_init
        ' prepare to parse the initialization parameters
        mov     r1,par

        ' get the image address
        rdlong  image,r1
        add     r1,#4
        
        ' get the state vector
        rdlong  state_ptr,r1
        add     r1,#4

        ' get the mailbox addresses
        rdlong  cmd_ptr,r1
        add     r1,#4
        mov     arg_sts_ptr,cmd_ptr
        add     arg_sts_ptr,#4
        mov     arg2_fcn_ptr,arg_sts_ptr
        add     arg2_fcn_ptr,#4
        
        ' get the stack address and size
        rdlong  stack,r1
        add     r1,#4
        rdlong  r1,r1
        mov     stackTop,stack
        add     stackTop,r1
        mov     sp,stackTop
        mov     fp,sp

        ' get the memory area base addresses
        mov     r1,image
        rdlong  dbase,r1    ' get the data space address
        add     dbase,image
        add     r1,#8
        rdlong  cbase,r1    ' get the code space address
        add     cbase,image
        add     r1,#8
        rdlong  sbase,r1    ' get the string space address
        add     sbase,image
        add     r1,#8
        rdlong  pc,r1
        add     pc,cbase
        
        ' put the address of a halt in tos
        mov     tos,cbase
        add     tos,#1

        ' return the initial state
        call    #store_state

        ' start processing commands
        mov     r1,#STS_Step

end_command
        wrlong  r1,arg_sts_ptr

done_command
        wrlong  zero,cmd_ptr

get_command
        rdlong  r1,cmd_ptr
        tjz     r1,#get_command

parse_command
        cmp     r1,#_VM_Last wc,wz ' check for valid command
  if_a  jmp     #err_command
        add     r1,#cmd_table-1 
        jmp     r1                  ' jump to command handler

err_command
        mov     r1,#STS_Fail
        jmp     #end_command

cmd_table                           ' command dispatch table
        jmp     #_VM_Continue
        jmp     #_VM_ReadLong
        jmp     #_VM_WriteLong
        jmp     #_VM_ReadByte

_VM_Continue
        mov     r1,state_ptr
        rdlong  fp,r1           ' load fp
        add     r1,#4
        rdlong  sp,r1           ' load sp
        add     r1,#4
        rdlong  tos,r1          ' load tos
        add     r1,#4
        rdlong  pc,r1           ' load pc
        add     r1,#4
        rdlong  stepping,r1     ' load stepping
        jmp     #_start

_VM_ReadLong
        rdlong  r1,arg_sts_ptr
        call    #_read_long
        wrlong  r1,arg2_fcn_ptr
        mov     r1,#STS_Success
        jmp     #end_command

_VM_WriteLong
        rdlong  r1,arg_sts_ptr
        rdlong  r2,arg2_fcn_ptr
        call    #_write_long
        mov     r1,#STS_Success
        jmp     #end_command

_VM_ReadByte
        rdlong  r1,arg_sts_ptr
        call    #_read_byte
        wrlong  r1,arg2_fcn_ptr
        mov     r1,#STS_Success
        jmp     #end_command

store_state
        mov     r1,state_ptr
        wrlong  fp,r1       ' store fp
        add     r1,#4
        wrlong  sp,r1       ' store sp
        add     r1,#4
        wrlong  tos,r1      ' store tos
        add     r1,#4
        wrlong  pc,r1       ' store pc
        add     r1,#4
        wrlong  stepping,r1 ' store stepping
        add     r1,#4
        wrlong  stack,r1    ' store stack
        add     r1,#4
        wrlong  stackTop,r1 ' store stackTop (over stack size)
store_state_ret
        ret

_next   tjz     stepping,#_start

_step_end
        call    #store_state
        mov     r1,#STS_Step
        jmp     #end_command

_start  call    #get_code_byte
        cmp     r1,#OP_LAST wc,wz       ' check for valid opcode
  if_a  jmp     #illegal_opcode_err
        add     r1,#opcode_table 
        jmp     r1                      ' jump to command
        
opcode_table                            ' opcode dispatch table
        jmp     #_OP_HALT               ' halt
        jmp     #_OP_BRT                ' branch on true
        jmp     #_OP_BRTSC              ' branch on true (for short circuit booleans)
        jmp     #_OP_BRF                ' branch on false
        jmp     #_OP_BRFSC              ' branch on false (for short circuit booleans)
        jmp     #_OP_BR                 ' branch unconditionally
        jmp     #_OP_NOT                ' logical negate top of stack
        jmp     #_OP_NEG                ' negate
        jmp     #_OP_ADD                ' add two numeric expressions
        jmp     #_OP_SUB                ' subtract two numeric expressions
        jmp     #_OP_MUL                ' multiply two numeric expressions
        jmp     #_OP_DIV                ' divide two numeric expressions
        jmp     #_OP_REM                ' remainder of two numeric expressions
        jmp     #_OP_BNOT               ' bitwise not of two numeric expressions
        jmp     #_OP_BAND               ' bitwise and of two numeric expressions
        jmp     #_OP_BOR                ' bitwise or of two numeric expressions
        jmp     #_OP_BXOR               ' bitwise exclusive or
        jmp     #_OP_SHL                ' shift left
        jmp     #_OP_SHR                ' shift right
        jmp     #_OP_LT                 ' less than
        jmp     #_OP_LE                 ' less than or equal to
        jmp     #_OP_EQ                 ' equal to
        jmp     #_OP_NE                 ' not equal to
        jmp     #_OP_GE                 ' greater than or equal to
        jmp     #_OP_GT                 ' greater than
        jmp     #_OP_LIT                ' load a literal
        jmp     #_OP_SLIT               ' load a short literal (-128 to 127)
        jmp     #_OP_LOAD               ' load a long from memory
        jmp     #_OP_LOADB              ' load a byte from memory
        jmp     #_OP_STORE              ' store a long into memory
        jmp     #_OP_STOREB             ' store a byte into memory
        jmp     #_OP_LADDR              ' load the address of a local variable
        jmp     #_OP_INDEX              ' index into a vector
        jmp     #_OP_CALL               ' call a function
        jmp     #_OP_FRAME              ' push a frame onto the stack
        jmp     #_OP_RETURN             ' return from a function
        jmp     #_OP_DROP               ' drop the top element of the stack
        jmp     #_OP_DUP                ' duplicate the top element of the stack
        jmp     #_OP_TUCK               ' a b -> b a b
        jmp     #_OP_SWAP               ' swap the top to elements of the stack
        jmp     #_OP_TRAP               ' invoke a trap handler
        jmp     #_OP_SEND               ' send a message to an object
        jmp     #_OP_DADDR              ' load the address of something in data space
        jmp     #_OP_PADDR              ' load the address of a property value
        jmp     #_OP_CLASS              ' get the class of an object
        jmp     #_OP_TRY                ' enter a try block
        jmp     #_OP_TRYEXIT            ' exit a try block
        jmp     #_OP_THROW              ' throw an exception

_OP_HALT               ' halt
        call    #store_state
        mov     r1,#STS_Halt
	    jmp	#end_command

_OP_BRT                ' branch on true
        tjnz    tos,#take_branch
        call    #pop_tos
        add     pc,#2
        jmp     #_next

_OP_BRTSC              ' branch on true (for short circuit booleans)
        tjnz    tos,#take_branch_sc
        call    #pop_tos
        add     pc,#2
        jmp     #_next

_OP_BRF                ' branch on false
        tjz     tos,#take_branch
        call    #pop_tos
        add     pc,#2
        jmp     #_next

_OP_BRFSC              ' branch on false (for short circuit booleans)
        tjz     tos,#take_branch_sc
        call    #pop_tos
        add     pc,#2
        jmp     #_next

take_branch
        call    #pop_tos
take_branch_sc

_OP_BR                 ' branch unconditionally
        call    #imm16
        adds    pc,r1
        jmp     #_next

_OP_NOT                ' logical negate top of stack
        cmp     tos,#0 wz
   if_z mov     tos,#1
  if_nz mov     tos,#0
        jmp     #_next
        
_OP_NEG                ' negate
        neg     tos,tos
        jmp     #_next
        
_OP_ADD                ' add two numeric expressions
        call    #pop_r1
        adds    tos,r1
        jmp     #_next
        
_OP_SUB                ' subtract two numeric expressions
        call    #pop_r1
        subs    r1,tos
        mov     tos,r1
        jmp     #_next
        
_OP_MUL                ' multiply two numeric expressions
        call    #pop_r1
        jmp     #fast_mul
        
_OP_DIV                ' divide two numeric expressions
        cmp     tos,#0 wz
   if_z jmp     #divide_by_zero_err
        call    #pop_r1
        mov     div_flags,#DIV_OP
        jmp     #fast_div

_OP_REM                ' remainder of two numeric expressions
        cmp     tos,#0 wz
   if_z jmp     #divide_by_zero_err
        call    #pop_r1
        mov     div_flags,#REM_OP
        jmp     #fast_div

_OP_BNOT               ' bitwise not of two numeric expressions
        xor     tos,allOnes
        jmp     #_next
        
_OP_BAND               ' bitwise and of two numeric expressions
        call    #pop_r1
        and     tos,r1
        jmp     #_next
        
_OP_BOR                ' bitwise or of two numeric expressions
        call    #pop_r1
        or      tos,r1
        jmp     #_next
        
_OP_BXOR               ' bitwise exclusive or
        call    #pop_r1
        xor     tos,r1
        jmp     #_next
        
_OP_SHL                ' shift left
        call    #pop_r1
        shl     r1,tos
        mov     tos,r1
        jmp     #_next
        
_OP_SHR                ' shift right
        call    #pop_r1
        shr     r1,tos
        mov     tos,r1
        jmp     #_next
        
_OP_LT                 ' less than
        call    #pop_r1
        cmps    r1,tos wz,wc
   if_b mov     tos,#1
  if_ae mov     tos,#0
        jmp     #_next
        
_OP_LE                 ' less than or equal to
        call    #pop_r1
        cmps    r1,tos wz,wc
  if_be mov     tos,#1
  if_a  mov     tos,#0
        jmp     #_next
        
_OP_EQ                 ' equal to
        call    #pop_r1
        cmps    r1,tos wz
   if_e mov     tos,#1
  if_ne mov     tos,#0
        jmp     #_next
        
_OP_NE                 ' not equal to
        call    #pop_r1
        cmps    r1,tos wz
  if_ne mov     tos,#1
   if_e mov     tos,#0
        jmp     #_next
        
_OP_GE                 ' greater than or equal to
        call    #pop_r1
        cmps    r1,tos wz,wc
  if_ae mov     tos,#1
   if_b mov     tos,#0
        jmp     #_next
        
_OP_GT                 ' greater than
        call    #pop_r1
        cmps    r1,tos wz,wc
   if_a mov     tos,#1
  if_be mov     tos,#0
        jmp     #_next
        
_OP_LIT                ' load a literal
        call    #push_tos
        call    #imm32
        mov     tos,r1
        jmp     #_next

_OP_SLIT               ' load a short literal (-128 to 127)
        call    #push_tos
        call    #get_code_byte
        shl     r1,#24
        sar     r1,#24
        mov     tos,r1
        jmp     #_next

_OP_LOAD               ' load a long from memory
        mov     r1,tos
        call    #_read_long
        mov     tos,r1
        jmp     #_next
        
_OP_LOADB              ' load a byte from memory
        mov     r1,tos
        call    #_read_byte
        mov     tos,r1
        jmp     #_next

_OP_STORE              ' store a long into memory
        call    #pop_r1
        mov     r2,tos
        call    #_write_long
        jmp     #_next
        
_OP_STOREB             ' store a byte into memory
        call    #pop_r1
        mov     r2,tos
        call    #_write_byte
        jmp     #_next

_OP_LADDR              ' load a local variable relative to the frame pointer
        call    #push_tos
        call    #get_code_byte
        shl     r1,#24
        sar     r1,#22
        add     r1,fp
        mov     tos,r1
        jmp     #_next
        
_OP_INDEX               ' index into a vector
        call    #pop_r1
        shl     tos,#2
        add     tos,r1
        jmp     #_next
        
_OP_CALL                ' call a function
        add     pc,#1   '   skip past the argument count
        mov     r1,tos
        mov     tos,pc
        mov     pc,r1
        add     pc,cbase
        jmp     #_next

_OP_FRAME               ' push a frame onto the stack
        call    #get_code_byte
        mov     r2,fp
        mov     fp,sp
        shl     r1,#2
        sub     sp,r1
        cmp     sp,stack wc,wz
  if_b  jmp     #stack_overflow_err
        wrlong  r2,sp
        jmp     #_next

_OP_RETURN              ' return from a function
        rdlong  pc,sp
        add     sp,#4
        rdlong  r1,sp
        mov     sp,fp
        mov     r2,pc
        sub     r2,#1
        rdbyte  r2,r2
        shl     r2,#2
        add     sp,r2
        mov     fp,r1
        jmp     #_next

_OP_DROP               ' drop the top element of the stack
        rdlong  tos,sp
        add     sp,#4
        jmp     #_next

_OP_DUP                ' duplicate the top element of the stack
        call    #push_tos
        jmp     #_next

_OP_TUCK               ' a b -> b a b
        rdlong  r1,sp
        call    #push_tos
        mov     tos,r1
        jmp     #_next

_OP_SWAP               ' swap the top to elements of the stack
        rdlong  r1,sp
        wrlong  tos,sp
        mov     tos,r1
        jmp     #_next

_OP_TRAP
        call    #get_code_byte
        wrlong  r1,arg2_fcn_ptr
        call    #store_state
        mov     r1,#STS_Trap
        jmp     #end_command

_OP_SEND               ' send a message to an object
        jmp     #_next

_OP_DADDR              ' load the address of something in data space
        add     tos,dbase
        jmp     #_next

_OP_PADDR              ' load the address of a property value
        jmp     #_next

_OP_CLASS              ' get the class of an object
        add     tos,dbase
        rdlong  tos,tos
        jmp     #_next

_OP_TRY                ' enter a try block
        jmp     #_next

_OP_TRYEXIT            ' exit a try block
        jmp     #_next

_OP_THROW              ' throw an exception
        jmp     #_next

imm16
        call    #get_code_byte  ' bits 15:8
        mov     r2,r1
        shl     r2,#24
        sar     r2,#16
        call    #get_code_byte  ' bits 7:0
        or      r1,r2
imm16_ret
        ret

imm32
        call    #get_code_byte  ' bits 31:24
        mov     r2,r1
        shl     r2,#8
        call    #get_code_byte  ' bits 16:23
        or      r2,r1
        shl     r2,#8
        call    #get_code_byte  ' bits 15:8
        or      r2,r1
        shl     r2,#8
        call    #get_code_byte  ' bits 7:0
        or      r1,r2
imm32_ret
        ret

push_tos
        sub     sp,#4
        cmp     sp,stack wc,wz
  if_b  jmp     #stack_overflow_err
        wrlong  tos,sp
push_tos_ret
        ret
        
pop_tos
        rdlong  tos,sp
        add     sp,#4
pop_tos_ret
        ret

pop_r1
        rdlong  r1,sp
        add     sp,#4
pop_r1_ret
        ret

illegal_opcode_err
        mov     r1,#STS_IllegalOpcode
        jmp     #end_command

stack_overflow_err
        mov     r1,#STS_StackOver
        jmp     #end_command

divide_by_zero_err
        mov     r1,#STS_DivideZero
        jmp     #end_command

cog_start        long COG_BASE	        'Start of COG access window in VM memory space

' input:
'    pc is address
' output:
'    r1 is value
'    pc is incremented
get_code_byte           rdbyte  r1, pc
                        add     pc, #1
get_code_byte_ret       ret

' Input:
'    r1 is address
' output:
'    r1 is value
_read_byte              rdbyte  r1, r1
_read_byte_ret          ret

' input:
'    r1 is address
' output:
'    r1 is value
_read_long              cmp     r1, cog_start wc        'Check for COG memory access
              if_nc     jmp     #read_cog_long

                        rdlong  r1, r1
                        jmp     #_read_long_ret

read_cog_long           shr     r1, #2
                        movs    :rcog, r1
                        nop
:rcog                   mov     r1, 0-0
_read_long_ret          ret

' Input:
'    r1 is address
'    r2 is value
' trashes:
'    r1
_write_byte             wrbyte  r2, r1
_write_byte_ret         ret

' input:
'    r1 is address
'    r2 is value
' trashes:
'    r1
_write_long             cmp     r1, cog_start wc        'Check for COG memory access
              if_nc     jmp     #write_cog_long

                        wrlong  r2, r1
                        jmp     #_write_long_ret

write_cog_long          shr     r1, #2
                        movd    :wcog, r1
                        nop
:wcog                   mov     0-0, r2
_write_long_ret         ret

' constants
zero                    long    0
allOnes                 long    $ffff_ffff
dstinc                  long    1<<9    ' increment for the destination field of an instruction

' vm mailbox variables
cmd_ptr                 long    0
arg_sts_ptr             long    0
arg2_fcn_ptr            long    0
state_ptr               long    0

fast_mul                ' tos * r1
                        ' account for sign
                        abs     tos, tos        wc
                        negc    r1, r1
                        abs     r1, r1          wc
                        ' make r3 the smaller of the 2 unsigned parameters
                        mov     r3, tos
                        max     r3, r1
                        min     r1, tos
                        ' correct the sign of the adder
                        negc    r1, r1
                        ' my accumulator
                        mov     tos, #0
                        ' do the work
:mul_loop               shr     r3, #1          wc,wz   ' get the low bit of r3
        if_c            add     tos, r1                 ' if it was a 1, add adder to accumulator
                        shl     r1, #1                  ' shift the adder left by 1 bit
        if_nz           jmp     #:mul_loop              ' continue as long as there are no more 1's
                        jmp     #_next

{{==    div_flags: xxxx_invert result_store remainder   ==}}
{{==    NOTE: Caller must not allow tos == 0!!!!        ==}}
div_flags               long    0

fast_div                ' tos = r1 / tos
                        ' handle the signs, and check for a 0 divisor
                        and     div_flags, #1   wz      ' keep only the 0 bit, and remember if it's a 0
                        abs     r2, tos         wc
             if_z_and_c or      div_flags, #2           ' data was negative, and we're looking for quotient, so set bit 1 hi
                        abs     r1, r1          wc
              if_c      xor     div_flags, #2           ' tos was negative, invert bit 1 (quotient or remainder)
                        ' align the divisor to the leftmost bit
                        neg     r3, #1          wc      ' count how many times we shift (negative)
:align_loop             rcl     r2, #1          wc      ' left shift the divisior, marking when we hit a 1
              if_nc     djnz    r3, #:align_loop        ' the divisior MUST NOT BE 0
                        rcr     r2, #1                  ' restore the 1 bit we just nuked
                        neg     r3, r3                  ' how many times did we shift? (we started at -1 and counted down)
                        ' perform the division
                        mov     tos, #0
:div_loop               cmpsub  r1, r2          wc      ' does the divisor fit into the dividend?
                        rcl     tos, #1                 ' if it did, store a one while shifting left
                        shr     r2, #1                  '
                        djnz    r3, #:div_loop
                        ' correct the sign
                        shr     div_flags, #1   wc,wz
              if_c      mov     tos, r1                 ' user wanted the remainder, not the quotient
                        negnz   tos, tos                ' need to invert the result
                        jmp     #_next

' adapted from Heater's ZOG

' base addresses
image       long    0
dbase       long    0
cbase       long    0
sbase       long    0

' registers for use by inline code
t1          long    0
t2          long    0
t3          long    0
t4          long    0
tos         long    0
sp          long    0
fp          long    0
pc          long    0
efp         long    0

' virtual machine registers
stack       long    0
stackTop    long    0
stepping    long    0

' temporaries used by the VM instructions
r1          long    0
r2          long    0
r3          long    0

            fit     496
