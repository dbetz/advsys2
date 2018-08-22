#define USE_JCACHE_MEMORY

CON

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
OP_TUCK         = $26    ' a b -> b a
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

OBJ
  int : "vm_interface"
  cache : "cache_interface"

PUB dummy

DAT
        org 0
_init
        jmp     #_init1

' registers for use by inline code
t1          long    0
t2          long    0
t3          long    0
t4          long    0
tos         long    0
base        long    0
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

_init1
        ' prepare to parse the initialization parameters
        mov     r1,par

        ' get the memory base address (only for hub mode)
        rdlong  base,r1
        add     r1,#4

        ' get the state vector
        rdlong  state_ptr,r1
        add     r1,#4

        ' get the mailbox address
        rdlong  cmd_ptr,r1
        add     r1,#4
        mov     arg_sts_ptr,cmd_ptr
        add     arg_sts_ptr,#4
        mov     arg2_fcn_ptr,arg_sts_ptr
        add     arg2_fcn_ptr,#4

#ifdef USE_JCACHE_MEMORY
        ' get the cache mailbox address
        rdlong  cache_mboxcmd,r1
        add     r1,#4
        mov     cache_mboxdat,cache_mboxcmd
        add     cache_mboxdat,#4
        rdlong  cache_linemask,r1
#endif

        ' return the initial state
        call    #store_state

        ' start processing commands
        mov     r1,#int#STS_Step

end_command
        wrlong  r1,arg_sts_ptr

done_command
        wrlong  zero,cmd_ptr

get_command
        rdlong  r1,cmd_ptr
        tjz     r1,#get_command

parse_command
        cmp     r1,#int#_VM_Last wc,wz ' check for valid command
  if_a  jmp     #err_command
        add     r1,#cmd_table-1 
        jmp     r1                  ' jump to command handler

err_command
        mov     r1,#int#STS_Fail
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
        add     r1,#4
        rdlong  stack,r1        ' load stack
        add     r1,#4
        rdlong  stackTop,r1     ' load the stack size
        add     stackTop,stack
        jmp     #_start

_VM_ReadLong
        rdlong  r1,arg_sts_ptr
        call    #_read_long
        wrlong  r1,arg2_fcn_ptr
        mov     r1,#int#STS_Success
        jmp     #end_command

_VM_WriteLong
        rdlong  r1,arg_sts_ptr
        rdlong  r2,arg2_fcn_ptr
        call    #_write_long
        mov     r1,#int#STS_Success
        jmp     #end_command

_VM_ReadByte
        rdlong  r1,arg_sts_ptr
        call    #_read_byte
        wrlong  r1,arg2_fcn_ptr
        mov     r1,#int#STS_Success
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
store_state_ret
        ret

_next   tjz     stepping,#_start

_step_end
        call    #store_state
        mov     r1,#int#STS_Step
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
        jmp     #_OP_TUCK               ' a b -> b a
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
        mov     r1,#int#STS_Halt
	jmp	#end_command

_OP_BRT                ' branch on true
        tjnz    tos,#take_branch
        call    #pop_tos
        add     pc,#4
        jmp     #_next

_OP_BRTSC              ' branch on true (for short circuit booleans)
        tjnz    tos,#take_branch_sc
        call    #pop_tos
        add     pc,#4
        jmp     #_next

_OP_BRF                ' branch on false
        tjz     tos,#take_branch
        call    #pop_tos
        add     pc,#4
        jmp     #_next

_OP_BRFSC              ' branch on false (for short circuit booleans)
        tjz     tos,#take_branch_sc
        call    #pop_tos
        add     pc,#4
        jmp     #_next

take_branch
        call    #pop_tos
take_branch_sc

_OP_BR                 ' branch unconditionally
        call    #imm32
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
        call    #pop_t1
        adds    tos,r1
        jmp     #_next
        
_OP_SUB                ' subtract two numeric expressions
        call    #pop_t1
        subs    r1,tos
        mov     tos,r1
        jmp     #_next
        
_OP_MUL                ' multiply two numeric expressions
        call    #pop_t1
        jmp     #fast_mul
        
_OP_DIV                ' divide two numeric expressions
        cmp     tos,#0 wz
   if_z jmp     #divide_by_zero_err
        call    #pop_t1
        mov     div_flags,#DIV_OP
        jmp     #fast_div

_OP_REM                ' remainder of two numeric expressions
        cmp     tos,#0 wz
   if_z jmp     #divide_by_zero_err
        call    #pop_t1
        mov     div_flags,#REM_OP
        jmp     #fast_div

_OP_BNOT               ' bitwise not of two numeric expressions
        xor     tos,allOnes
        jmp     #_next
        
_OP_BAND               ' bitwise and of two numeric expressions
        call    #pop_t1
        and     tos,r1
        jmp     #_next
        
_OP_BOR                ' bitwise or of two numeric expressions
        call    #pop_t1
        or      tos,r1
        jmp     #_next
        
_OP_BXOR               ' bitwise exclusive or
        call    #pop_t1
        xor     tos,r1
        jmp     #_next
        
_OP_SHL                ' shift left
        call    #pop_t1
        shl     r1,tos
        mov     tos,r1
        jmp     #_next
        
_OP_SHR                ' shift right
        call    #pop_t1
        shr     r1,tos
        mov     tos,r1
        jmp     #_next
        
_OP_LT                 ' less than
        call    #pop_t1
        cmps    r1,tos wz,wc
   if_b mov     tos,#1
  if_ae mov     tos,#0
        jmp     #_next
        
_OP_LE                 ' less than or equal to
        call    #pop_t1
        cmps    r1,tos wz,wc
  if_be mov     tos,#1
  if_a  mov     tos,#0
        jmp     #_next
        
_OP_EQ                 ' equal to
        call    #pop_t1
        cmps    r1,tos wz
   if_e mov     tos,#1
  if_ne mov     tos,#0
        jmp     #_next
        
_OP_NE                 ' not equal to
        call    #pop_t1
        cmps    r1,tos wz
  if_ne mov     tos,#1
   if_e mov     tos,#0
        jmp     #_next
        
_OP_GE                 ' greater than or equal to
        call    #pop_t1
        cmps    r1,tos wz,wc
  if_ae mov     tos,#1
   if_b mov     tos,#0
        jmp     #_next
        
_OP_GT                 ' greater than
        call    #pop_t1
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
        call    #pop_t1
        mov     r2,r1
        mov     r1,tos
        call    #_write_long
        call    #pop_tos
        jmp     #_next
        
_OP_STOREB             ' store a byte into memory
        call    #pop_t1
        mov     r2,r1
        mov     r1,tos
        call    #_write_byte
        call    #pop_tos
        jmp     #_next

_OP_LADDR              ' load a local variable relative to the frame pointer
        call    #push_tos
        call    #lref
        jmp     #_next
        
_OP_INDEX               ' index into a vector
        call    #pop_t1
        shl     tos,#2
        add     tos,r1
        jmp     #_next
        
_OP_CALL                ' call a function
        mov     r1,tos
        mov     tos,pc
        mov     pc,r1
        jmp     #_next

_OP_FRAME               ' push a frame onto the stack
        mov     r2,fp
        mov     fp,sp
        call    #get_code_byte
        shl     r1,#2
        sub     sp,r1
        cmp     sp,stack wc,wz
   if_b jmp     #stack_overflow_err
        mov     r1,fp
        sub     r1,#4
        wrlong  r2,r1       ' store the old fp
        jmp     #_next

_OP_RETURN              ' return from a function
        mov     pc,tos
        call    #pop_tos
        jmp     #_next

_OP_DROP               ' drop the top element of the stack
        rdlong  tos,sp
        add     sp,#4
        jmp     #_next

_OP_DUP                ' duplicate the top element of the stack
        call    #push_tos
        jmp     #_next

_OP_TUCK               ' a b -> b a
        jmp     #_next

_OP_SWAP               ' swap the top to elements of the stack
        jmp     #_next

_OP_TRAP
        call    #get_code_byte
        wrlong  r1,arg2_fcn_ptr
        call    #store_state
        mov     r1,#int#STS_Trap
        jmp     #end_command

_OP_SEND               ' send a message to an object
        jmp     #_next

_OP_DADDR              ' load the address of something in data space
        jmp     #_next

_OP_PADDR              ' load the address of a property value
        jmp     #_next

_OP_CLASS              ' get the class of an object
        jmp     #_next

_OP_TRY                ' enter a try block
        jmp     #_next

_OP_TRYEXIT            ' exit a try block
        jmp     #_next

_OP_THROW              ' throw an exception
        jmp     #_next

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

lref
        call    #get_code_byte
        shl     r1,#24
        sar     r1,#22
        add     r1,fp
lref_ret
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

pop_t1
        rdlong  r1,sp
        add     sp,#4
pop_t1_ret
        ret

illegal_opcode_err
        mov     r1,#int#STS_IllegalOpcode
        jmp     #end_command

stack_overflow_err
        mov     r1,#int#STS_StackOver
        jmp     #end_command

divide_by_zero_err
        mov     r1,#int#STS_DivideZero
        jmp     #end_command
external_start   long int#EXTERNAL_BASE     'Start of external memory access window in VM memory space
cog_start        long int#COG_BASE	        'Start of COG access window in VM memory space

' input:
'    pc is address
' output:
'    r1 is value
'    pc is incremented
get_code_byte           mov     r1, pc
                        add     pc, #1
                        ' fall through

' Input:
'    r1 is address
' output:
'    r1 is value
_read_byte              cmp     r1, external_start wc    'Check for normal memory access
              if_nc     jmp     #read_external_byte

read_hub_byte           add     r1, base
                        rdbyte  r1, r1
                        jmp     #_read_byte_ret

read_external_byte
#ifdef USE_JCACHE_MEMORY
                        call    #cache_read
                        rdbyte  r1, memp
#endif
get_code_byte_ret
_read_byte_ret          ret

' input:
'    r1 is address
' output:
'    r1 is value
_read_long              cmp     r1, external_start wc   'Check for normal memory access
              if_nc     jmp     #read_external_long

read_hub_long           cmp     r1, cog_start wc        'Check for COG memory access
              if_nc     jmp     #read_cog_long

                        add     r1, base
                        rdlong  r1, r1
                        jmp     #_read_long_ret

read_cog_long           shr     r1, #2
                        movs    :rcog, r1
                        nop
:rcog                   mov     r1, 0-0
                        jmp     #_read_long_ret

read_external_long
#ifdef USE_JCACHE_MEMORY
                        call    #cache_read
                        rdlong  r1, memp
#endif
_read_long_ret          ret

' Input:
'    r1 is address
'    r2 is value
' trashes:
'    r1
_write_byte             cmp     r1, external_start wc    'Check for normal memory access
              if_nc     jmp     #write_external_byte

write_hub_byte          add     r1, base
                        wrbyte  r2, r1
                        jmp     #_write_byte_ret

write_external_byte
#ifdef USE_JCACHE_MEMORY
                        call    #cache_write
                        wrbyte  r2, memp
#endif
_write_byte_ret         ret

' input:
'    r1 is address
'    r2 is value
' trashes:
'    r1
_write_long             cmp     r1, external_start wc   'Check for normal memory access
              if_nc     jmp     #write_external_long

write_hub_long          cmp     r1, cog_start wc        'Check for COG memory access
              if_nc     jmp     #write_cog_long

                        add     r1, base
                        wrlong  r2, r1
                        jmp     #_write_long_ret

write_cog_long          shr     r1, #2
                        movd    :wcog, r1
                        nop
:wcog                   mov     0-0, r2
                        jmp     #_write_long_ret

write_external_long
#ifdef USE_JCACHE_MEMORY
                        call    #cache_write
                        wrlong  r2, memp
#endif
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

#ifdef USE_JCACHE_MEMORY

cache_linemask          long    0
cache_mboxcmd           long    0
cache_mboxdat           long    0
temp                    long    0
memp                    long    0
cacheaddr               long    0
cacheptr                long    0

cache_write             mov     memp, r1                    'save address for index
                        andn    r1, #cache#CMD_MASK         'ensure a write is not a read
                        or      r1, #cache#WRITE_CMD
                        jmp     #cache_access

cache_read              mov     temp, r1                    'ptr + cache_mboxdat = hub address of byte to load
                        andn    temp, cache_linemask
                        cmp     cacheaddr,temp wz           'if cacheaddr == addr, just pull form cache
            if_ne       jmp     #:next                      'memp gets overwriteen on a miss
                        mov     memp, r1                    'ptr + cache_mboxdat = hub address of byte to load
                        and     memp, cache_linemask
                        add     memp, cacheptr              'add ptr to memp to get data address
                        jmp     #cache_read_ret
:next
                        mov     memp, r1                    'save address for index
                        or      r1, #cache#READ_CMD         'read must be 3 to avoid needing andn addr,#cache#CMD_MASK

cache_access            ' if cacheaddr <> addr, load new cache line
                        wrlong  r1, cache_mboxcmd
                        mov     cacheaddr,r1                'Save new cache address. it's free time here
                        andn    cacheaddr,cache_linemask    'Kill command bits in free time
:waitres                rdlong  temp, cache_mboxcmd wz
            if_nz       jmp     #:waitres
                        and     memp, cache_linemask        'memp is index into buffer
                        rdlong  cacheptr, cache_mboxdat     'Get new buffer
                        add     memp, cacheptr              'memp is now HUB buf address of data to read
cache_read_ret
cache_write_ret
                        ret

#endif

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

                        fit     496
