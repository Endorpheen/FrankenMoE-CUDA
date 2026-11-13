
/home/end0/AI/LlamaServer/franken-goose-live-20261107/variants/moe-quantized-perf/build/matrix.o:     file format elf64-x86-64


Disassembly of section .text:

0000000000000230 <gg_function_34.constprop.0>:
     230:	push   %rbp
     231:	vpxor  %xmm0,%xmm0,%xmm0
     235:	mov    %rdi,%rax
     238:	xor    %ecx,%ecx
     23a:	mov    %rsp,%rbp
     23d:	and    $0xffffffffffffffe0,%rsp
     241:	sub    $0xa0,%rsp
     248:	vmovdqu 0x10(%rbp),%ymm2
     24d:	vmovdqu 0x30(%rbp),%ymm3
     252:	mov    %fs:0x28,%rdx
     25b:	mov    %rdx,0x98(%rsp)
     263:	xor    %edx,%edx
     265:	lea    0x10(%rsp),%r9
     26a:	vmovdqu %ymm0,0x50(%rsp)
     270:	vmovdqu %ymm0,0x70(%rsp)
     276:	lea    0x50(%rsp),%r8
     27b:	lea    0x70(%rsp),%rsi
     280:	vmovdqu %ymm2,0x10(%rsp)
     286:	vmovdqu %ymm3,0x30(%rsp)
     28c:	jmp    2a7 <gg_function_34.constprop.0+0x77>
     28e:	xchg   %ax,%ax
     290:	mov    %dl,(%r8,%rcx,1)
     294:	je     29b <gg_function_34.constprop.0+0x6b>
     296:	movzbl 0x30(%rsp,%rcx,1),%edx
     29b:	mov    %dl,(%rsi,%rcx,1)
     29e:	inc    %rcx
     2a1:	cmp    $0x20,%rcx
     2a5:	je     2cb <gg_function_34.constprop.0+0x9b>
     2a7:	movsbw (%r9,%rcx,1),%dx
     2ad:	test   %dl,%dl
     2af:	jns    290 <gg_function_34.constprop.0+0x60>
     2b1:	neg    %edx
     2b3:	mov    %dl,(%r8,%rcx,1)
     2b7:	movsbw 0x30(%rsp,%rcx,1),%dx
     2bd:	neg    %edx
     2bf:	mov    %dl,(%rsi,%rcx,1)
     2c2:	inc    %rcx
     2c5:	cmp    $0x20,%rcx
     2c9:	jne    2a7 <gg_function_34.constprop.0+0x77>
     2cb:	vmovdqu 0x50(%rsp),%ymm4
     2d1:	mov    $0x1,%edx
     2d6:	vpmaddubsw 0x70(%rsp),%ymm4,%ymm1
     2dd:	vmovd  %edx,%xmm0
     2e1:	vpbroadcastw %xmm0,%ymm0
     2e6:	vpmaddwd %ymm1,%ymm0,%ymm0
     2ea:	vmovdqu %ymm0,(%rax)
     2ee:	mov    0x98(%rsp),%rdx
     2f6:	sub    %fs:0x28,%rdx
     2ff:	jne    306 <gg_function_34.constprop.0+0xd6>
     301:	vzeroupper
     304:	leave
     305:	ret
     306:	vzeroupper
     309:	call   30e <gg_function_34.constprop.0+0xde>

Disassembly of section .text.unlikely:
