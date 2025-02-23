
  @ tells the assembler to use the unified instruction set
  .syntax unified
  @ this directive selects the thumb (16-bit) instruction set
  .thumb
  @ this directive specifies the following symbol is a thumb-encoded function
  .thumb_func
  @ align the next variable or instruction on a 2-byte boundary
  .balign 2
  @ make the symbol visible to the linker
  .global strcmp_
  @ marks the symbol as being a function name
  .type strcmp_, STT_FUNC
strcmp_:
@ r0 = pointer to the beginning of str1
@ r1 = pointer to the beginning of str2
@ returns str1[n]-str2[n], where n is the point where the strings differ

  subs  r2, r0, r1
  beq   no_pop_exit         @ if pointers are the same, exit
  push  {r4, r5, r6}

  lsls  r2, r2, #30         @ look at the 2 least-significant bits
  bne   byte_compare        @ if str1_addr - str2_addr is not a multiple of 4, compare bytewise
  lsls  r2, r0, #30         @ look at the 2 least-significant bits of str1_addr
  beq   word_compare        @ if aligned to 4 bytes, we can compare wordwise
@ if str1_addr - str2_addr is a multiple of 4, but str1_addr is not 4-byte aligned, it is possible to compare
@ wordwise once we achieve 4-byte alignment

@ compare bytewise until aligned
align_to_word:
  ldrb  r2, [r0]
  ldrb  r3, [r1]
  adds  r0, r0, #1
  adds  r1, r1, #1
  cmp   r2, #0
  beq   sub_and_exit
  cmp   r2, r3
  bne   sub_and_exit
  lsls  r4, r0, #30
  bne   align_to_word

word_compare:
  ldr   r5, =#0x01010101     @ put 0x01010101 into r5 to later check for nulls
  lsls  r6, r5, #7          @ put 0x80808080 into r6 to later check for nulls
wc_loop:
  ldm   r0!, {r2}           @ load the word r0 points to into r2, and increment r0 by 4 after
  ldm   r1!, {r3}           @ load the word r1 points to into r3, and increment r1 by 4 after
  
  @ test if r2 contains any null bytes (0x00) using the algorithm found here: https://graphics.stanford.edu/~seander/bithacks.html#HasLessInWord
  subs  r4, r2, r5          @ do r2 - 0x01010101 and store it in r4
  bics  r4, r4, r2          @ do r4 & ~(r2) and store it in r4
  ands  r4, r4, r6          @ do r4 & 0x80808080 and store it in r4
  bne   find_different_byte @ if there were any null bytes in r2, r4 will be nonzero in the null-byte byte
  cmp   r2, r3
  beq   wc_loop             @ the two words matched, now loop and do it again   

find_different_byte:
  uxtb  r0, r2
  uxtb  r1, r3
  subs  r0, r0, r1
  lsls  r1, r4, #24
  orrs  r1, r0, r1
  bne   exit
  uxth  r0, r2
  uxth  r1, r3
  subs  r0, r0, r1
  asrs  r0, r0, #8
  lsls  r1, r4, #16
  orrs  r1, r0, r1
  bne   exit
  lsls  r0, r2, #8
  lsrs  r0, r0, #8
  lsls  r1, r3, #8
  lsrs  r1, r1, #8
  subs  r0, r0, r1
  asrs  r0, r0, #16
  lsls  r1, r4, #8
  orrs  r1, r0, r1
  bne   exit
  lsrs  r0, r2, #24
  lsrs  r1, r3, #24
  subs  r0, r0, r1
  b     exit


@ find_different_byte:
@   rev   r2, r2
@   rev   r3, r3
@   lsrs  r1, r3, #24
@   lsrs  r0, r2, #24
@   beq   sub_and_exit
@   subs  r0, r0, r1
@   bne   exit
@   lsrs  r1, r3, #16
@   lsrs  r0, r2, #16
@   beq   sub_and_exit
@   subs  r0, r0, r1
@   bne   exit
@   lsrs  r0, r2, #8
@   lsrs  r1, r3, #8
@   subs  r0, r0, r1
@   bne   exit
@   b     sub_and_exit

byte_compare:
  ldrb  r2, [r0]
  ldrb  r3, [r1]
  adds  r0, r0, #1
  adds  r1, r1, #1
  cmp   r2, #0
  beq   sub_and_exit
  cmp   r2, r3
  beq   byte_compare

sub_and_exit:
  subs  r0, r2, r3

exit:
  pop   {r4, r5, r6}        @ restore previous value of r4 & r5
no_pop_exit:
  bx    lr                  @ exit function

  .size strcmp_, . - strcmp_

