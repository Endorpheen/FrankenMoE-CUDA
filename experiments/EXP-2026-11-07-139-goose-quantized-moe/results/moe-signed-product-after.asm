
/home/end0/AI/LlamaServer/franken-goose-live-20261107/variants/moe-quantized-branchless-perf/build/matrix.o:     file format elf64-x86-64


Disassembly of section .text:

0000000000000ad0 <gg_function_34.constprop.0>:
     ad0:	push   %rbp
     ad1:	vpxor  %xmm5,%xmm5,%xmm5
     ad5:	mov    %rsp,%rbp
     ad8:	and    $0xffffffffffffffe0,%rsp
     adc:	sub    $0x60,%rsp
     ae0:	vmovdqu 0x10(%rbp),%ymm4
     ae5:	mov    %fs:0x28,%rdx
     aee:	mov    %rdx,0x58(%rsp)
     af3:	xor    %edx,%edx
     af5:	mov    $0x1,%edx
     afa:	vmovd  %edx,%xmm1
     afe:	mov    $0xff,%edx
     b03:	vpbroadcastb %xmm1,%ymm1
     b08:	vpsraw $0x7,%ymm4,%ymm0
     b0d:	vpandn %ymm1,%ymm0,%ymm0
     b11:	vpmovsxbw %xmm4,%ymm7
     b16:	vpsrlw $0x7,%ymm4,%ymm6
     b1b:	vpsubb %ymm1,%ymm0,%ymm0
     b1f:	vpsubw %ymm7,%ymm5,%ymm2
     b23:	vpand  %ymm6,%ymm1,%ymm6
     b27:	vpxor  %ymm0,%ymm4,%ymm3
     b2b:	vpxor  0x30(%rbp),%ymm0,%ymm0
     b30:	vextracti128 $0x1,%ymm4,%xmm4
     b36:	vpor   %ymm7,%ymm2,%ymm2
     b3a:	vpmovsxbw %xmm4,%ymm4
     b3f:	vpsraw $0xf,%ymm2,%ymm2
     b44:	vpaddb %ymm6,%ymm3,%ymm3
     b48:	vpsubw %ymm4,%ymm5,%ymm5
     b4c:	vpor   %ymm4,%ymm5,%ymm4
     b50:	vmovd  %edx,%xmm5
     b54:	mov    $0x1,%edx
     b59:	vpbroadcastw %xmm5,%ymm5
     b5e:	vpsraw $0xf,%ymm4,%ymm4
     b63:	vpand  %ymm2,%ymm5,%ymm2
     b67:	vpand  %ymm4,%ymm5,%ymm5
     b6b:	vpackuswb %ymm5,%ymm2,%ymm2
     b6f:	vpermq $0xd8,%ymm2,%ymm2
     b75:	vpand  %ymm2,%ymm1,%ymm1
     b79:	vpaddb %ymm6,%ymm0,%ymm0
     b7d:	vpunpcklbw %ymm0,%ymm0,%ymm2
     b81:	vpunpcklbw %ymm1,%ymm1,%ymm4
     b85:	vpunpckhbw %ymm0,%ymm0,%ymm0
     b89:	vpunpckhbw %ymm1,%ymm1,%ymm1
     b8d:	vpmullw %ymm1,%ymm0,%ymm0
     b91:	vpmullw %ymm4,%ymm2,%ymm2
     b95:	vpshufb 0x0(%rip),%ymm0,%ymm0        # b9e <gg_function_34.constprop.0+0xce>
     b9e:	vpshufb 0x0(%rip),%ymm2,%ymm1        # ba7 <gg_function_34.constprop.0+0xd7>
     ba7:	vpblendd $0xcc,%ymm0,%ymm1,%ymm0
     bad:	vpmaddubsw %ymm0,%ymm3,%ymm3
     bb2:	vmovd  %edx,%xmm0
     bb6:	vpbroadcastw %xmm0,%ymm0
     bbb:	vpmaddwd %ymm3,%ymm0,%ymm0
     bbf:	vmovdqu %ymm0,(%rdi)
     bc3:	mov    0x58(%rsp),%rdx
     bc8:	sub    %fs:0x28,%rdx
     bd1:	jne    bdb <gg_function_34.constprop.0+0x10b>
     bd3:	mov    %rdi,%rax
     bd6:	vzeroupper
     bd9:	leave
     bda:	ret
     bdb:	vzeroupper
     bde:	call   be3 <gg_function_34.constprop.0+0x113>

Disassembly of section .text.unlikely:
