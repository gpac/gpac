
   AREA XVID,CODE,READONLY
   EXPORT XVID_ClearMatrix
   EXPORT XVID_MemCpy
   EXPORT interpolate8x8_halfpel_hv
   EXPORT interpolate8x8_halfpel_h
   EXPORT interpolate8x8_halfpel_v
   EXPORT transfer8x8_copy

;----------------------------
XVID_ClearMatrix

   mov r1, #0
   mov r2, #0
   mov r3, #0
   mov r12, #0
   stmia r0!, { r1, r2, r3, r12 }
   stmia r0!, { r1, r2, r3, r12 }
   stmia r0!, { r1, r2, r3, r12 }
   stmia r0!, { r1, r2, r3, r12 }
   stmia r0!, { r1, r2, r3, r12 }
   stmia r0!, { r1, r2, r3, r12 }
   stmia r0!, { r1, r2, r3, r12 }
   stmia r0!, { r1, r2, r3, r12 }

   stmia r0!, { r1, r2, r3, r12 }
   stmia r0!, { r1, r2, r3, r12 }
   stmia r0!, { r1, r2, r3, r12 }
   stmia r0!, { r1, r2, r3, r12 }
   stmia r0!, { r1, r2, r3, r12 }
   stmia r0!, { r1, r2, r3, r12 }
   stmia r0!, { r1, r2, r3, r12 }
   stmia r0!, { r1, r2, r3, r12 }

   mov pc, lr

;----------------------------
XVID_MemCpy

   stmfd sp!, { r4 - r6 }

   subs r2, r2, #16
   blt mc_no16
mc_loop16
                              ;the order of regs in {} is not important, now it's ordered as to avoid compiler warnings
   ldmia r1!, { r3 - r6 }
   stmia r0!, { r3 - r6 }
   subs r2, r2, #16
   bge mc_loop16
mc_no16
   adds r2, r2, #16
   beq mc_end
mc_loop4
   ldr r3, [r1], #4
   str r3, [r0], #4
   subs r2, r2, #4
   bne mc_loop4
mc_end

   ldmfd sp!, { r4 - r6 }
   mov pc, lr

;----------------------------
interpolate8x8_halfpel_hv

   stmfd sp!, { r4 - r9, r14 }

   tst r3, r3
   movne r14, #1
   moveq r14, #2

   mov r3, #8
ihhv_loop
   add r4, r1, r2
   ldrb r5, [r1, #8]
   ldrb r7, [r4, #8]
   add r5, r5, r7
                           ;7+8
   ldrb r6, [r1, #7]
   ldrb r7, [r4, #7]
   add r6, r6, r7
   add r7, r5, r6
   add r7, r7, r14
   mov r8, r7, asr #2
                           ;6+7
   ldrb r5, [r1, #6]
   ldrb r7, [r4, #6]
   add r5, r5, r7
   add r7, r5, r6
   add r7, r7, r14
   mov r7, r7, asr #2
   orr r8, r7, r8, asl #8
                           ;5+6
   ldrb r6, [r1, #5]
   ldrb r7, [r4, #5]
   add r6, r6, r7
   add r7, r5, r6
   add r7, r7, r14
   mov r7, r7, asr #2
   orr r8, r7, r8, asl #8
                           ;4+5
   ldrb r5, [r1, #4]
   ldrb r7, [r4, #4]
   add r5, r5, r7
   add r7, r5, r6
   add r7, r7, r14
   mov r7, r7, asr #2
   orr r8, r7, r8, asl #8

   str r8, [r0, #4]

                           ;3+4
   ldrb r6, [r1, #3]
   ldrb r7, [r4, #3]
   add r6, r6, r7
   add r7, r5, r6
   add r7, r7, r14
   mov r8, r7, asr #2
                           ;2+3
   ldrb r5, [r1, #2]
   ldrb r7, [r4, #2]
   add r5, r5, r7
   add r7, r5, r6
   add r7, r7, r14
   mov r7, r7, asr #2
   orr r8, r7, r8, asl #8
                           ;1+2
   ldrb r6, [r1, #1]
   ldrb r7, [r4, #1]
   add r6, r6, r7
   add r7, r5, r6
   add r7, r7, r14
   mov r7, r7, asr #2
   orr r8, r7, r8, asl #8
                           ;0+1
   ldrb r5, [r1, #0]
   ldrb r7, [r4, #0]
   add r5, r5, r7
   add r7, r5, r6
   add r7, r7, r14
   mov r7, r7, asr #2
   orr r8, r7, r8, asl #8

   str r8, [r0, #0]

   add r0, r0, r2
   add r1, r1, r2
   subs r3, r3, #1
   bne ihhv_loop

   ldmfd sp!, { r4 - r9, r14 }
   mov pc, lr

;----------------------------
interpolate8x8_halfpel_h

   stmfd sp!, { r4 - r6 }

   ands r3, r3, #1
   beq i_hp_h_no_round

   mov r6, #8
ihh_loop
   ldrb r12, [r1, #8]
                                 ;7+8
   ldrb r3, [r1, #7]
   add r4, r12, r3
   mov r4, r4, asr #1
                                 ;6+7
   ldrb r12, [r1, #6]
   add r5, r12, r3
   mov r5, r5, asr #1
   orr r4, r5, r4, asl #8
                                 ;5+6
   ldrb r3, [r1, #5]
   add r5, r12, r3
   mov r5, r5, asr #1
   orr r4, r5, r4, asl #8
                                 ;4+5
   ldrb r12, [r1, #4]
   add r5, r12, r3
   mov r5, r5, asr #1
   orr r4, r5, r4, asl #8

   str r4, [r0, #4]

                                 ;3+4
   ldrb r3, [r1, #3]
   add r4, r12, r3
   mov r4, r4, asr #1
                                 ;2+3
   ldrb r12, [r1, #2]
   add r5, r12, r3
   mov r5, r5, asr #1
   orr r4, r5, r4, asl #8
                                 ;1+2
   ldrb r3, [r1, #1]
   add r5, r12, r3
   mov r5, r5, asr #1
   orr r4, r5, r4, asl #8
                                 ;0+1
   ldrb r12, [r1, #0]
   add r5, r12, r3
   mov r5, r5, asr #1
   orr r4, r5, r4, asl #8

   str r4, [r0, #0]

   add r0, r0, r2
   add r1, r1, r2
   subs r6, r6, #1   
   bne ihh_loop
   b i_hp_h_end

i_hp_h_no_round

   mov r6, #8
ihh_loop1
   ldrb r12, [r1, #8]
                                 ;7+8
   ldrb r3, [r1, #7]
   add r4, r12, r3
   add r4, r4, #1
   mov r4, r4, asr #1
                                 ;6+7
   ldrb r12, [r1, #6]
   add r5, r12, r3
   add r5, r5, #1
   mov r5, r5, asr #1
   orr r4, r5, r4, asl #8
                                 ;5+6
   ldrb r3, [r1, #5]
   add r5, r12, r3
   add r5, r5, #1
   mov r5, r5, asr #1
   orr r4, r5, r4, asl #8
                                 ;4+5
   ldrb r12, [r1, #4]
   add r5, r12, r3
   add r5, r5, #1
   mov r5, r5, asr #1
   orr r4, r5, r4, asl #8

   str r4, [r0, #4]

                                 ;3+4
   ldrb r3, [r1, #3]
   add r4, r12, r3
   add r4, r4, #1
   mov r4, r4, asr #1
                                 ;2+3
   ldrb r12, [r1, #2]
   add r5, r12, r3
   add r5, r5, #1
   mov r5, r5, asr #1
   orr r4, r5, r4, asl #8
                                 ;1+2
   ldrb r3, [r1, #1]
   add r5, r12, r3
   add r5, r5, #1
   mov r5, r5, asr #1
   orr r4, r5, r4, asl #8
                                 ;0+1
   ldrb r12, [r1, #0]
   add r5, r12, r3
   add r5, r5, #1
   mov r5, r5, asr #1
   orr r4, r5, r4, asl #8

   str r4, [r0, #0]

   add r0, r0, r2
   add r1, r1, r2
   subs r6, r6, #1   
   bne ihh_loop1

i_hp_h_end
   ldmfd sp!, { r4 - r6 }
   mov pc, lr

;----------------------------
interpolate8x8_halfpel_v

   stmfd sp!, { r4 - r6 }

   ands r3, r3, #1
   beq i_hp_v_no_round

   mov r6, #8
ihv_loop
   add r5, r1, r2
                           ;3
   ldrb r12, [r1, #3]   
   ldrb r3, [r5, #3]
   add r4, r12, r3   
   mov r4, r4, asr #1
                           ;2
   ldrb r12, [r1, #2]   
   ldrb r3, [r5, #2]
   add r12, r12, r3   
   mov r12, r12, asr #1
   orr r4, r12, r4, asl #8
                           ;1
   ldrb r12, [r1, #1]   
   ldrb r3, [r5, #1]
   add r12, r12, r3   
   mov r12, r12, asr #1
   orr r4, r12, r4, asl #8
                           ;0
   ldrb r12, [r1, #0]   
   ldrb r3, [r5, #0]
   add r12, r12, r3   
   mov r12, r12, asr #1
   orr r4, r12, r4, asl #8

   str r4, [r0, #0]

                           ;7
   ldrb r12, [r1, #7]   
   ldrb r3, [r5, #7]
   add r4, r12, r3   
   mov r4, r4, asr #1
                           ;6
   ldrb r12, [r1, #6]   
   ldrb r3, [r5, #6]
   add r12, r12, r3   
   mov r12, r12, asr #1
   orr r4, r12, r4, asl #8
                           ;5
   ldrb r12, [r1, #5]   
   ldrb r3, [r5, #5]
   add r12, r12, r3   
   mov r12, r12, asr #1
   orr r4, r12, r4, asl #8
                           ;4
   ldrb r12, [r1, #4]   
   ldrb r3, [r5, #4]
   add r12, r12, r3   
   mov r12, r12, asr #1
   orr r4, r12, r4, asl #8

   str r4, [r0, #4]

   add r0, r0, r2   
   add r1, r1, r2
   subs r6, r6, #1   
   bne ihv_loop
   b i_hp_v_end

i_hp_v_no_round
   mov r6, #8
ihv_loop1
   add r5, r1, r2
                           ;3
   ldrb r12, [r1, #3]   
   ldrb r3, [r5, #3]
   add r4, r12, r3   
   add r4, r4, #1   
   mov r4, r4, asr #1
                           ;2
   ldrb r12, [r1, #2]   
   ldrb r3, [r5, #2]
   add r12, r12, r3   
   add r12, r12, #1   
   mov r12, r12, asr #1
   orr r4, r12, r4, asl #8
                           ;1
   ldrb r12, [r1, #1]   
   ldrb r3, [r5, #1]
   add r12, r12, r3   
   add r12, r12, #1   
   mov r12, r12, asr #1
   orr r4, r12, r4, asl #8
                           ;0
   ldrb r12, [r1, #0]   
   ldrb r3, [r5, #0]
   add r12, r12, r3   
   add r12, r12, #1   
   mov r12, r12, asr #1
   orr r4, r12, r4, asl #8

   str r4, [r0, #0]

                           ;7
   ldrb r12, [r1, #7]   
   ldrb r3, [r5, #7]
   add r4, r12, r3   
   add r4, r4, #1   
   mov r4, r4, asr #1
                           ;6
   ldrb r12, [r1, #6]   
   ldrb r3, [r5, #6]
   add r12, r12, r3   
   add r12, r12, #1   
   mov r12, r12, asr #1
   orr r4, r12, r4, asl #8
                           ;5
   ldrb r12, [r1, #5]   
   ldrb r3, [r5, #5]
   add r12, r12, r3   
   add r12, r12, #1   
   mov r12, r12, asr #1
   orr r4, r12, r4, asl #8
                           ;4
   ldrb r12, [r1, #4]   
   ldrb r3, [r5, #4]
   add r12, r12, r3   
   add r12, r12, #1   
   mov r12, r12, asr #1
   orr r4, r12, r4, asl #8

   str r4, [r0, #4]

   add r0, r0, r2   
   add r1, r1, r2
   subs r6, r6, #1   
   bne ihv_loop1
i_hp_v_end
   ldmfd sp!, { r4 - r6 }
   mov pc, lr

;----------------------------
transfer8x8_copy
   
   orr r12, r1, r2
   tst r12, #3
   bne tc_no_dw
                              ;dword version
;tc_dw_loop
   ldmia r1, { r12, r3 }  
   stmia r0, { r12, r3 }
   add r0, r0, r2   
   add r1, r1, r2

   ldmia r1, { r12, r3 }  
   stmia r0, { r12, r3 }
   add r0, r0, r2   
   add r1, r1, r2

   ldmia r1, { r12, r3 }  
   stmia r0, { r12, r3 }
   add r0, r0, r2   
   add r1, r1, r2

   ldmia r1, { r12, r3 }  
   stmia r0, { r12, r3 }
   add r0, r0, r2   
   add r1, r1, r2

   ldmia r1, { r12, r3 }  
   stmia r0, { r12, r3 }
   add r0, r0, r2   
   add r1, r1, r2

   ldmia r1, { r12, r3 }  
   stmia r0, { r12, r3 }
   add r0, r0, r2   
   add r1, r1, r2

   ldmia r1, { r12, r3 }  
   stmia r0, { r12, r3 }
   add r0, r0, r2   
   add r1, r1, r2

   ldmia r1, { r12, r3 }  
   stmia r0, { r12, r3 }

   mov pc, lr

tc_no_dw
   tst r12, #1   
   bne tc_no_w
                           ;word version
;tc_w_loop
   ldrh r12, [r1, #0]   
   strh r12, [r0, #0] 
   ldrh r12, [r1, #2]   
   strh r12, [r0, #2] 
   ldrh r12, [r1, #4]   
   strh r12, [r0, #4] 
   ldrh r12, [r1, #6]   
   strh r12, [r0, #6]
   add r0, r0, r2   
   add r1, r1, r2

   ldrh r12, [r1, #0]   
   strh r12, [r0, #0] 
   ldrh r12, [r1, #2]   
   strh r12, [r0, #2] 
   ldrh r12, [r1, #4]   
   strh r12, [r0, #4] 
   ldrh r12, [r1, #6]   
   strh r12, [r0, #6]
   add r0, r0, r2   
   add r1, r1, r2

   ldrh r12, [r1, #0]   
   strh r12, [r0, #0] 
   ldrh r12, [r1, #2]   
   strh r12, [r0, #2] 
   ldrh r12, [r1, #4]   
   strh r12, [r0, #4] 
   ldrh r12, [r1, #6]   
   strh r12, [r0, #6]
   add r0, r0, r2   
   add r1, r1, r2

   ldrh r12, [r1, #0]   
   strh r12, [r0, #0] 
   ldrh r12, [r1, #2]   
   strh r12, [r0, #2] 
   ldrh r12, [r1, #4]   
   strh r12, [r0, #4] 
   ldrh r12, [r1, #6]   
   strh r12, [r0, #6]
   add r0, r0, r2   
   add r1, r1, r2

   ldrh r12, [r1, #0]   
   strh r12, [r0, #0] 
   ldrh r12, [r1, #2]   
   strh r12, [r0, #2] 
   ldrh r12, [r1, #4]   
   strh r12, [r0, #4] 
   ldrh r12, [r1, #6]   
   strh r12, [r0, #6]
   add r0, r0, r2   
   add r1, r1, r2

   ldrh r12, [r1, #0]   
   strh r12, [r0, #0] 
   ldrh r12, [r1, #2]   
   strh r12, [r0, #2] 
   ldrh r12, [r1, #4]   
   strh r12, [r0, #4] 
   ldrh r12, [r1, #6]   
   strh r12, [r0, #6]
   add r0, r0, r2   
   add r1, r1, r2

   ldrh r12, [r1, #0]   
   strh r12, [r0, #0] 
   ldrh r12, [r1, #2]   
   strh r12, [r0, #2] 
   ldrh r12, [r1, #4]   
   strh r12, [r0, #4] 
   ldrh r12, [r1, #6]   
   strh r12, [r0, #6]
   add r0, r0, r2   
   add r1, r1, r2

   ldrh r12, [r1, #0]   
   strh r12, [r0, #0] 
   ldrh r12, [r1, #2]   
   strh r12, [r0, #2] 
   ldrh r12, [r1, #4]   
   strh r12, [r0, #4] 
   ldrh r12, [r1, #6]   
   strh r12, [r0, #6]

   mov pc, lr

tc_no_w
   mov r3, #8
tc_b_loop
   ldrb r12, [r1, #0]   
   strb r12, [r0, #0]
   ldrb r12, [r1, #1]   
   strb r12, [r0, #1]
   ldrb r12, [r1, #2]   
   strb r12, [r0, #2]
   ldrb r12, [r1, #3]   
   strb r12, [r0, #3]
   ldrb r12, [r1, #4]   
   strb r12, [r0, #4]
   ldrb r12, [r1, #5]   
   strb r12, [r0, #5]
   ldrb r12, [r1, #6]   
   strb r12, [r0, #6]
   ldrb r12, [r1, #7]   
   strb r12, [r0, #7]
   add r0, r0, r2   
   add r1, r1, r2

   subs r3, r3, #1   
   bne tc_b_loop

   mov pc, lr

;----------------------------

   END