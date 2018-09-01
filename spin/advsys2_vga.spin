''
''        Author: David Betz
''        Based on code by: Marko Lukat and Jim Goodpaster
'' Last modified: 2018/09/01
''       Version: 0.1
''
CON
  columns  = driver#res_x / 16
  rows     = driver#res_y / 32
  bcnt     = columns * rows

  rows_raw = (driver#res_y + 32 - 1) / 32
  bcnt_raw = columns * rows_raw

  XSTART = 1
  XEND = columns - 2
  YSTART = 1
  YEND = rows - 2
    
  BW = $37

OBJ
  driver: "vga.driver"

VAR
  long  link[driver#res_m]                              ' driver mailbox

  word  scrn[bcnt_raw / 2]                              ' screen buffer (2n aligned)
  word  indx[bcnt_raw / 2]                              ' colour buffer (2n aligned)

  byte  x, y                                            ' cursor position
 
PUB null
'' This is not a top level object.

PUB init(config) : c

  link{0} := @scrn{0}                                   ' initial screen and
  link[2] := @indx{0}                                   ' colour index buffer (default palette)
  link[3] := config                                     ' lock/vgrp/vpin

  x := XSTART
  y := YSTART

  return driver.init(@link{0})                          ' video driver and pixel generator
  
PUB clear
  bytefill(@indx{0},  BW, bcnt_raw)
  bytefill(@scrn{0}, " ", bcnt_raw)
      
PUB tx(c)
  if (c == $0a)
    crlf
  else
    indx.byte[y * columns + x] := BW
    scrn.byte[y * columns + x] := c
    if ++x > XEND                                       
      x := XSTART
      crlf
    
PUB str(addr)
  repeat strsize(addr)
    tx(byte[addr++])
    
PUB hex(value, digits)

'' Print a hexadecimal number

  value <<= (8 - digits) << 2
  repeat digits
    tx(lookupz((value <-= 4) & $F : "0".."9", "A".."F"))

PUB dec(value) | i, xx

'' Print a decimal number

  xx := value == NEGX                                   'Check for max negative
  if value < 0
    value := ||(value+xx)                               'If negative, make positive; adjust for max negative
    tx("-")                                             'and output sign

  i := 1_000_000_000                                    'Initialize divisor

  repeat 10                                             'Loop for 10 digits
    if value => i                                                               
      tx(value / i + "0" + xx*(i == 1))                 'If non-zero digit, output digit; adjust for max negative
      value //= i                                       'and digit from value
      result~~                                          'flag non-zero found
    elseif result or i == 1
      tx("0")                                           'If zero digit (or only digit) output it
    i /= 10                                             'Update divisor

PUB crlf
  x := XSTART
  if ++y > YEND
    y := YEND
    wordmove(@indx.byte[columns], @indx.byte[constant(columns*2)], constant((bcnt_raw - columns*2) / 2))
    wordmove(@scrn.byte[columns], @scrn.byte[constant(columns*2)], constant((bcnt_raw - columns*2) / 2))
    wordfill(@indx.byte[constant(bcnt_raw - columns*2)], BW * $0101, constant(columns / 2))
    wordfill(@scrn.byte[constant(bcnt_raw - columns*2)], $2020, constant(columns / 2))
