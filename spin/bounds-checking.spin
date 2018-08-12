_OP_VREF               ' load an element of a vector
        rdlong  arg0,sp
        sub     sp,#4
        call    #get_vector
        cmp     tos,arg1 wc,wz
   if_a jmp     #index_err
        cmp     tos,#0 wc,wz
  if_be jmp     #index_err
        shl     tos,#2
        add     tos,arg0
        rdlong  tos,tos
        jmp     #_next

_OP_VSET               ' set an element of a vector
        rdlong  t0,sp
        sub     sp,#4
        rdlong  arg0,sp
        sub     sp,#4
        call    #get_vector
        cmp     tos,t0 wc,wz
   if_a jmp     #index_err
        cmp     t0,#0 wc,wz
  if_be jmp     #index_err
        shl     t0,#2
        add     t0,arg0
        wrlong  tos,t0
        rdlong  tos,sp
        sub     sp,#4
        jmp     #_next

