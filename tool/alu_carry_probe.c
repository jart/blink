#include <stdio.h>
#include <stdint.h>
__attribute__((noinline)) static void f_8_add_0(uint8_t x, unsigned *out) {
 unsigned char cf,zf;
 __asm__ volatile("clc; addb $-1, %b0; setc %1; setz %2" : "+r"(x), "=qm"(cf), "=qm"(zf) :: "cc");
 out[0]=x;out[1]=cf;out[2]=zf;
}
__attribute__((noinline)) static void f_8_add_1(uint8_t x, unsigned *out) {
 unsigned char cf,zf;
 __asm__ volatile("stc; addb $-1, %b0; setc %1; setz %2" : "+r"(x), "=qm"(cf), "=qm"(zf) :: "cc");
 out[0]=x;out[1]=cf;out[2]=zf;
}
__attribute__((noinline)) static void f_8_sub_0(uint8_t x, unsigned *out) {
 unsigned char cf,zf;
 __asm__ volatile("clc; subb $-1, %b0; setc %1; setz %2" : "+r"(x), "=qm"(cf), "=qm"(zf) :: "cc");
 out[0]=x;out[1]=cf;out[2]=zf;
}
__attribute__((noinline)) static void f_8_sub_1(uint8_t x, unsigned *out) {
 unsigned char cf,zf;
 __asm__ volatile("stc; subb $-1, %b0; setc %1; setz %2" : "+r"(x), "=qm"(cf), "=qm"(zf) :: "cc");
 out[0]=x;out[1]=cf;out[2]=zf;
}
__attribute__((noinline)) static void f_8_adc_0(uint8_t x, unsigned *out) {
 unsigned char cf,zf;
 __asm__ volatile("clc; adcb $-1, %b0; setc %1; setz %2" : "+r"(x), "=qm"(cf), "=qm"(zf) :: "cc");
 out[0]=x;out[1]=cf;out[2]=zf;
}
__attribute__((noinline)) static void f_8_adc_1(uint8_t x, unsigned *out) {
 unsigned char cf,zf;
 __asm__ volatile("stc; adcb $-1, %b0; setc %1; setz %2" : "+r"(x), "=qm"(cf), "=qm"(zf) :: "cc");
 out[0]=x;out[1]=cf;out[2]=zf;
}
__attribute__((noinline)) static void f_8_sbb_0(uint8_t x, unsigned *out) {
 unsigned char cf,zf;
 __asm__ volatile("clc; sbbb $-1, %b0; setc %1; setz %2" : "+r"(x), "=qm"(cf), "=qm"(zf) :: "cc");
 out[0]=x;out[1]=cf;out[2]=zf;
}
__attribute__((noinline)) static void f_8_sbb_1(uint8_t x, unsigned *out) {
 unsigned char cf,zf;
 __asm__ volatile("stc; sbbb $-1, %b0; setc %1; setz %2" : "+r"(x), "=qm"(cf), "=qm"(zf) :: "cc");
 out[0]=x;out[1]=cf;out[2]=zf;
}
__attribute__((noinline)) static void f_16_add_0(uint16_t x, unsigned *out) {
 unsigned char cf,zf;
 __asm__ volatile("clc; addw $-1, %w0; setc %1; setz %2" : "+r"(x), "=qm"(cf), "=qm"(zf) :: "cc");
 out[0]=x;out[1]=cf;out[2]=zf;
}
__attribute__((noinline)) static void f_16_add_1(uint16_t x, unsigned *out) {
 unsigned char cf,zf;
 __asm__ volatile("stc; addw $-1, %w0; setc %1; setz %2" : "+r"(x), "=qm"(cf), "=qm"(zf) :: "cc");
 out[0]=x;out[1]=cf;out[2]=zf;
}
__attribute__((noinline)) static void f_16_sub_0(uint16_t x, unsigned *out) {
 unsigned char cf,zf;
 __asm__ volatile("clc; subw $-1, %w0; setc %1; setz %2" : "+r"(x), "=qm"(cf), "=qm"(zf) :: "cc");
 out[0]=x;out[1]=cf;out[2]=zf;
}
__attribute__((noinline)) static void f_16_sub_1(uint16_t x, unsigned *out) {
 unsigned char cf,zf;
 __asm__ volatile("stc; subw $-1, %w0; setc %1; setz %2" : "+r"(x), "=qm"(cf), "=qm"(zf) :: "cc");
 out[0]=x;out[1]=cf;out[2]=zf;
}
__attribute__((noinline)) static void f_16_adc_0(uint16_t x, unsigned *out) {
 unsigned char cf,zf;
 __asm__ volatile("clc; adcw $-1, %w0; setc %1; setz %2" : "+r"(x), "=qm"(cf), "=qm"(zf) :: "cc");
 out[0]=x;out[1]=cf;out[2]=zf;
}
__attribute__((noinline)) static void f_16_adc_1(uint16_t x, unsigned *out) {
 unsigned char cf,zf;
 __asm__ volatile("stc; adcw $-1, %w0; setc %1; setz %2" : "+r"(x), "=qm"(cf), "=qm"(zf) :: "cc");
 out[0]=x;out[1]=cf;out[2]=zf;
}
__attribute__((noinline)) static void f_16_sbb_0(uint16_t x, unsigned *out) {
 unsigned char cf,zf;
 __asm__ volatile("clc; sbbw $-1, %w0; setc %1; setz %2" : "+r"(x), "=qm"(cf), "=qm"(zf) :: "cc");
 out[0]=x;out[1]=cf;out[2]=zf;
}
__attribute__((noinline)) static void f_16_sbb_1(uint16_t x, unsigned *out) {
 unsigned char cf,zf;
 __asm__ volatile("stc; sbbw $-1, %w0; setc %1; setz %2" : "+r"(x), "=qm"(cf), "=qm"(zf) :: "cc");
 out[0]=x;out[1]=cf;out[2]=zf;
}
__attribute__((noinline)) static void f_32_add_0(uint32_t x, unsigned *out) {
 unsigned char cf,zf;
 __asm__ volatile("clc; addl $-1, %k0; setc %1; setz %2" : "+r"(x), "=qm"(cf), "=qm"(zf) :: "cc");
 out[0]=x;out[1]=cf;out[2]=zf;
}
__attribute__((noinline)) static void f_32_add_1(uint32_t x, unsigned *out) {
 unsigned char cf,zf;
 __asm__ volatile("stc; addl $-1, %k0; setc %1; setz %2" : "+r"(x), "=qm"(cf), "=qm"(zf) :: "cc");
 out[0]=x;out[1]=cf;out[2]=zf;
}
__attribute__((noinline)) static void f_32_sub_0(uint32_t x, unsigned *out) {
 unsigned char cf,zf;
 __asm__ volatile("clc; subl $-1, %k0; setc %1; setz %2" : "+r"(x), "=qm"(cf), "=qm"(zf) :: "cc");
 out[0]=x;out[1]=cf;out[2]=zf;
}
__attribute__((noinline)) static void f_32_sub_1(uint32_t x, unsigned *out) {
 unsigned char cf,zf;
 __asm__ volatile("stc; subl $-1, %k0; setc %1; setz %2" : "+r"(x), "=qm"(cf), "=qm"(zf) :: "cc");
 out[0]=x;out[1]=cf;out[2]=zf;
}
__attribute__((noinline)) static void f_32_adc_0(uint32_t x, unsigned *out) {
 unsigned char cf,zf;
 __asm__ volatile("clc; adcl $-1, %k0; setc %1; setz %2" : "+r"(x), "=qm"(cf), "=qm"(zf) :: "cc");
 out[0]=x;out[1]=cf;out[2]=zf;
}
__attribute__((noinline)) static void f_32_adc_1(uint32_t x, unsigned *out) {
 unsigned char cf,zf;
 __asm__ volatile("stc; adcl $-1, %k0; setc %1; setz %2" : "+r"(x), "=qm"(cf), "=qm"(zf) :: "cc");
 out[0]=x;out[1]=cf;out[2]=zf;
}
__attribute__((noinline)) static void f_32_sbb_0(uint32_t x, unsigned *out) {
 unsigned char cf,zf;
 __asm__ volatile("clc; sbbl $-1, %k0; setc %1; setz %2" : "+r"(x), "=qm"(cf), "=qm"(zf) :: "cc");
 out[0]=x;out[1]=cf;out[2]=zf;
}
__attribute__((noinline)) static void f_32_sbb_1(uint32_t x, unsigned *out) {
 unsigned char cf,zf;
 __asm__ volatile("stc; sbbl $-1, %k0; setc %1; setz %2" : "+r"(x), "=qm"(cf), "=qm"(zf) :: "cc");
 out[0]=x;out[1]=cf;out[2]=zf;
}
int main(void) { unsigned out[3];
for(int i=0;i<1000;++i) f_8_add_0(0u,out); printf("8 add 0 0 %u %u %u\n",out[0],out[1],out[2]);
for(int i=0;i<1000;++i) f_8_add_0(1u,out); printf("8 add 0 1 %u %u %u\n",out[0],out[1],out[2]);
for(int i=0;i<1000;++i) f_8_add_0(255u,out); printf("8 add 0 255 %u %u %u\n",out[0],out[1],out[2]);
for(int i=0;i<1000;++i) f_8_add_1(0u,out); printf("8 add 1 0 %u %u %u\n",out[0],out[1],out[2]);
for(int i=0;i<1000;++i) f_8_add_1(1u,out); printf("8 add 1 1 %u %u %u\n",out[0],out[1],out[2]);
for(int i=0;i<1000;++i) f_8_add_1(255u,out); printf("8 add 1 255 %u %u %u\n",out[0],out[1],out[2]);
for(int i=0;i<1000;++i) f_8_sub_0(0u,out); printf("8 sub 0 0 %u %u %u\n",out[0],out[1],out[2]);
for(int i=0;i<1000;++i) f_8_sub_0(1u,out); printf("8 sub 0 1 %u %u %u\n",out[0],out[1],out[2]);
for(int i=0;i<1000;++i) f_8_sub_0(255u,out); printf("8 sub 0 255 %u %u %u\n",out[0],out[1],out[2]);
for(int i=0;i<1000;++i) f_8_sub_1(0u,out); printf("8 sub 1 0 %u %u %u\n",out[0],out[1],out[2]);
for(int i=0;i<1000;++i) f_8_sub_1(1u,out); printf("8 sub 1 1 %u %u %u\n",out[0],out[1],out[2]);
for(int i=0;i<1000;++i) f_8_sub_1(255u,out); printf("8 sub 1 255 %u %u %u\n",out[0],out[1],out[2]);
for(int i=0;i<1000;++i) f_8_adc_0(0u,out); printf("8 adc 0 0 %u %u %u\n",out[0],out[1],out[2]);
for(int i=0;i<1000;++i) f_8_adc_0(1u,out); printf("8 adc 0 1 %u %u %u\n",out[0],out[1],out[2]);
for(int i=0;i<1000;++i) f_8_adc_0(255u,out); printf("8 adc 0 255 %u %u %u\n",out[0],out[1],out[2]);
for(int i=0;i<1000;++i) f_8_adc_1(0u,out); printf("8 adc 1 0 %u %u %u\n",out[0],out[1],out[2]);
for(int i=0;i<1000;++i) f_8_adc_1(1u,out); printf("8 adc 1 1 %u %u %u\n",out[0],out[1],out[2]);
for(int i=0;i<1000;++i) f_8_adc_1(255u,out); printf("8 adc 1 255 %u %u %u\n",out[0],out[1],out[2]);
for(int i=0;i<1000;++i) f_8_sbb_0(0u,out); printf("8 sbb 0 0 %u %u %u\n",out[0],out[1],out[2]);
for(int i=0;i<1000;++i) f_8_sbb_0(1u,out); printf("8 sbb 0 1 %u %u %u\n",out[0],out[1],out[2]);
for(int i=0;i<1000;++i) f_8_sbb_0(255u,out); printf("8 sbb 0 255 %u %u %u\n",out[0],out[1],out[2]);
for(int i=0;i<1000;++i) f_8_sbb_1(0u,out); printf("8 sbb 1 0 %u %u %u\n",out[0],out[1],out[2]);
for(int i=0;i<1000;++i) f_8_sbb_1(1u,out); printf("8 sbb 1 1 %u %u %u\n",out[0],out[1],out[2]);
for(int i=0;i<1000;++i) f_8_sbb_1(255u,out); printf("8 sbb 1 255 %u %u %u\n",out[0],out[1],out[2]);
for(int i=0;i<1000;++i) f_16_add_0(0u,out); printf("16 add 0 0 %u %u %u\n",out[0],out[1],out[2]);
for(int i=0;i<1000;++i) f_16_add_0(1u,out); printf("16 add 0 1 %u %u %u\n",out[0],out[1],out[2]);
for(int i=0;i<1000;++i) f_16_add_0(65535u,out); printf("16 add 0 65535 %u %u %u\n",out[0],out[1],out[2]);
for(int i=0;i<1000;++i) f_16_add_1(0u,out); printf("16 add 1 0 %u %u %u\n",out[0],out[1],out[2]);
for(int i=0;i<1000;++i) f_16_add_1(1u,out); printf("16 add 1 1 %u %u %u\n",out[0],out[1],out[2]);
for(int i=0;i<1000;++i) f_16_add_1(65535u,out); printf("16 add 1 65535 %u %u %u\n",out[0],out[1],out[2]);
for(int i=0;i<1000;++i) f_16_sub_0(0u,out); printf("16 sub 0 0 %u %u %u\n",out[0],out[1],out[2]);
for(int i=0;i<1000;++i) f_16_sub_0(1u,out); printf("16 sub 0 1 %u %u %u\n",out[0],out[1],out[2]);
for(int i=0;i<1000;++i) f_16_sub_0(65535u,out); printf("16 sub 0 65535 %u %u %u\n",out[0],out[1],out[2]);
for(int i=0;i<1000;++i) f_16_sub_1(0u,out); printf("16 sub 1 0 %u %u %u\n",out[0],out[1],out[2]);
for(int i=0;i<1000;++i) f_16_sub_1(1u,out); printf("16 sub 1 1 %u %u %u\n",out[0],out[1],out[2]);
for(int i=0;i<1000;++i) f_16_sub_1(65535u,out); printf("16 sub 1 65535 %u %u %u\n",out[0],out[1],out[2]);
for(int i=0;i<1000;++i) f_16_adc_0(0u,out); printf("16 adc 0 0 %u %u %u\n",out[0],out[1],out[2]);
for(int i=0;i<1000;++i) f_16_adc_0(1u,out); printf("16 adc 0 1 %u %u %u\n",out[0],out[1],out[2]);
for(int i=0;i<1000;++i) f_16_adc_0(65535u,out); printf("16 adc 0 65535 %u %u %u\n",out[0],out[1],out[2]);
for(int i=0;i<1000;++i) f_16_adc_1(0u,out); printf("16 adc 1 0 %u %u %u\n",out[0],out[1],out[2]);
for(int i=0;i<1000;++i) f_16_adc_1(1u,out); printf("16 adc 1 1 %u %u %u\n",out[0],out[1],out[2]);
for(int i=0;i<1000;++i) f_16_adc_1(65535u,out); printf("16 adc 1 65535 %u %u %u\n",out[0],out[1],out[2]);
for(int i=0;i<1000;++i) f_16_sbb_0(0u,out); printf("16 sbb 0 0 %u %u %u\n",out[0],out[1],out[2]);
for(int i=0;i<1000;++i) f_16_sbb_0(1u,out); printf("16 sbb 0 1 %u %u %u\n",out[0],out[1],out[2]);
for(int i=0;i<1000;++i) f_16_sbb_0(65535u,out); printf("16 sbb 0 65535 %u %u %u\n",out[0],out[1],out[2]);
for(int i=0;i<1000;++i) f_16_sbb_1(0u,out); printf("16 sbb 1 0 %u %u %u\n",out[0],out[1],out[2]);
for(int i=0;i<1000;++i) f_16_sbb_1(1u,out); printf("16 sbb 1 1 %u %u %u\n",out[0],out[1],out[2]);
for(int i=0;i<1000;++i) f_16_sbb_1(65535u,out); printf("16 sbb 1 65535 %u %u %u\n",out[0],out[1],out[2]);
for(int i=0;i<1000;++i) f_32_add_0(0u,out); printf("32 add 0 0 %u %u %u\n",out[0],out[1],out[2]);
for(int i=0;i<1000;++i) f_32_add_0(1u,out); printf("32 add 0 1 %u %u %u\n",out[0],out[1],out[2]);
for(int i=0;i<1000;++i) f_32_add_0(4294967295u,out); printf("32 add 0 4294967295 %u %u %u\n",out[0],out[1],out[2]);
for(int i=0;i<1000;++i) f_32_add_1(0u,out); printf("32 add 1 0 %u %u %u\n",out[0],out[1],out[2]);
for(int i=0;i<1000;++i) f_32_add_1(1u,out); printf("32 add 1 1 %u %u %u\n",out[0],out[1],out[2]);
for(int i=0;i<1000;++i) f_32_add_1(4294967295u,out); printf("32 add 1 4294967295 %u %u %u\n",out[0],out[1],out[2]);
for(int i=0;i<1000;++i) f_32_sub_0(0u,out); printf("32 sub 0 0 %u %u %u\n",out[0],out[1],out[2]);
for(int i=0;i<1000;++i) f_32_sub_0(1u,out); printf("32 sub 0 1 %u %u %u\n",out[0],out[1],out[2]);
for(int i=0;i<1000;++i) f_32_sub_0(4294967295u,out); printf("32 sub 0 4294967295 %u %u %u\n",out[0],out[1],out[2]);
for(int i=0;i<1000;++i) f_32_sub_1(0u,out); printf("32 sub 1 0 %u %u %u\n",out[0],out[1],out[2]);
for(int i=0;i<1000;++i) f_32_sub_1(1u,out); printf("32 sub 1 1 %u %u %u\n",out[0],out[1],out[2]);
for(int i=0;i<1000;++i) f_32_sub_1(4294967295u,out); printf("32 sub 1 4294967295 %u %u %u\n",out[0],out[1],out[2]);
for(int i=0;i<1000;++i) f_32_adc_0(0u,out); printf("32 adc 0 0 %u %u %u\n",out[0],out[1],out[2]);
for(int i=0;i<1000;++i) f_32_adc_0(1u,out); printf("32 adc 0 1 %u %u %u\n",out[0],out[1],out[2]);
for(int i=0;i<1000;++i) f_32_adc_0(4294967295u,out); printf("32 adc 0 4294967295 %u %u %u\n",out[0],out[1],out[2]);
for(int i=0;i<1000;++i) f_32_adc_1(0u,out); printf("32 adc 1 0 %u %u %u\n",out[0],out[1],out[2]);
for(int i=0;i<1000;++i) f_32_adc_1(1u,out); printf("32 adc 1 1 %u %u %u\n",out[0],out[1],out[2]);
for(int i=0;i<1000;++i) f_32_adc_1(4294967295u,out); printf("32 adc 1 4294967295 %u %u %u\n",out[0],out[1],out[2]);
for(int i=0;i<1000;++i) f_32_sbb_0(0u,out); printf("32 sbb 0 0 %u %u %u\n",out[0],out[1],out[2]);
for(int i=0;i<1000;++i) f_32_sbb_0(1u,out); printf("32 sbb 0 1 %u %u %u\n",out[0],out[1],out[2]);
for(int i=0;i<1000;++i) f_32_sbb_0(4294967295u,out); printf("32 sbb 0 4294967295 %u %u %u\n",out[0],out[1],out[2]);
for(int i=0;i<1000;++i) f_32_sbb_1(0u,out); printf("32 sbb 1 0 %u %u %u\n",out[0],out[1],out[2]);
for(int i=0;i<1000;++i) f_32_sbb_1(1u,out); printf("32 sbb 1 1 %u %u %u\n",out[0],out[1],out[2]);
for(int i=0;i<1000;++i) f_32_sbb_1(4294967295u,out); printf("32 sbb 1 4294967295 %u %u %u\n",out[0],out[1],out[2]);
return 0; }
