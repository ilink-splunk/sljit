/*
 *    Stack-less Just-In-Time compiler
 *
 *    Copyright 2009-2012 Zoltan Herczeg (hzmester@freemail.hu). All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are
 * permitted provided that the following conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright notice, this list of
 *      conditions and the following disclaimer.
 *
 *   2. Redistributions in binary form must reproduce the above copyright notice, this list
 *      of conditions and the following disclaimer in the documentation and/or other materials
 *      provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER(S) AND CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE COPYRIGHT HOLDER(S) OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

SLJIT_API_FUNC_ATTRIBUTE SLJIT_CONST char* sljit_get_platform_name(void)
{
	return "PowerPC" SLJIT_CPUINFO;
}

/* Length of an instruction word.
   Both for ppc-32 and ppc-64. */
typedef sljit_ui sljit_ins;

#ifdef _AIX
#include <sys/cache.h>
#endif

static void ppc_cache_flush(sljit_ins *from, sljit_ins *to)
{
#ifdef _AIX
	_sync_cache_range((caddr_t)from, (int)((size_t)to - (size_t)from));
#elif defined(__GNUC__) || (defined(__IBM_GCC_ASM) && __IBM_GCC_ASM)
#	if defined(_ARCH_PWR) || defined(_ARCH_PWR2)
	/* Cache flush for POWER architecture. */
	while (from < to) {
		__asm__ volatile (
			"clf 0, %0\n"
			"dcs\n"
			: : "r"(from)
		);
		from++;
	}
	__asm__ volatile ( "ics" );
#	elif defined(_ARCH_COM) && !defined(_ARCH_PPC)
#	error "Cache flush is not implemented for PowerPC/POWER common mode."
#	else
	/* Cache flush for PowerPC architecture. */
	while (from < to) {
		__asm__ volatile (
			"dcbf 0, %0\n"
			"sync\n"
			"icbi 0, %0\n"
			: : "r"(from)
		);
		from++;
	}
	__asm__ volatile ( "isync" );
#	endif
#	ifdef __xlc__
#	warning "This file may fail to compile if -qfuncsect is used"
#	endif
#elif defined(__xlc__)
#error "Please enable GCC syntax for inline assembly statements with -qasm=gcc"
#else
#error "This platform requires a cache flush implementation."
#endif /* _AIX */
}

#define TMP_REG1	(SLJIT_NO_REGISTERS + 1)
#define TMP_REG2	(SLJIT_NO_REGISTERS + 2)
#define TMP_REG3	(SLJIT_NO_REGISTERS + 3)
#define ZERO_REG	(SLJIT_NO_REGISTERS + 4)

#define TMP_FREG1	(0)
#define TMP_FREG2	(SLJIT_FLOAT_REG6 + 1)

static SLJIT_CONST sljit_ub reg_map[SLJIT_NO_REGISTERS + 5] = {
	0, 3, 4, 5, 6, 7, 30, 29, 28, 27, 26, 1, 8, 9, 10, 31
};

/* --------------------------------------------------------------------- */
/*  Instrucion forms                                                     */
/* --------------------------------------------------------------------- */
#define D(d)		(reg_map[d] << 21)
#define S(s)		(reg_map[s] << 21)
#define A(a)		(reg_map[a] << 16)
#define B(b)		(reg_map[b] << 11)
#define C(c)		(reg_map[c] << 6)
#define FD(fd)		((fd) << 21)
#define FA(fa)		((fa) << 16)
#define FB(fb)		((fb) << 11)
#define FC(fc)		((fc) << 6)
#define IMM(imm)	((imm) & 0xffff)
#define CRD(d)		((d) << 21)

/* Instruction bit sections.
   OE and Rc flag (see ALT_SET_FLAGS). */
#define OERC(flags)	(((flags & ALT_SET_FLAGS) >> 10) | (flags & ALT_SET_FLAGS))
/* Rc flag (see ALT_SET_FLAGS). */
#define RC(flags)	((flags & ALT_SET_FLAGS) >> 10)
#define HI(opcode)	((opcode) << 26)
#define LO(opcode)	((opcode) << 1)

#define ADD		(HI(31) | LO(266))
#define ADDC		(HI(31) | LO(10))
#define ADDE		(HI(31) | LO(138))
#define ADDI		(HI(14))
#define ADDIC		(HI(13))
#define ADDIS		(HI(15))
#define ADDME		(HI(31) | LO(234))
#define AND		(HI(31) | LO(28))
#define ANDI		(HI(28))
#define ANDIS		(HI(29))
#define Bx		(HI(18))
#define BCx		(HI(16))
#define BCCTR		(HI(19) | LO(528) | (3 << 11))
#define BLR		(HI(19) | LO(16) | (0x14 << 21))
#define CNTLZD		(HI(31) | LO(58))
#define CNTLZW		(HI(31) | LO(26))
#define CMP		(HI(31) | LO(0))
#define CMPI		(HI(11))
#define CMPL		(HI(31) | LO(32))
#define CMPLI		(HI(10))
#define CROR		(HI(19) | LO(449))
#define DIVD		(HI(31) | LO(489))
#define DIVDU		(HI(31) | LO(457))
#define DIVW		(HI(31) | LO(491))
#define DIVWU		(HI(31) | LO(459))
#define EXTSB		(HI(31) | LO(954))
#define EXTSH		(HI(31) | LO(922))
#define EXTSW		(HI(31) | LO(986))
#define FABS		(HI(63) | LO(264))
#define FADD		(HI(63) | LO(21))
#define FADDS		(HI(59) | LO(21))
#define FCMPU		(HI(63) | LO(0))
#define FDIV		(HI(63) | LO(18))
#define FDIVS		(HI(59) | LO(18))
#define FMR		(HI(63) | LO(72))
#define FMUL		(HI(63) | LO(25))
#define FMULS		(HI(59) | LO(25))
#define FNEG		(HI(63) | LO(40))
#define FSUB		(HI(63) | LO(20))
#define FSUBS		(HI(59) | LO(20))
#define LD		(HI(58) | 0)
#define LWZ		(HI(32))
#define MFCR		(HI(31) | LO(19))
#define MFLR		(HI(31) | LO(339) | 0x80000)
#define MFXER		(HI(31) | LO(339) | 0x10000)
#define MTCTR		(HI(31) | LO(467) | 0x90000)
#define MTLR		(HI(31) | LO(467) | 0x80000)
#define MTXER		(HI(31) | LO(467) | 0x10000)
#define MULHD		(HI(31) | LO(73))
#define MULHDU		(HI(31) | LO(9))
#define MULHW		(HI(31) | LO(75))
#define MULHWU		(HI(31) | LO(11))
#define MULLD		(HI(31) | LO(233))
#define MULLI		(HI(7))
#define MULLW		(HI(31) | LO(235))
#define NEG		(HI(31) | LO(104))
#define NOP		(HI(24))
#define NOR		(HI(31) | LO(124))
#define OR		(HI(31) | LO(444))
#define ORI		(HI(24))
#define ORIS		(HI(25))
#define RLDICL		(HI(30))
#define RLWINM		(HI(21))
#define SLD		(HI(31) | LO(27))
#define SLW		(HI(31) | LO(24))
#define SRAD		(HI(31) | LO(794))
#define SRADI		(HI(31) | LO(413 << 1))
#define SRAW		(HI(31) | LO(792))
#define SRAWI		(HI(31) | LO(824))
#define SRD		(HI(31) | LO(539))
#define SRW		(HI(31) | LO(536))
#define STD		(HI(62) | 0)
#define STDU		(HI(62) | 1)
#define STDUX		(HI(31) | LO(181))
#define STW		(HI(36))
#define STWU		(HI(37))
#define STWUX		(HI(31) | LO(183))
#define SUBF		(HI(31) | LO(40))
#define SUBFC		(HI(31) | LO(8))
#define SUBFE		(HI(31) | LO(136))
#define SUBFIC		(HI(8))
#define XOR		(HI(31) | LO(316))
#define XORI		(HI(26))
#define XORIS		(HI(27))

#define SIMM_MAX	(0x7fff)
#define SIMM_MIN	(-0x8000)
#define UIMM_MAX	(0xffff)

#if (defined SLJIT_INDIRECT_CALL && SLJIT_INDIRECT_CALL)
SLJIT_API_FUNC_ATTRIBUTE void sljit_set_function_context(void** func_ptr, struct sljit_function_context* context, sljit_sw addr, void* func)
{
	sljit_sw* ptrs;
	if (func_ptr)
		*func_ptr = (void*)context;
	ptrs = (sljit_sw*)func;
	context->addr = addr ? addr : ptrs[0];
	context->r2 = ptrs[1];
	context->r11 = ptrs[2];
}
#endif

static sljit_si push_inst(struct sljit_compiler *compiler, sljit_ins ins)
{
	sljit_ins *ptr = (sljit_ins*)ensure_buf(compiler, sizeof(sljit_ins));
	FAIL_IF(!ptr);
	*ptr = ins;
	compiler->size++;
	return SLJIT_SUCCESS;
}

static SLJIT_INLINE sljit_si optimize_jump(struct sljit_jump *jump, sljit_ins *code_ptr, sljit_ins *code)
{
	sljit_sw diff;
	sljit_uw target_addr;

	if (jump->flags & SLJIT_REWRITABLE_JUMP)
		return 0;

	if (jump->flags & JUMP_ADDR)
		target_addr = jump->u.target;
	else {
		SLJIT_ASSERT(jump->flags & JUMP_LABEL);
		target_addr = (sljit_uw)(code + jump->u.label->size);
	}
	diff = ((sljit_sw)target_addr - (sljit_sw)(code_ptr)) & ~0x3l;

	if (jump->flags & UNCOND_B) {
		if (diff <= 0x01ffffff && diff >= -0x02000000) {
			jump->flags |= PATCH_B;
			return 1;
		}
		if (target_addr <= 0x03ffffff) {
			jump->flags |= PATCH_B | ABSOLUTE_B;
			return 1;
		}
	}
	else {
		if (diff <= 0x7fff && diff >= -0x8000) {
			jump->flags |= PATCH_B;
			return 1;
		}
		if (target_addr <= 0xffff) {
			jump->flags |= PATCH_B | ABSOLUTE_B;
			return 1;
		}
	}
	return 0;
}

SLJIT_API_FUNC_ATTRIBUTE void* sljit_generate_code(struct sljit_compiler *compiler)
{
	struct sljit_memory_fragment *buf;
	sljit_ins *code;
	sljit_ins *code_ptr;
	sljit_ins *buf_ptr;
	sljit_ins *buf_end;
	sljit_uw word_count;
	sljit_uw addr;

	struct sljit_label *label;
	struct sljit_jump *jump;
	struct sljit_const *const_;

	CHECK_ERROR_PTR();
	check_sljit_generate_code(compiler);
	reverse_buf(compiler);

#if (defined SLJIT_INDIRECT_CALL && SLJIT_INDIRECT_CALL)
#if (defined SLJIT_CONFIG_PPC_64 && SLJIT_CONFIG_PPC_64)
	compiler->size += (compiler->size & 0x1) + (sizeof(struct sljit_function_context) / sizeof(sljit_ins));
#else
	compiler->size += (sizeof(struct sljit_function_context) / sizeof(sljit_ins));
#endif
#endif
	code = (sljit_ins*)SLJIT_MALLOC_EXEC(compiler->size * sizeof(sljit_ins));
	PTR_FAIL_WITH_EXEC_IF(code);
	buf = compiler->buf;

	code_ptr = code;
	word_count = 0;
	label = compiler->labels;
	jump = compiler->jumps;
	const_ = compiler->consts;
	do {
		buf_ptr = (sljit_ins*)buf->memory;
		buf_end = buf_ptr + (buf->used_size >> 2);
		do {
			*code_ptr = *buf_ptr++;
			SLJIT_ASSERT(!label || label->size >= word_count);
			SLJIT_ASSERT(!jump || jump->addr >= word_count);
			SLJIT_ASSERT(!const_ || const_->addr >= word_count);
			/* These structures are ordered by their address. */
			if (label && label->size == word_count) {
				/* Just recording the address. */
				label->addr = (sljit_uw)code_ptr;
				label->size = code_ptr - code;
				label = label->next;
			}
			if (jump && jump->addr == word_count) {
#if (defined SLJIT_CONFIG_PPC_32 && SLJIT_CONFIG_PPC_32)
				jump->addr = (sljit_uw)(code_ptr - 3);
#else
				jump->addr = (sljit_uw)(code_ptr - 6);
#endif
				if (optimize_jump(jump, code_ptr, code)) {
#if (defined SLJIT_CONFIG_PPC_32 && SLJIT_CONFIG_PPC_32)
					code_ptr[-3] = code_ptr[0];
					code_ptr -= 3;
#else
					code_ptr[-6] = code_ptr[0];
					code_ptr -= 6;
#endif
				}
				jump = jump->next;
			}
			if (const_ && const_->addr == word_count) {
				/* Just recording the address. */
				const_->addr = (sljit_uw)code_ptr;
				const_ = const_->next;
			}
			code_ptr ++;
			word_count ++;
		} while (buf_ptr < buf_end);

		buf = buf->next;
	} while (buf);

	if (label && label->size == word_count) {
		label->addr = (sljit_uw)code_ptr;
		label->size = code_ptr - code;
		label = label->next;
	}

	SLJIT_ASSERT(!label);
	SLJIT_ASSERT(!jump);
	SLJIT_ASSERT(!const_);
#if (defined SLJIT_INDIRECT_CALL && SLJIT_INDIRECT_CALL)
	SLJIT_ASSERT(code_ptr - code <= (sljit_sw)compiler->size - (sizeof(struct sljit_function_context) / sizeof(sljit_ins)));
#else
	SLJIT_ASSERT(code_ptr - code <= (sljit_sw)compiler->size);
#endif

	jump = compiler->jumps;
	while (jump) {
		do {
			addr = (jump->flags & JUMP_LABEL) ? jump->u.label->addr : jump->u.target;
			buf_ptr = (sljit_ins*)jump->addr;
			if (jump->flags & PATCH_B) {
				if (jump->flags & UNCOND_B) {
					if (!(jump->flags & ABSOLUTE_B)) {
						addr = addr - jump->addr;
						SLJIT_ASSERT((sljit_sw)addr <= 0x01ffffff && (sljit_sw)addr >= -0x02000000);
						*buf_ptr = Bx | (addr & 0x03fffffc) | ((*buf_ptr) & 0x1);
					}
					else {
						SLJIT_ASSERT(addr <= 0x03ffffff);
						*buf_ptr = Bx | (addr & 0x03fffffc) | 0x2 | ((*buf_ptr) & 0x1);
					}
				}
				else {
					if (!(jump->flags & ABSOLUTE_B)) {
						addr = addr - jump->addr;
						SLJIT_ASSERT((sljit_sw)addr <= 0x7fff && (sljit_sw)addr >= -0x8000);
						*buf_ptr = BCx | (addr & 0xfffc) | ((*buf_ptr) & 0x03ff0001);
					}
					else {
						addr = addr & ~0x3l;
						SLJIT_ASSERT(addr <= 0xffff);
						*buf_ptr = BCx | (addr & 0xfffc) | 0x2 | ((*buf_ptr) & 0x03ff0001);
					}

				}
				break;
			}
			/* Set the fields of immediate loads. */
#if (defined SLJIT_CONFIG_PPC_32 && SLJIT_CONFIG_PPC_32)
			buf_ptr[0] = (buf_ptr[0] & 0xffff0000) | ((addr >> 16) & 0xffff);
			buf_ptr[1] = (buf_ptr[1] & 0xffff0000) | (addr & 0xffff);
#else
			buf_ptr[0] = (buf_ptr[0] & 0xffff0000) | ((addr >> 48) & 0xffff);
			buf_ptr[1] = (buf_ptr[1] & 0xffff0000) | ((addr >> 32) & 0xffff);
			buf_ptr[3] = (buf_ptr[3] & 0xffff0000) | ((addr >> 16) & 0xffff);
			buf_ptr[4] = (buf_ptr[4] & 0xffff0000) | (addr & 0xffff);
#endif
		} while (0);
		jump = jump->next;
	}

	SLJIT_CACHE_FLUSH(code, code_ptr);
	compiler->error = SLJIT_ERR_COMPILED;
	compiler->executable_size = compiler->size * sizeof(sljit_ins);

#if (defined SLJIT_INDIRECT_CALL && SLJIT_INDIRECT_CALL)
#if (defined SLJIT_CONFIG_PPC_64 && SLJIT_CONFIG_PPC_64)
	if (((sljit_sw)code_ptr) & 0x4)
		code_ptr++;
	sljit_set_function_context(NULL, (struct sljit_function_context*)code_ptr, (sljit_sw)code, (void*)sljit_generate_code);
	return code_ptr;
#else
	sljit_set_function_context(NULL, (struct sljit_function_context*)code_ptr, (sljit_sw)code, (void*)sljit_generate_code);
	return code_ptr;
#endif
#else
	return code;
#endif
}

/* --------------------------------------------------------------------- */
/*  Entry, exit                                                          */
/* --------------------------------------------------------------------- */

/* inp_flags: */

/* Creates an index in data_transfer_insts array. */
#define LOAD_DATA	0x01
#define INDEXED		0x02
#define WRITE_BACK	0x04
#define WORD_DATA	0x00
#define BYTE_DATA	0x08
#define HALF_DATA	0x10
#define INT_DATA	0x18
#define SIGNED_DATA	0x20
/* Separates integer and floating point registers */
#define GPR_REG		0x3f
#define DOUBLE_DATA	0x40

#define MEM_MASK	0x7f

/* Other inp_flags. */

#define ARG_TEST	0x000100
/* Integer opertion and set flags -> requires exts on 64 bit systems. */
#define ALT_SIGN_EXT	0x000200
/* This flag affects the RC() and OERC() macros. */
#define ALT_SET_FLAGS	0x000400
#define ALT_FORM1	0x010000
#define ALT_FORM2	0x020000
#define ALT_FORM3	0x040000
#define ALT_FORM4	0x080000
#define ALT_FORM5	0x100000
#define ALT_FORM6	0x200000

/* Source and destination is register. */
#define REG_DEST	0x000001
#define REG1_SOURCE	0x000002
#define REG2_SOURCE	0x000004
/* getput_arg_fast returned true. */
#define FAST_DEST	0x000008
/* Multiple instructions are required. */
#define SLOW_DEST	0x000010
/*
ALT_SIGN_EXT		0x000200
ALT_SET_FLAGS		0x000400
ALT_FORM1		0x010000
...
ALT_FORM6		0x200000 */

#if (defined SLJIT_CONFIG_PPC_32 && SLJIT_CONFIG_PPC_32)
#include "sljitNativePPC_32.c"
#else
#include "sljitNativePPC_64.c"
#endif

#if (defined SLJIT_CONFIG_PPC_32 && SLJIT_CONFIG_PPC_32)
#define STACK_STORE	STW
#define STACK_LOAD	LWZ
#else
#define STACK_STORE	STD
#define STACK_LOAD	LD
#endif

SLJIT_API_FUNC_ATTRIBUTE sljit_si sljit_emit_enter(struct sljit_compiler *compiler, sljit_si args, sljit_si temporaries, sljit_si saveds, sljit_si local_size)
{
	CHECK_ERROR();
	check_sljit_emit_enter(compiler, args, temporaries, saveds, local_size);

	compiler->temporaries = temporaries;
	compiler->saveds = saveds;
#if (defined SLJIT_DEBUG && SLJIT_DEBUG)
	compiler->logical_local_size = local_size;
#endif

	FAIL_IF(push_inst(compiler, MFLR | D(0)));
	FAIL_IF(push_inst(compiler, STACK_STORE | S(ZERO_REG) | A(SLJIT_LOCALS_REG) | IMM(-(sljit_si)(sizeof(sljit_sw))) ));
	if (saveds >= 1)
		FAIL_IF(push_inst(compiler, STACK_STORE | S(SLJIT_SAVED_REG1) | A(SLJIT_LOCALS_REG) | IMM(-2 * (sljit_si)(sizeof(sljit_sw))) ));
	if (saveds >= 2)
		FAIL_IF(push_inst(compiler, STACK_STORE | S(SLJIT_SAVED_REG2) | A(SLJIT_LOCALS_REG) | IMM(-3 * (sljit_si)(sizeof(sljit_sw))) ));
	if (saveds >= 3)
		FAIL_IF(push_inst(compiler, STACK_STORE | S(SLJIT_SAVED_REG3) | A(SLJIT_LOCALS_REG) | IMM(-4 * (sljit_si)(sizeof(sljit_sw))) ));
	if (saveds >= 4)
		FAIL_IF(push_inst(compiler, STACK_STORE | S(SLJIT_SAVED_EREG1) | A(SLJIT_LOCALS_REG) | IMM(-5 * (sljit_si)(sizeof(sljit_sw))) ));
	if (saveds >= 5)
		FAIL_IF(push_inst(compiler, STACK_STORE | S(SLJIT_SAVED_EREG2) | A(SLJIT_LOCALS_REG) | IMM(-6 * (sljit_si)(sizeof(sljit_sw))) ));
	FAIL_IF(push_inst(compiler, STACK_STORE | S(0) | A(SLJIT_LOCALS_REG) | IMM(sizeof(sljit_sw)) ));

	FAIL_IF(push_inst(compiler, ADDI | D(ZERO_REG) | A(0) | 0));
	if (args >= 1)
		FAIL_IF(push_inst(compiler, OR | S(SLJIT_TEMPORARY_REG1) | A(SLJIT_SAVED_REG1) | B(SLJIT_TEMPORARY_REG1)));
	if (args >= 2)
		FAIL_IF(push_inst(compiler, OR | S(SLJIT_TEMPORARY_REG2) | A(SLJIT_SAVED_REG2) | B(SLJIT_TEMPORARY_REG2)));
	if (args >= 3)
		FAIL_IF(push_inst(compiler, OR | S(SLJIT_TEMPORARY_REG3) | A(SLJIT_SAVED_REG3) | B(SLJIT_TEMPORARY_REG3)));

#if (defined SLJIT_INDIRECT_CALL && SLJIT_INDIRECT_CALL)
	compiler->local_size = (1 + saveds + 6 + 8) * sizeof(sljit_sw) + local_size;
#else
	compiler->local_size = (1 + saveds + 2) * sizeof(sljit_sw) + local_size;
#endif
	compiler->local_size = (compiler->local_size + 15) & ~0xf;

#if (defined SLJIT_CONFIG_PPC_32 && SLJIT_CONFIG_PPC_32)
	if (compiler->local_size <= SIMM_MAX)
		FAIL_IF(push_inst(compiler, STWU | S(SLJIT_LOCALS_REG) | A(SLJIT_LOCALS_REG) | IMM(-compiler->local_size)));
	else {
		FAIL_IF(load_immediate(compiler, 0, -compiler->local_size));
		FAIL_IF(push_inst(compiler, STWUX | S(SLJIT_LOCALS_REG) | A(SLJIT_LOCALS_REG) | B(0)));
	}
#else
	if (compiler->local_size <= SIMM_MAX)
		FAIL_IF(push_inst(compiler, STDU | S(SLJIT_LOCALS_REG) | A(SLJIT_LOCALS_REG) | IMM(-compiler->local_size)));
	else {
		FAIL_IF(load_immediate(compiler, 0, -compiler->local_size));
		FAIL_IF(push_inst(compiler, STDUX | S(SLJIT_LOCALS_REG) | A(SLJIT_LOCALS_REG) | B(0)));
	}
#endif

	return SLJIT_SUCCESS;
}

SLJIT_API_FUNC_ATTRIBUTE void sljit_set_context(struct sljit_compiler *compiler, sljit_si args, sljit_si temporaries, sljit_si saveds, sljit_si local_size)
{
	CHECK_ERROR_VOID();
	check_sljit_set_context(compiler, args, temporaries, saveds, local_size);

	compiler->temporaries = temporaries;
	compiler->saveds = saveds;
#if (defined SLJIT_DEBUG && SLJIT_DEBUG)
	compiler->logical_local_size = local_size;
#endif

#if (defined SLJIT_INDIRECT_CALL && SLJIT_INDIRECT_CALL)
	compiler->local_size = (1 + saveds + 6 + 8) * sizeof(sljit_sw) + local_size;
#else
	compiler->local_size = (1 + saveds + 2) * sizeof(sljit_sw) + local_size;
#endif
	compiler->local_size = (compiler->local_size + 15) & ~0xf;
}

SLJIT_API_FUNC_ATTRIBUTE sljit_si sljit_emit_return(struct sljit_compiler *compiler, sljit_si op, sljit_si src, sljit_sw srcw)
{
	CHECK_ERROR();
	check_sljit_emit_return(compiler, op, src, srcw);

	FAIL_IF(emit_mov_before_return(compiler, op, src, srcw));

	if (compiler->local_size <= SIMM_MAX)
		FAIL_IF(push_inst(compiler, ADDI | D(SLJIT_LOCALS_REG) | A(SLJIT_LOCALS_REG) | IMM(compiler->local_size)));
	else {
		FAIL_IF(load_immediate(compiler, 0, compiler->local_size));
		FAIL_IF(push_inst(compiler, ADD | D(SLJIT_LOCALS_REG) | A(SLJIT_LOCALS_REG) | B(0)));
	}

	FAIL_IF(push_inst(compiler, STACK_LOAD | D(0) | A(SLJIT_LOCALS_REG) | IMM(sizeof(sljit_sw))));
	if (compiler->saveds >= 5)
		FAIL_IF(push_inst(compiler, STACK_LOAD | D(SLJIT_SAVED_EREG2) | A(SLJIT_LOCALS_REG) | IMM(-6 * (sljit_si)(sizeof(sljit_sw))) ));
	if (compiler->saveds >= 4)
		FAIL_IF(push_inst(compiler, STACK_LOAD | D(SLJIT_SAVED_EREG1) | A(SLJIT_LOCALS_REG) | IMM(-5 * (sljit_si)(sizeof(sljit_sw))) ));
	if (compiler->saveds >= 3)
		FAIL_IF(push_inst(compiler, STACK_LOAD | D(SLJIT_SAVED_REG3) | A(SLJIT_LOCALS_REG) | IMM(-4 * (sljit_si)(sizeof(sljit_sw))) ));
	if (compiler->saveds >= 2)
		FAIL_IF(push_inst(compiler, STACK_LOAD | D(SLJIT_SAVED_REG2) | A(SLJIT_LOCALS_REG) | IMM(-3 * (sljit_si)(sizeof(sljit_sw))) ));
	if (compiler->saveds >= 1)
		FAIL_IF(push_inst(compiler, STACK_LOAD | D(SLJIT_SAVED_REG1) | A(SLJIT_LOCALS_REG) | IMM(-2 * (sljit_si)(sizeof(sljit_sw))) ));
	FAIL_IF(push_inst(compiler, STACK_LOAD | D(ZERO_REG) | A(SLJIT_LOCALS_REG) | IMM(-(sljit_si)(sizeof(sljit_sw))) ));

	FAIL_IF(push_inst(compiler, MTLR | S(0)));
	FAIL_IF(push_inst(compiler, BLR));

	return SLJIT_SUCCESS;
}

#undef STACK_STORE
#undef STACK_LOAD

/* --------------------------------------------------------------------- */
/*  Operators                                                            */
/* --------------------------------------------------------------------- */

/* i/x - immediate/indexed form
   n/w - no write-back / write-back (1 bit)
   s/l - store/load (1 bit)
   u/s - signed/unsigned (1 bit)
   w/b/h/i - word/byte/half/int allowed (2 bit)
   It contans 32 items, but not all are different. */

/* 64 bit only: [reg+imm] must be aligned to 4 bytes. */
#define ADDR_MODE2	0x10000
/* 64-bit only: there is no lwau instruction. */
#define UPDATE_REQ	0x20000

#if (defined SLJIT_CONFIG_PPC_32 && SLJIT_CONFIG_PPC_32)
#define ARCH_32_64(a, b)	a
#define INST_CODE_AND_DST(inst, flags, reg) \
	((inst) | (((flags) & MEM_MASK) <= GPR_REG ? D(reg) : FD(reg)))
#else
#define ARCH_32_64(a, b)	b
#define INST_CODE_AND_DST(inst, flags, reg) \
	(((inst) & ~(ADDR_MODE2 | UPDATE_REQ)) | (((flags) & MEM_MASK) <= GPR_REG ? D(reg) : FD(reg)))
#endif

static SLJIT_CONST sljit_ins data_transfer_insts[64 + 8] = {

/* -------- Unsigned -------- */

/* Word. */

/* u w n i s */ ARCH_32_64(HI(36) /* stw */, HI(62) | ADDR_MODE2 | 0x0 /* std */),
/* u w n i l */ ARCH_32_64(HI(32) /* lwz */, HI(58) | ADDR_MODE2 | 0x0 /* ld */),
/* u w n x s */ ARCH_32_64(HI(31) | LO(151) /* stwx */, HI(31) | LO(149) /* stdx */),
/* u w n x l */ ARCH_32_64(HI(31) | LO(23) /* lwzx */, HI(31) | LO(21) /* ldx */),

/* u w w i s */ ARCH_32_64(HI(37) /* stwu */, HI(62) | ADDR_MODE2 | 0x1 /* stdu */),
/* u w w i l */ ARCH_32_64(HI(33) /* lwzu */, HI(58) | ADDR_MODE2 | 0x1 /* ldu */),
/* u w w x s */ ARCH_32_64(HI(31) | LO(183) /* stwux */, HI(31) | LO(181) /* stdux */),
/* u w w x l */ ARCH_32_64(HI(31) | LO(55) /* lwzux */, HI(31) | LO(53) /* ldux */),

/* Byte. */

/* u b n i s */ HI(38) /* stb */, 
/* u b n i l */ HI(34) /* lbz */,
/* u b n x s */ HI(31) | LO(215) /* stbx */,
/* u b n x l */ HI(31) | LO(87) /* lbzx */,

/* u b w i s */ HI(39) /* stbu */,
/* u b w i l */ HI(35) /* lbzu */,
/* u b w x s */ HI(31) | LO(247) /* stbux */,
/* u b w x l */ HI(31) | LO(119) /* lbzux */,

/* Half. */

/* u h n i s */ HI(44) /* sth */,
/* u h n i l */ HI(40) /* lhz */,
/* u h n x s */ HI(31) | LO(407) /* sthx */,
/* u h n x l */ HI(31) | LO(279) /* lhzx */,

/* u h w i s */ HI(45) /* sthu */,
/* u h w i l */ HI(41) /* lhzu */,
/* u h w x s */ HI(31) | LO(439) /* sthux */,
/* u h w x l */ HI(31) | LO(311) /* lhzux */,

/* Int. */

/* u i n i s */ HI(36) /* stw */,
/* u i n i l */ HI(32) /* lwz */,
/* u i n x s */ HI(31) | LO(151) /* stwx */,
/* u i n x l */ HI(31) | LO(23) /* lwzx */,

/* u i w i s */ HI(37) /* stwu */,
/* u i w i l */ HI(33) /* lwzu */,
/* u i w x s */ HI(31) | LO(183) /* stwux */,
/* u i w x l */ HI(31) | LO(55) /* lwzux */,

/* -------- Signed -------- */

/* Word. */

/* s w n i s */ ARCH_32_64(HI(36) /* stw */, HI(62) | ADDR_MODE2 | 0x0 /* std */),
/* s w n i l */ ARCH_32_64(HI(32) /* lwz */, HI(58) | ADDR_MODE2 | 0x0 /* ld */),
/* s w n x s */ ARCH_32_64(HI(31) | LO(151) /* stwx */, HI(31) | LO(149) /* stdx */),
/* s w n x l */ ARCH_32_64(HI(31) | LO(23) /* lwzx */, HI(31) | LO(21) /* ldx */),

/* s w w i s */ ARCH_32_64(HI(37) /* stwu */, HI(62) | ADDR_MODE2 | 0x1 /* stdu */),
/* s w w i l */ ARCH_32_64(HI(33) /* lwzu */, HI(58) | ADDR_MODE2 | 0x1 /* ldu */),
/* s w w x s */ ARCH_32_64(HI(31) | LO(183) /* stwux */, HI(31) | LO(181) /* stdux */),
/* s w w x l */ ARCH_32_64(HI(31) | LO(55) /* lwzux */, HI(31) | LO(53) /* ldux */),

/* Byte. */

/* s b n i s */ HI(38) /* stb */,
/* s b n i l */ HI(34) /* lbz */ /* EXTS_REQ */,
/* s b n x s */ HI(31) | LO(215) /* stbx */,
/* s b n x l */ HI(31) | LO(87) /* lbzx */ /* EXTS_REQ */,

/* s b w i s */ HI(39) /* stbu */,
/* s b w i l */ HI(35) /* lbzu */ /* EXTS_REQ */,
/* s b w x s */ HI(31) | LO(247) /* stbux */,
/* s b w x l */ HI(31) | LO(119) /* lbzux */ /* EXTS_REQ */,

/* Half. */

/* s h n i s */ HI(44) /* sth */,
/* s h n i l */ HI(42) /* lha */,
/* s h n x s */ HI(31) | LO(407) /* sthx */,
/* s h n x l */ HI(31) | LO(343) /* lhax */,

/* s h w i s */ HI(45) /* sthu */,
/* s h w i l */ HI(43) /* lhau */,
/* s h w x s */ HI(31) | LO(439) /* sthux */,
/* s h w x l */ HI(31) | LO(375) /* lhaux */,

/* Int. */

/* s i n i s */ HI(36) /* stw */,
/* s i n i l */ ARCH_32_64(HI(32) /* lwz */, HI(58) | ADDR_MODE2 | 0x2 /* lwa */),
/* s i n x s */ HI(31) | LO(151) /* stwx */,
/* s i n x l */ ARCH_32_64(HI(31) | LO(23) /* lwzx */, HI(31) | LO(341) /* lwax */),

/* s i w i s */ HI(37) /* stwu */,
/* s i w i l */ ARCH_32_64(HI(33) /* lwzu */, HI(58) | ADDR_MODE2 | UPDATE_REQ | 0x2 /* lwa */),
/* s i w x s */ HI(31) | LO(183) /* stwux */,
/* s i w x l */ ARCH_32_64(HI(31) | LO(55) /* lwzux */, HI(31) | LO(373) /* lwaux */),

/* -------- Double -------- */

/* d   n i s */ HI(54) /* stfd */,
/* d   n i l */ HI(50) /* lfd */,
/* d   n x s */ HI(31) | LO(727) /* stfdx */,
/* d   n x l */ HI(31) | LO(599) /* lfdx */,

/* s   n i s */ HI(52) /* stfs */,
/* s   n i l */ HI(48) /* lfs */,
/* s   n x s */ HI(31) | LO(663) /* stfsx */,
/* s   n x l */ HI(31) | LO(535) /* lfsx */,

};

#undef ARCH_32_64

/* Simple cases, (no caching is required). */
static sljit_si getput_arg_fast(struct sljit_compiler *compiler, sljit_si inp_flags, sljit_si reg, sljit_si arg, sljit_sw argw)
{
	sljit_ins inst;
#if (defined SLJIT_CONFIG_PPC_64 && SLJIT_CONFIG_PPC_64)
	sljit_si tmp_reg;
#endif

	SLJIT_ASSERT(arg & SLJIT_MEM);
	if (!(arg & 0xf)) {
#if (defined SLJIT_CONFIG_PPC_32 && SLJIT_CONFIG_PPC_32)
		if (argw <= SIMM_MAX && argw >= SIMM_MIN) {
			if (inp_flags & ARG_TEST)
				return 1;

			inst = data_transfer_insts[(inp_flags & ~WRITE_BACK) & MEM_MASK];
			SLJIT_ASSERT(!(inst & (ADDR_MODE2 | UPDATE_REQ)));
			push_inst(compiler, INST_CODE_AND_DST(inst, inp_flags, reg) | IMM(argw));
			return -1;
		}
#else
		inst = data_transfer_insts[(inp_flags & ~WRITE_BACK) & MEM_MASK];
		if (argw <= SIMM_MAX && argw >= SIMM_MIN &&
				(!(inst & ADDR_MODE2) || (argw & 0x3) == 0)) {
			if (inp_flags & ARG_TEST)
				return 1;

			push_inst(compiler, INST_CODE_AND_DST(inst, inp_flags, reg) | IMM(argw));
			return -1;
		}
#endif
		return 0;
	}

	if (!(arg & 0xf0)) {
#if (defined SLJIT_CONFIG_PPC_32 && SLJIT_CONFIG_PPC_32)
		if (argw <= SIMM_MAX && argw >= SIMM_MIN) {
			if (inp_flags & ARG_TEST)
				return 1;

			inst = data_transfer_insts[inp_flags & MEM_MASK];
			SLJIT_ASSERT(!(inst & (ADDR_MODE2 | UPDATE_REQ)));
			push_inst(compiler, INST_CODE_AND_DST(inst, inp_flags, reg) | A(arg & 0xf) | IMM(argw));
			return -1;
		}
#else
		inst = data_transfer_insts[inp_flags & MEM_MASK];
		if (argw <= SIMM_MAX && argw >= SIMM_MIN && (!(inst & ADDR_MODE2) || (argw & 0x3) == 0)) {
			if (inp_flags & ARG_TEST)
				return 1;

			if ((inp_flags & WRITE_BACK) && (inst & UPDATE_REQ)) {
				tmp_reg = (inp_flags & LOAD_DATA) ? (arg & 0xf) : TMP_REG3;
				if (push_inst(compiler, ADDI | D(tmp_reg) | A(arg & 0xf) | IMM(argw)))
					return -1;
				arg = tmp_reg | SLJIT_MEM;
				argw = 0;
			}
			push_inst(compiler, INST_CODE_AND_DST(inst, inp_flags, reg) | A(arg & 0xf) | IMM(argw));
			return -1;
		}
#endif
	}
	else if (!(argw & 0x3)) {
		if (inp_flags & ARG_TEST)
			return 1;
		inst = data_transfer_insts[(inp_flags | INDEXED) & MEM_MASK];
		SLJIT_ASSERT(!(inst & (ADDR_MODE2 | UPDATE_REQ)));
		push_inst(compiler, INST_CODE_AND_DST(inst, inp_flags, reg) | A(arg & 0xf) | B((arg >> 4) & 0xf));
		return -1;
	}
	return 0;
}

/* See getput_arg below.
   Note: can_cache is called only for binary operators. Those operator always
   uses word arguments without write back. */
static sljit_si can_cache(sljit_si arg, sljit_sw argw, sljit_si next_arg, sljit_sw next_argw)
{
	SLJIT_ASSERT((arg & SLJIT_MEM) && (next_arg & SLJIT_MEM));

	if (!(arg & 0xf))
		return (next_arg & SLJIT_MEM) && ((sljit_uw)argw - (sljit_uw)next_argw <= SIMM_MAX || (sljit_uw)next_argw - (sljit_uw)argw <= SIMM_MAX);

	if (arg & 0xf0)
		return ((arg & 0xf0) == (next_arg & 0xf0) && (argw & 0x3) == (next_argw & 0x3));

	if (argw <= SIMM_MAX && argw >= SIMM_MIN) {
		if (arg == next_arg && (next_argw >= SIMM_MAX && next_argw <= SIMM_MIN))
			return 1;
	}

	if (arg == next_arg && ((sljit_uw)argw - (sljit_uw)next_argw <= SIMM_MAX || (sljit_uw)next_argw - (sljit_uw)argw <= SIMM_MAX))
		return 1;

	return 0;
}

#if (defined SLJIT_CONFIG_PPC_64 && SLJIT_CONFIG_PPC_64)
#define ADJUST_CACHED_IMM(imm) \
	if ((inst & ADDR_MODE2) && (imm & 0x3)) { \
		/* Adjust cached value. Fortunately this is really a rare case */ \
		compiler->cache_argw += imm & 0x3; \
		FAIL_IF(push_inst(compiler, ADDI | D(TMP_REG3) | A(TMP_REG3) | (imm & 0x3))); \
		imm &= ~0x3; \
	}
#else
#define ADJUST_CACHED_IMM(imm)
#endif

/* Emit the necessary instructions. See can_cache above. */
static sljit_si getput_arg(struct sljit_compiler *compiler, sljit_si inp_flags, sljit_si reg, sljit_si arg, sljit_sw argw, sljit_si next_arg, sljit_sw next_argw)
{
	sljit_si tmp_r;
	sljit_ins inst;

	SLJIT_ASSERT(arg & SLJIT_MEM);

	tmp_r = ((inp_flags & LOAD_DATA) && ((inp_flags) & MEM_MASK) <= GPR_REG) ? reg : TMP_REG1;
	/* Special case for "mov reg, [reg, ... ]". */
	if ((arg & 0xf) == tmp_r)
		tmp_r = TMP_REG1;

	if (!(arg & 0xf)) {
		inst = data_transfer_insts[(inp_flags & ~WRITE_BACK) & MEM_MASK];
		if ((compiler->cache_arg & SLJIT_IMM) && (((sljit_uw)argw - (sljit_uw)compiler->cache_argw) <= SIMM_MAX || ((sljit_uw)compiler->cache_argw - (sljit_uw)argw) <= SIMM_MAX)) {
			argw = argw - compiler->cache_argw;
			ADJUST_CACHED_IMM(argw);
			SLJIT_ASSERT(!(inst & UPDATE_REQ));
			return push_inst(compiler, INST_CODE_AND_DST(inst, inp_flags, reg) | A(TMP_REG3) | IMM(argw));
		}

		if ((next_arg & SLJIT_MEM) && (argw - next_argw <= SIMM_MAX || next_argw - argw <= SIMM_MAX)) {
			SLJIT_ASSERT(inp_flags & LOAD_DATA);

			compiler->cache_arg = SLJIT_IMM;
			compiler->cache_argw = argw;
			tmp_r = TMP_REG3;
		}

		FAIL_IF(load_immediate(compiler, tmp_r, argw));
		return push_inst(compiler, INST_CODE_AND_DST(inst, inp_flags, reg) | A(tmp_r));
	}

	if (SLJIT_UNLIKELY(arg & 0xf0)) {
		argw &= 0x3;
		/* Otherwise getput_arg_fast would capture it. */
		SLJIT_ASSERT(argw);

		if ((SLJIT_MEM | (arg & 0xf0)) == compiler->cache_arg && argw == compiler->cache_argw)
			tmp_r = TMP_REG3;
		else {
			if ((arg & 0xf0) == (next_arg & 0xf0) && argw == (next_argw & 0x3)) {
				compiler->cache_arg = SLJIT_MEM | (arg & 0xf0);
				compiler->cache_argw = argw;
				tmp_r = TMP_REG3;
			}
#if (defined SLJIT_CONFIG_PPC_32 && SLJIT_CONFIG_PPC_32)
			FAIL_IF(push_inst(compiler, RLWINM | S((arg >> 4) & 0xf) | A(tmp_r) | (argw << 11) | ((31 - argw) << 1)));
#else
			FAIL_IF(push_inst(compiler, RLDI(tmp_r, (arg >> 4) & 0xf, argw, 63 - argw, 1)));
#endif
		}
		inst = data_transfer_insts[(inp_flags | INDEXED) & MEM_MASK];
		SLJIT_ASSERT(!(inst & (ADDR_MODE2 | UPDATE_REQ)));
		return push_inst(compiler, INST_CODE_AND_DST(inst, inp_flags, reg) | A(arg & 0xf) | B(tmp_r));
	}

	inst = data_transfer_insts[inp_flags & MEM_MASK];

	if (compiler->cache_arg == arg && ((sljit_uw)argw - (sljit_uw)compiler->cache_argw <= SIMM_MAX || (sljit_uw)compiler->cache_argw - (sljit_uw)argw <= SIMM_MAX)) {
		SLJIT_ASSERT(!(inp_flags & WRITE_BACK));
		argw = argw - compiler->cache_argw;
		ADJUST_CACHED_IMM(argw);
		return push_inst(compiler, INST_CODE_AND_DST(inst, inp_flags, reg) | A(TMP_REG3) | IMM(argw));
	}

	if ((compiler->cache_arg & SLJIT_IMM) && compiler->cache_argw == argw) {
		inst = data_transfer_insts[(inp_flags | INDEXED) & MEM_MASK];
		SLJIT_ASSERT(!(inst & (ADDR_MODE2 | UPDATE_REQ)));
		return push_inst(compiler, INST_CODE_AND_DST(inst, inp_flags, reg) | A(arg & 0xf) | B(TMP_REG3));
	}

	if (argw == next_argw && (next_arg & SLJIT_MEM)) {
		SLJIT_ASSERT(inp_flags & LOAD_DATA);
		FAIL_IF(load_immediate(compiler, TMP_REG3, argw));

		compiler->cache_arg = SLJIT_IMM;
		compiler->cache_argw = argw;

		inst = data_transfer_insts[(inp_flags | INDEXED) & MEM_MASK];
		SLJIT_ASSERT(!(inst & (ADDR_MODE2 | UPDATE_REQ)));
		return push_inst(compiler, INST_CODE_AND_DST(inst, inp_flags, reg) | A(arg & 0xf) | B(TMP_REG3));
	}

	if (arg == next_arg && !(inp_flags & WRITE_BACK) && ((sljit_uw)argw - (sljit_uw)next_argw <= SIMM_MAX || (sljit_uw)next_argw - (sljit_uw)argw <= SIMM_MAX)) {
		SLJIT_ASSERT(inp_flags & LOAD_DATA);
		FAIL_IF(load_immediate(compiler, TMP_REG3, argw));
		FAIL_IF(push_inst(compiler, ADD | D(TMP_REG3) | A(TMP_REG3) | B(arg & 0xf)));

		compiler->cache_arg = arg;
		compiler->cache_argw = argw;

		return push_inst(compiler, INST_CODE_AND_DST(inst, inp_flags, reg) | A(TMP_REG3));
	}

	/* Get the indexed version instead of the normal one. */
	inst = data_transfer_insts[(inp_flags | INDEXED) & MEM_MASK];
	SLJIT_ASSERT(!(inst & (ADDR_MODE2 | UPDATE_REQ)));
	FAIL_IF(load_immediate(compiler, tmp_r, argw));
	return push_inst(compiler, INST_CODE_AND_DST(inst, inp_flags, reg) | A(arg & 0xf) | B(tmp_r));
}

static SLJIT_INLINE sljit_si emit_op_mem2(struct sljit_compiler *compiler, sljit_si flags, sljit_si reg, sljit_si arg1, sljit_sw arg1w, sljit_si arg2, sljit_sw arg2w)
{
	if (getput_arg_fast(compiler, flags, reg, arg1, arg1w))
		return compiler->error;
	return getput_arg(compiler, flags, reg, arg1, arg1w, arg2, arg2w);
}

static sljit_si emit_op(struct sljit_compiler *compiler, sljit_si op, sljit_si input_flags,
	sljit_si dst, sljit_sw dstw,
	sljit_si src1, sljit_sw src1w,
	sljit_si src2, sljit_sw src2w)
{
	/* arg1 goes to TMP_REG1 or src reg
	   arg2 goes to TMP_REG2, imm or src reg
	   TMP_REG3 can be used for caching
	   result goes to TMP_REG2, so put result can use TMP_REG1 and TMP_REG3. */
	sljit_si dst_r;
	sljit_si src1_r;
	sljit_si src2_r;
	sljit_si sugg_src2_r = TMP_REG2;
	sljit_si flags = input_flags & (ALT_FORM1 | ALT_FORM2 | ALT_FORM3 | ALT_FORM4 | ALT_FORM5 | ALT_FORM6 | ALT_SIGN_EXT | ALT_SET_FLAGS);

	compiler->cache_arg = 0;
	compiler->cache_argw = 0;

	/* Destination check. */
	if (dst >= SLJIT_TEMPORARY_REG1 && dst <= ZERO_REG) {
		dst_r = dst;
		flags |= REG_DEST;
		if (op >= SLJIT_MOV && op <= SLJIT_MOVU_SI)
			sugg_src2_r = dst_r;
	}
	else if (dst == SLJIT_UNUSED) {
		if (op >= SLJIT_MOV && op <= SLJIT_MOVU_SI && !(src2 & SLJIT_MEM))
			return SLJIT_SUCCESS;
		dst_r = TMP_REG2;
	}
	else {
		SLJIT_ASSERT(dst & SLJIT_MEM);
		if (getput_arg_fast(compiler, input_flags | ARG_TEST, TMP_REG2, dst, dstw)) {
			flags |= FAST_DEST;
			dst_r = TMP_REG2;
		}
		else {
			flags |= SLOW_DEST;
			dst_r = 0;
		}
	}

	/* Source 1. */
	if (src1 >= SLJIT_TEMPORARY_REG1 && src1 <= ZERO_REG) {
		src1_r = src1;
		flags |= REG1_SOURCE;
	}
	else if (src1 & SLJIT_IMM) {
		FAIL_IF(load_immediate(compiler, TMP_REG1, src1w));
		src1_r = TMP_REG1;
	}
	else if (getput_arg_fast(compiler, input_flags | LOAD_DATA, TMP_REG1, src1, src1w)) {
		FAIL_IF(compiler->error);
		src1_r = TMP_REG1;
	}
	else
		src1_r = 0;

	/* Source 2. */
	if (src2 >= SLJIT_TEMPORARY_REG1 && src2 <= ZERO_REG) {
		src2_r = src2;
		flags |= REG2_SOURCE;
		if (!(flags & REG_DEST) && op >= SLJIT_MOV && op <= SLJIT_MOVU_SI)
			dst_r = src2_r;
	}
	else if (src2 & SLJIT_IMM) {
		FAIL_IF(load_immediate(compiler, sugg_src2_r, src2w));
		src2_r = sugg_src2_r;
	}
	else if (getput_arg_fast(compiler, input_flags | LOAD_DATA, sugg_src2_r, src2, src2w)) {
		FAIL_IF(compiler->error);
		src2_r = sugg_src2_r;
	}
	else
		src2_r = 0;

	/* src1_r, src2_r and dst_r can be zero (=unprocessed).
	   All arguments are complex addressing modes, and it is a binary operator. */
	if (src1_r == 0 && src2_r == 0 && dst_r == 0) {
		if (!can_cache(src1, src1w, src2, src2w) && can_cache(src1, src1w, dst, dstw)) {
			FAIL_IF(getput_arg(compiler, input_flags | LOAD_DATA, TMP_REG2, src2, src2w, src1, src1w));
			FAIL_IF(getput_arg(compiler, input_flags | LOAD_DATA, TMP_REG1, src1, src1w, dst, dstw));
		}
		else {
			FAIL_IF(getput_arg(compiler, input_flags | LOAD_DATA, TMP_REG1, src1, src1w, src2, src2w));
			FAIL_IF(getput_arg(compiler, input_flags | LOAD_DATA, TMP_REG2, src2, src2w, dst, dstw));
		}
		src1_r = TMP_REG1;
		src2_r = TMP_REG2;
	}
	else if (src1_r == 0 && src2_r == 0) {
		FAIL_IF(getput_arg(compiler, input_flags | LOAD_DATA, TMP_REG1, src1, src1w, src2, src2w));
		src1_r = TMP_REG1;
	}
	else if (src1_r == 0 && dst_r == 0) {
		FAIL_IF(getput_arg(compiler, input_flags | LOAD_DATA, TMP_REG1, src1, src1w, dst, dstw));
		src1_r = TMP_REG1;
	}
	else if (src2_r == 0 && dst_r == 0) {
		FAIL_IF(getput_arg(compiler, input_flags | LOAD_DATA, sugg_src2_r, src2, src2w, dst, dstw));
		src2_r = sugg_src2_r;
	}

	if (dst_r == 0)
		dst_r = TMP_REG2;

	if (src1_r == 0) {
		FAIL_IF(getput_arg(compiler, input_flags | LOAD_DATA, TMP_REG1, src1, src1w, 0, 0));
		src1_r = TMP_REG1;
	}

	if (src2_r == 0) {
		FAIL_IF(getput_arg(compiler, input_flags | LOAD_DATA, sugg_src2_r, src2, src2w, 0, 0));
		src2_r = sugg_src2_r;
	}

	FAIL_IF(emit_single_op(compiler, op, flags, dst_r, src1_r, src2_r));

	if (flags & (FAST_DEST | SLOW_DEST)) {
		if (flags & FAST_DEST)
			FAIL_IF(getput_arg_fast(compiler, input_flags, dst_r, dst, dstw));
		else
			FAIL_IF(getput_arg(compiler, input_flags, dst_r, dst, dstw, 0, 0));
	}
	return SLJIT_SUCCESS;
}

SLJIT_API_FUNC_ATTRIBUTE sljit_si sljit_emit_op0(struct sljit_compiler *compiler, sljit_si op)
{
	CHECK_ERROR();
	check_sljit_emit_op0(compiler, op);

	switch (GET_OPCODE(op)) {
	case SLJIT_BREAKPOINT:
	case SLJIT_NOP:
		return push_inst(compiler, NOP);
		break;
	case SLJIT_UMUL:
	case SLJIT_SMUL:
		FAIL_IF(push_inst(compiler, OR | S(SLJIT_TEMPORARY_REG1) | A(TMP_REG1) | B(SLJIT_TEMPORARY_REG1)));
#if (defined SLJIT_CONFIG_PPC_64 && SLJIT_CONFIG_PPC_64)
		FAIL_IF(push_inst(compiler, MULLD | D(SLJIT_TEMPORARY_REG1) | A(TMP_REG1) | B(SLJIT_TEMPORARY_REG2)));
		return push_inst(compiler, (GET_OPCODE(op) == SLJIT_UMUL ? MULHDU : MULHD) | D(SLJIT_TEMPORARY_REG2) | A(TMP_REG1) | B(SLJIT_TEMPORARY_REG2));
#else
		FAIL_IF(push_inst(compiler, MULLW | D(SLJIT_TEMPORARY_REG1) | A(TMP_REG1) | B(SLJIT_TEMPORARY_REG2)));
		return push_inst(compiler, (GET_OPCODE(op) == SLJIT_UMUL ? MULHWU : MULHW) | D(SLJIT_TEMPORARY_REG2) | A(TMP_REG1) | B(SLJIT_TEMPORARY_REG2));
#endif
	case SLJIT_UDIV:
	case SLJIT_SDIV:
		FAIL_IF(push_inst(compiler, OR | S(SLJIT_TEMPORARY_REG1) | A(TMP_REG1) | B(SLJIT_TEMPORARY_REG1)));
#if (defined SLJIT_CONFIG_PPC_64 && SLJIT_CONFIG_PPC_64)
		if (op & SLJIT_INT_OP) {
			FAIL_IF(push_inst(compiler, (GET_OPCODE(op) == SLJIT_UDIV ? DIVWU : DIVW) | D(SLJIT_TEMPORARY_REG1) | A(TMP_REG1) | B(SLJIT_TEMPORARY_REG2)));
			FAIL_IF(push_inst(compiler, MULLW | D(SLJIT_TEMPORARY_REG2) | A(SLJIT_TEMPORARY_REG1) | B(SLJIT_TEMPORARY_REG2)));
			return push_inst(compiler, SUBF | D(SLJIT_TEMPORARY_REG2) | A(SLJIT_TEMPORARY_REG2) | B(TMP_REG1));
		}
		FAIL_IF(push_inst(compiler, (GET_OPCODE(op) == SLJIT_UDIV ? DIVDU : DIVD) | D(SLJIT_TEMPORARY_REG1) | A(TMP_REG1) | B(SLJIT_TEMPORARY_REG2)));
		FAIL_IF(push_inst(compiler, MULLD | D(SLJIT_TEMPORARY_REG2) | A(SLJIT_TEMPORARY_REG1) | B(SLJIT_TEMPORARY_REG2)));
		return push_inst(compiler, SUBF | D(SLJIT_TEMPORARY_REG2) | A(SLJIT_TEMPORARY_REG2) | B(TMP_REG1));
#else
		FAIL_IF(push_inst(compiler, (GET_OPCODE(op) == SLJIT_UDIV ? DIVWU : DIVW) | D(SLJIT_TEMPORARY_REG1) | A(TMP_REG1) | B(SLJIT_TEMPORARY_REG2)));
		FAIL_IF(push_inst(compiler, MULLW | D(SLJIT_TEMPORARY_REG2) | A(SLJIT_TEMPORARY_REG1) | B(SLJIT_TEMPORARY_REG2)));
		return push_inst(compiler, SUBF | D(SLJIT_TEMPORARY_REG2) | A(SLJIT_TEMPORARY_REG2) | B(TMP_REG1));
#endif
	}

	return SLJIT_SUCCESS;
}

#define EMIT_MOV(type, type_flags, type_cast) \
	emit_op(compiler, (src & SLJIT_IMM) ? SLJIT_MOV : type, flags | (type_flags), dst, dstw, TMP_REG1, 0, src, (src & SLJIT_IMM) ? type_cast srcw : srcw)

SLJIT_API_FUNC_ATTRIBUTE sljit_si sljit_emit_op1(struct sljit_compiler *compiler, sljit_si op,
	sljit_si dst, sljit_sw dstw,
	sljit_si src, sljit_sw srcw)
{
	sljit_si flags = GET_FLAGS(op) ? ALT_SET_FLAGS : 0;
	sljit_si op_flags = GET_ALL_FLAGS(op);

	CHECK_ERROR();
	check_sljit_emit_op1(compiler, op, dst, dstw, src, srcw);
	ADJUST_LOCAL_OFFSET(dst, dstw);
	ADJUST_LOCAL_OFFSET(src, srcw);

	op = GET_OPCODE(op);
	if ((src & SLJIT_IMM) && srcw == 0 && op >= SLJIT_NOT)
		src = ZERO_REG;

	if (op_flags & SLJIT_SET_O)
		FAIL_IF(push_inst(compiler, MTXER | S(ZERO_REG)));

	if (op_flags & SLJIT_INT_OP) {
		if (op >= SLJIT_MOV && op <= SLJIT_MOVU_P) {
			if (src <= SLJIT_NO_REGISTERS && src == dst) {
				if (!TYPE_CAST_NEEDED(op))
					return SLJIT_SUCCESS;
			}
#if (defined SLJIT_CONFIG_PPC_64 && SLJIT_CONFIG_PPC_64)
			if (op == SLJIT_MOV_SI && (src & SLJIT_MEM))
				op = SLJIT_MOV_UI;
			if (op == SLJIT_MOVU_SI && (src & SLJIT_MEM))
				op = SLJIT_MOVU_UI;
			if (op == SLJIT_MOV_UI && (src & SLJIT_IMM))
				op = SLJIT_MOV_SI;
			if (op == SLJIT_MOVU_UI && (src & SLJIT_IMM))
				op = SLJIT_MOVU_SI;
#endif
		}
#if (defined SLJIT_CONFIG_PPC_64 && SLJIT_CONFIG_PPC_64)
		else {
			/* Most operations expect sign extended arguments. */
			flags |= INT_DATA | SIGNED_DATA;
			if (src & SLJIT_IMM)
				srcw = (sljit_si)srcw;
		}
#endif
	}

	switch (op) {
	case SLJIT_MOV:
	case SLJIT_MOV_P:
#if (defined SLJIT_CONFIG_PPC_32 && SLJIT_CONFIG_PPC_32)
	case SLJIT_MOV_UI:
	case SLJIT_MOV_SI:
#endif
		return emit_op(compiler, SLJIT_MOV, flags | WORD_DATA, dst, dstw, TMP_REG1, 0, src, srcw);

#if (defined SLJIT_CONFIG_PPC_64 && SLJIT_CONFIG_PPC_64)
	case SLJIT_MOV_UI:
		return EMIT_MOV(SLJIT_MOV_UI, INT_DATA, (sljit_ui));

	case SLJIT_MOV_SI:
		return EMIT_MOV(SLJIT_MOV_SI, INT_DATA | SIGNED_DATA, (sljit_si));
#endif

	case SLJIT_MOV_UB:
		return EMIT_MOV(SLJIT_MOV_UB, BYTE_DATA, (sljit_ub));

	case SLJIT_MOV_SB:
		return EMIT_MOV(SLJIT_MOV_SB, BYTE_DATA | SIGNED_DATA, (sljit_sb));

	case SLJIT_MOV_UH:
		return EMIT_MOV(SLJIT_MOV_UH, HALF_DATA, (sljit_uh));

	case SLJIT_MOV_SH:
		return EMIT_MOV(SLJIT_MOV_SH, HALF_DATA | SIGNED_DATA, (sljit_sh));

	case SLJIT_MOVU:
	case SLJIT_MOVU_P:
#if (defined SLJIT_CONFIG_PPC_32 && SLJIT_CONFIG_PPC_32)
	case SLJIT_MOVU_UI:
	case SLJIT_MOVU_SI:
#endif
		return emit_op(compiler, SLJIT_MOV, flags | WORD_DATA | WRITE_BACK, dst, dstw, TMP_REG1, 0, src, srcw);

#if (defined SLJIT_CONFIG_PPC_64 && SLJIT_CONFIG_PPC_64)
	case SLJIT_MOVU_UI:
		return EMIT_MOV(SLJIT_MOV_UI, INT_DATA | WRITE_BACK, (sljit_ui));

	case SLJIT_MOVU_SI:
		return EMIT_MOV(SLJIT_MOV_SI, INT_DATA | SIGNED_DATA | WRITE_BACK, (sljit_si));
#endif

	case SLJIT_MOVU_UB:
		return EMIT_MOV(SLJIT_MOV_UB, BYTE_DATA | WRITE_BACK, (sljit_ub));

	case SLJIT_MOVU_SB:
		return EMIT_MOV(SLJIT_MOV_SB, BYTE_DATA | SIGNED_DATA | WRITE_BACK, (sljit_sb));

	case SLJIT_MOVU_UH:
		return EMIT_MOV(SLJIT_MOV_UH, HALF_DATA | WRITE_BACK, (sljit_uh));

	case SLJIT_MOVU_SH:
		return EMIT_MOV(SLJIT_MOV_SH, HALF_DATA | SIGNED_DATA | WRITE_BACK, (sljit_sh));

	case SLJIT_NOT:
		return emit_op(compiler, SLJIT_NOT, flags, dst, dstw, TMP_REG1, 0, src, srcw);

	case SLJIT_NEG:
		return emit_op(compiler, SLJIT_NEG, flags, dst, dstw, TMP_REG1, 0, src, srcw);

	case SLJIT_CLZ:
#if (defined SLJIT_CONFIG_PPC_64 && SLJIT_CONFIG_PPC_64)
		return emit_op(compiler, SLJIT_CLZ, flags | (!(op_flags & SLJIT_INT_OP) ? 0 : ALT_FORM1), dst, dstw, TMP_REG1, 0, src, srcw);
#else
		return emit_op(compiler, SLJIT_CLZ, flags, dst, dstw, TMP_REG1, 0, src, srcw);
#endif
	}

	return SLJIT_SUCCESS;
}

#undef EMIT_MOV

#define TEST_SL_IMM(src, srcw) \
	(((src) & SLJIT_IMM) && (srcw) <= SIMM_MAX && (srcw) >= SIMM_MIN)

#define TEST_UL_IMM(src, srcw) \
	(((src) & SLJIT_IMM) && !((srcw) & ~0xffff))

#if (defined SLJIT_CONFIG_PPC_64 && SLJIT_CONFIG_PPC_64)
#define TEST_SH_IMM(src, srcw) \
	(((src) & SLJIT_IMM) && !((srcw) & 0xffff) && (srcw) <= SLJIT_W(0x7fffffff) && (srcw) >= SLJIT_W(-0x80000000))
#else
#define TEST_SH_IMM(src, srcw) \
	(((src) & SLJIT_IMM) && !((srcw) & 0xffff))
#endif

#define TEST_UH_IMM(src, srcw) \
	(((src) & SLJIT_IMM) && !((srcw) & ~0xffff0000))

#if (defined SLJIT_CONFIG_PPC_64 && SLJIT_CONFIG_PPC_64)
#define TEST_ADD_IMM(src, srcw) \
	(((src) & SLJIT_IMM) && (srcw) <= SLJIT_W(0x7fff7fff) && (srcw) >= SLJIT_W(-0x80000000))
#else
#define TEST_ADD_IMM(src, srcw) \
	((src) & SLJIT_IMM)
#endif

#if (defined SLJIT_CONFIG_PPC_64 && SLJIT_CONFIG_PPC_64)
#define TEST_UI_IMM(src, srcw) \
	(((src) & SLJIT_IMM) && !((srcw) & ~0xffffffff))
#else
#define TEST_UI_IMM(src, srcw) \
	((src) & SLJIT_IMM)
#endif

SLJIT_API_FUNC_ATTRIBUTE sljit_si sljit_emit_op2(struct sljit_compiler *compiler, sljit_si op,
	sljit_si dst, sljit_sw dstw,
	sljit_si src1, sljit_sw src1w,
	sljit_si src2, sljit_sw src2w)
{
	sljit_si flags = GET_FLAGS(op) ? ALT_SET_FLAGS : 0;

	CHECK_ERROR();
	check_sljit_emit_op2(compiler, op, dst, dstw, src1, src1w, src2, src2w);
	ADJUST_LOCAL_OFFSET(dst, dstw);
	ADJUST_LOCAL_OFFSET(src1, src1w);
	ADJUST_LOCAL_OFFSET(src2, src2w);

	if ((src1 & SLJIT_IMM) && src1w == 0)
		src1 = ZERO_REG;
	if ((src2 & SLJIT_IMM) && src2w == 0)
		src2 = ZERO_REG;

#if (defined SLJIT_CONFIG_PPC_64 && SLJIT_CONFIG_PPC_64)
	if (op & SLJIT_INT_OP) {
		/* Most operations expect sign extended arguments. */
		flags |= INT_DATA | SIGNED_DATA;
		if (src1 & SLJIT_IMM)
			src1w = (sljit_si)(src1w);
		if (src2 & SLJIT_IMM)
			src2w = (sljit_si)(src2w);
		if (GET_FLAGS(op))
			flags |= ALT_SIGN_EXT;
	}
#endif
	if (op & SLJIT_SET_O)
		FAIL_IF(push_inst(compiler, MTXER | S(ZERO_REG)));

	switch (GET_OPCODE(op)) {
	case SLJIT_ADD:
		if (!GET_FLAGS(op) && ((src1 | src2) & SLJIT_IMM)) {
			if (TEST_SL_IMM(src2, src2w)) {
				compiler->imm = src2w & 0xffff;
				return emit_op(compiler, SLJIT_ADD, flags | ALT_FORM1, dst, dstw, src1, src1w, TMP_REG2, 0);
			}
			if (TEST_SL_IMM(src1, src1w)) {
				compiler->imm = src1w & 0xffff;
				return emit_op(compiler, SLJIT_ADD, flags | ALT_FORM1, dst, dstw, src2, src2w, TMP_REG2, 0);
			}
			if (TEST_SH_IMM(src2, src2w)) {
				compiler->imm = (src2w >> 16) & 0xffff;
				return emit_op(compiler, SLJIT_ADD, flags | ALT_FORM2, dst, dstw, src1, src1w, TMP_REG2, 0);
			}
			if (TEST_SH_IMM(src1, src1w)) {
				compiler->imm = (src1w >> 16) & 0xffff;
				return emit_op(compiler, SLJIT_ADD, flags | ALT_FORM2, dst, dstw, src2, src2w, TMP_REG2, 0);
			}
			/* Range between -1 and -32768 is covered above. */
			if (TEST_ADD_IMM(src2, src2w)) {
				compiler->imm = src2w & 0xffffffff;
				return emit_op(compiler, SLJIT_ADD, flags | ALT_FORM4, dst, dstw, src1, src1w, TMP_REG2, 0);
			}
			if (TEST_ADD_IMM(src1, src1w)) {
				compiler->imm = src1w & 0xffffffff;
				return emit_op(compiler, SLJIT_ADD, flags | ALT_FORM4, dst, dstw, src2, src2w, TMP_REG2, 0);
			}
		}
		if (!(GET_FLAGS(op) & (SLJIT_SET_E | SLJIT_SET_O))) {
			if (TEST_SL_IMM(src2, src2w)) {
				compiler->imm = src2w & 0xffff;
				return emit_op(compiler, SLJIT_ADD, flags | ALT_FORM3, dst, dstw, src1, src1w, TMP_REG2, 0);
			}
			if (TEST_SL_IMM(src1, src1w)) {
				compiler->imm = src1w & 0xffff;
				return emit_op(compiler, SLJIT_ADD, flags | ALT_FORM3, dst, dstw, src2, src2w, TMP_REG2, 0);
			}
		}
		return emit_op(compiler, SLJIT_ADD, flags, dst, dstw, src1, src1w, src2, src2w);

	case SLJIT_ADDC:
		return emit_op(compiler, SLJIT_ADDC, flags | (!(op & SLJIT_KEEP_FLAGS) ? 0 : ALT_FORM1), dst, dstw, src1, src1w, src2, src2w);

	case SLJIT_SUB:
		if (!GET_FLAGS(op) && ((src1 | src2) & SLJIT_IMM)) {
			if (TEST_SL_IMM(src2, -src2w)) {
				compiler->imm = (-src2w) & 0xffff;
				return emit_op(compiler, SLJIT_ADD, flags | ALT_FORM1, dst, dstw, src1, src1w, TMP_REG2, 0);
			}
			if (TEST_SL_IMM(src1, src1w)) {
				compiler->imm = src1w & 0xffff;
				return emit_op(compiler, SLJIT_SUB, flags | ALT_FORM1, dst, dstw, src2, src2w, TMP_REG2, 0);
			}
			if (TEST_SH_IMM(src2, -src2w)) {
				compiler->imm = ((-src2w) >> 16) & 0xffff;
				return emit_op(compiler, SLJIT_ADD, flags | ALT_FORM2, dst, dstw, src1, src1w, TMP_REG2, 0);
			}
			/* Range between -1 and -32768 is covered above. */
			if (TEST_ADD_IMM(src2, -src2w)) {
				compiler->imm = -src2w & 0xffffffff;
				return emit_op(compiler, SLJIT_ADD, flags | ALT_FORM4, dst, dstw, src1, src1w, TMP_REG2, 0);
			}
		}
		if (dst == SLJIT_UNUSED && (op & (SLJIT_SET_E | SLJIT_SET_S | SLJIT_SET_U)) && !(op & (SLJIT_SET_O | SLJIT_SET_C))) {
			if (!(op & SLJIT_SET_U)) {
				/* We know ALT_SIGN_EXT is set if it is an SLJIT_INT_OP on 64 bit systems. */
				if (TEST_SL_IMM(src2, src2w)) {
					compiler->imm = src2w & 0xffff;
					return emit_op(compiler, SLJIT_SUB, flags | ALT_FORM2, dst, dstw, src1, src1w, TMP_REG2, 0);
				}
				if (GET_FLAGS(op) == SLJIT_SET_E && TEST_SL_IMM(src1, src1w)) {
					compiler->imm = src1w & 0xffff;
					return emit_op(compiler, SLJIT_SUB, flags | ALT_FORM2, dst, dstw, src2, src2w, TMP_REG2, 0);
				}
			}
			if (!(op & (SLJIT_SET_E | SLJIT_SET_S))) {
				/* We know ALT_SIGN_EXT is set if it is an SLJIT_INT_OP on 64 bit systems. */
				if (TEST_UL_IMM(src2, src2w)) {
					compiler->imm = src2w & 0xffff;
					return emit_op(compiler, SLJIT_SUB, flags | ALT_FORM3, dst, dstw, src1, src1w, TMP_REG2, 0);
				}
				return emit_op(compiler, SLJIT_SUB, flags | ALT_FORM4, dst, dstw, src1, src1w, src2, src2w);
			}
			if ((src2 & SLJIT_IMM) && src2w >= 0 && src2w <= 0x7fff) {
				compiler->imm = src2w;
				return emit_op(compiler, SLJIT_SUB, flags | ALT_FORM2 | ALT_FORM3, dst, dstw, src1, src1w, TMP_REG2, 0);
			}
			return emit_op(compiler, SLJIT_SUB, flags | ((op & SLJIT_SET_U) ? ALT_FORM4 : 0) | ((op & (SLJIT_SET_E | SLJIT_SET_S)) ? ALT_FORM5 : 0), dst, dstw, src1, src1w, src2, src2w);
		}
		if (!(op & (SLJIT_SET_E | SLJIT_SET_S | SLJIT_SET_U | SLJIT_SET_O))) {
			if (TEST_SL_IMM(src2, -src2w)) {
				compiler->imm = (-src2w) & 0xffff;
				return emit_op(compiler, SLJIT_ADD, flags | ALT_FORM3, dst, dstw, src1, src1w, TMP_REG2, 0);
			}
		}
		/* We know ALT_SIGN_EXT is set if it is an SLJIT_INT_OP on 64 bit systems. */
		return emit_op(compiler, SLJIT_SUB, flags | (!(op & SLJIT_SET_U) ? 0 : ALT_FORM6), dst, dstw, src1, src1w, src2, src2w);

	case SLJIT_SUBC:
		return emit_op(compiler, SLJIT_SUBC, flags | (!(op & SLJIT_KEEP_FLAGS) ? 0 : ALT_FORM1), dst, dstw, src1, src1w, src2, src2w);

	case SLJIT_MUL:
#if (defined SLJIT_CONFIG_PPC_64 && SLJIT_CONFIG_PPC_64)
		if (op & SLJIT_INT_OP)
			flags |= ALT_FORM2;
#endif
		if (!GET_FLAGS(op)) {
			if (TEST_SL_IMM(src2, src2w)) {
				compiler->imm = src2w & 0xffff;
				return emit_op(compiler, SLJIT_MUL, flags | ALT_FORM1, dst, dstw, src1, src1w, TMP_REG2, 0);
			}
			if (TEST_SL_IMM(src1, src1w)) {
				compiler->imm = src1w & 0xffff;
				return emit_op(compiler, SLJIT_MUL, flags | ALT_FORM1, dst, dstw, src2, src2w, TMP_REG2, 0);
			}
		}
		return emit_op(compiler, SLJIT_MUL, flags, dst, dstw, src1, src1w, src2, src2w);

	case SLJIT_AND:
	case SLJIT_OR:
	case SLJIT_XOR:
		/* Commutative unsigned operations. */
		if (!GET_FLAGS(op) || GET_OPCODE(op) == SLJIT_AND) {
			if (TEST_UL_IMM(src2, src2w)) {
				compiler->imm = src2w;
				return emit_op(compiler, GET_OPCODE(op), flags | ALT_FORM1, dst, dstw, src1, src1w, TMP_REG2, 0);
			}
			if (TEST_UL_IMM(src1, src1w)) {
				compiler->imm = src1w;
				return emit_op(compiler, GET_OPCODE(op), flags | ALT_FORM1, dst, dstw, src2, src2w, TMP_REG2, 0);
			}
			if (TEST_UH_IMM(src2, src2w)) {
				compiler->imm = (src2w >> 16) & 0xffff;
				return emit_op(compiler, GET_OPCODE(op), flags | ALT_FORM2, dst, dstw, src1, src1w, TMP_REG2, 0);
			}
			if (TEST_UH_IMM(src1, src1w)) {
				compiler->imm = (src1w >> 16) & 0xffff;
				return emit_op(compiler, GET_OPCODE(op), flags | ALT_FORM2, dst, dstw, src2, src2w, TMP_REG2, 0);
			}
		}
		if (!GET_FLAGS(op) && GET_OPCODE(op) != SLJIT_AND) {
			if (TEST_UI_IMM(src2, src2w)) {
				compiler->imm = src2w;
				return emit_op(compiler, GET_OPCODE(op), flags | ALT_FORM3, dst, dstw, src1, src1w, TMP_REG2, 0);
			}
			if (TEST_UI_IMM(src1, src1w)) {
				compiler->imm = src1w;
				return emit_op(compiler, GET_OPCODE(op), flags | ALT_FORM3, dst, dstw, src2, src2w, TMP_REG2, 0);
			}
		}
		return emit_op(compiler, GET_OPCODE(op), flags, dst, dstw, src1, src1w, src2, src2w);

	case SLJIT_ASHR:
		if (op & SLJIT_KEEP_FLAGS)
			flags |= ALT_FORM3;
		/* Fall through. */
	case SLJIT_SHL:
	case SLJIT_LSHR:
#if (defined SLJIT_CONFIG_PPC_64 && SLJIT_CONFIG_PPC_64)
		if (op & SLJIT_INT_OP)
			flags |= ALT_FORM2;
#endif
		if (src2 & SLJIT_IMM) {
			compiler->imm = src2w;
			return emit_op(compiler, GET_OPCODE(op), flags | ALT_FORM1, dst, dstw, src1, src1w, TMP_REG2, 0);
		}
		return emit_op(compiler, GET_OPCODE(op), flags, dst, dstw, src1, src1w, src2, src2w);
	}

	return SLJIT_SUCCESS;
}

SLJIT_API_FUNC_ATTRIBUTE sljit_si sljit_get_register_index(sljit_si reg)
{
	check_sljit_get_register_index(reg);
	return reg_map[reg];
}

SLJIT_API_FUNC_ATTRIBUTE sljit_si sljit_emit_op_custom(struct sljit_compiler *compiler,
	void *instruction, sljit_si size)
{
	CHECK_ERROR();
	check_sljit_emit_op_custom(compiler, instruction, size);
	SLJIT_ASSERT(size == 4);

	return push_inst(compiler, *(sljit_ins*)instruction);
}

/* --------------------------------------------------------------------- */
/*  Floating point operators                                             */
/* --------------------------------------------------------------------- */

SLJIT_API_FUNC_ATTRIBUTE sljit_si sljit_is_fpu_available(void)
{
	/* Always available. */
	return 1;
}

#define FLOAT_DATA(op) (DOUBLE_DATA | ((op & SLJIT_SINGLE_OP) >> 6))
#define SELECT_FOP(op, single, double) ((op & SLJIT_SINGLE_OP) ? single : double)

SLJIT_API_FUNC_ATTRIBUTE sljit_si sljit_emit_fop1(struct sljit_compiler *compiler, sljit_si op,
	sljit_si dst, sljit_sw dstw,
	sljit_si src, sljit_sw srcw)
{
	sljit_si dst_fr;

	CHECK_ERROR();
	check_sljit_emit_fop1(compiler, op, dst, dstw, src, srcw);
	SLJIT_COMPILE_ASSERT((SLJIT_SINGLE_OP == 0x100) && !(DOUBLE_DATA & 0x4), float_transfer_bit_error);

	compiler->cache_arg = 0;
	compiler->cache_argw = 0;

	if (GET_OPCODE(op) == SLJIT_CMPD) {
		if (dst > SLJIT_FLOAT_REG6) {
			FAIL_IF(emit_op_mem2(compiler, FLOAT_DATA(op) | LOAD_DATA, TMP_FREG1, dst, dstw, src, srcw));
			dst = TMP_FREG1;
		}

		if (src > SLJIT_FLOAT_REG6) {
			FAIL_IF(emit_op_mem2(compiler, FLOAT_DATA(op) | LOAD_DATA, TMP_FREG2, src, srcw, 0, 0));
			src = TMP_FREG2;
		}

		return push_inst(compiler, FCMPU | CRD(4) | FA(dst) | FB(src));
	}

	dst_fr = (dst > SLJIT_FLOAT_REG6) ? TMP_FREG1 : dst;

	if (src > SLJIT_FLOAT_REG6) {
		FAIL_IF(emit_op_mem2(compiler, FLOAT_DATA(op) | LOAD_DATA, dst_fr, src, srcw, dst, dstw));
		src = dst_fr;
	}

	switch (GET_OPCODE(op)) {
		case SLJIT_MOVD:
			if (src != dst_fr && dst_fr != TMP_FREG1)
				FAIL_IF(push_inst(compiler, FMR | FD(dst_fr) | FB(src)));
			break;
		case SLJIT_NEGD:
			FAIL_IF(push_inst(compiler, FNEG | FD(dst_fr) | FB(src)));
			break;
		case SLJIT_ABSD:
			FAIL_IF(push_inst(compiler, FABS | FD(dst_fr) | FB(src)));
			break;
	}

	if (dst_fr == TMP_FREG1) {
		if (GET_OPCODE(op) == SLJIT_MOVD)
			dst_fr = src;
		FAIL_IF(emit_op_mem2(compiler, FLOAT_DATA(op), dst_fr, dst, dstw, 0, 0));
	}

	return SLJIT_SUCCESS;
}

SLJIT_API_FUNC_ATTRIBUTE sljit_si sljit_emit_fop2(struct sljit_compiler *compiler, sljit_si op,
	sljit_si dst, sljit_sw dstw,
	sljit_si src1, sljit_sw src1w,
	sljit_si src2, sljit_sw src2w)
{
	sljit_si dst_fr, flags = 0;

	CHECK_ERROR();
	check_sljit_emit_fop2(compiler, op, dst, dstw, src1, src1w, src2, src2w);

	compiler->cache_arg = 0;
	compiler->cache_argw = 0;

	dst_fr = (dst > SLJIT_FLOAT_REG6) ? TMP_FREG2 : dst;

	if (src1 > SLJIT_FLOAT_REG6) {
		if (getput_arg_fast(compiler, FLOAT_DATA(op) | LOAD_DATA, TMP_FREG1, src1, src1w)) {
			FAIL_IF(compiler->error);
			src1 = TMP_FREG1;
		} else
			flags |= ALT_FORM1;
	}

	if (src2 > SLJIT_FLOAT_REG6) {
		if (getput_arg_fast(compiler, FLOAT_DATA(op) | LOAD_DATA, TMP_FREG2, src2, src2w)) {
			FAIL_IF(compiler->error);
			src2 = TMP_FREG2;
		} else
			flags |= ALT_FORM2;
	}

	if ((flags & (ALT_FORM1 | ALT_FORM2)) == (ALT_FORM1 | ALT_FORM2)) {
		if (!can_cache(src1, src1w, src2, src2w) && can_cache(src1, src1w, dst, dstw)) {
			FAIL_IF(getput_arg(compiler, FLOAT_DATA(op) | LOAD_DATA, TMP_FREG2, src2, src2w, src1, src1w));
			FAIL_IF(getput_arg(compiler, FLOAT_DATA(op) | LOAD_DATA, TMP_FREG1, src1, src1w, dst, dstw));
		}
		else {
			FAIL_IF(getput_arg(compiler, FLOAT_DATA(op) | LOAD_DATA, TMP_FREG1, src1, src1w, src2, src2w));
			FAIL_IF(getput_arg(compiler, FLOAT_DATA(op) | LOAD_DATA, TMP_FREG2, src2, src2w, dst, dstw));
		}
	}
	else if (flags & ALT_FORM1)
		FAIL_IF(getput_arg(compiler, FLOAT_DATA(op) | LOAD_DATA, TMP_FREG1, src1, src1w, dst, dstw));
	else if (flags & ALT_FORM2)
		FAIL_IF(getput_arg(compiler, FLOAT_DATA(op) | LOAD_DATA, TMP_FREG2, src2, src2w, dst, dstw));

	if (flags & ALT_FORM1)
		src1 = TMP_FREG1;
	if (flags & ALT_FORM2)
		src2 = TMP_FREG2;

	switch (GET_OPCODE(op)) {
	case SLJIT_ADDD:
		FAIL_IF(push_inst(compiler, SELECT_FOP(op, FADDS, FADD) | FD(dst_fr) | FA(src1) | FB(src2)));
		break;

	case SLJIT_SUBD:
		FAIL_IF(push_inst(compiler, SELECT_FOP(op, FSUBS, FSUB) | FD(dst_fr) | FA(src1) | FB(src2)));
		break;

	case SLJIT_MULD:
		FAIL_IF(push_inst(compiler, SELECT_FOP(op, FMULS, FMUL) | FD(dst_fr) | FA(src1) | FC(src2) /* FMUL use FC as src2 */));
		break;

	case SLJIT_DIVD:
		FAIL_IF(push_inst(compiler, SELECT_FOP(op, FDIVS, FDIV) | FD(dst_fr) | FA(src1) | FB(src2)));
		break;
	}

	if (dst_fr == TMP_FREG2)
		FAIL_IF(emit_op_mem2(compiler, FLOAT_DATA(op), TMP_FREG2, dst, dstw, 0, 0));

	return SLJIT_SUCCESS;
}

#undef FLOAT_DATA
#undef SELECT_FOP

/* --------------------------------------------------------------------- */
/*  Other instructions                                                   */
/* --------------------------------------------------------------------- */

SLJIT_API_FUNC_ATTRIBUTE sljit_si sljit_emit_fast_enter(struct sljit_compiler *compiler, sljit_si dst, sljit_sw dstw)
{
	CHECK_ERROR();
	check_sljit_emit_fast_enter(compiler, dst, dstw);
	ADJUST_LOCAL_OFFSET(dst, dstw);

	if (dst >= SLJIT_TEMPORARY_REG1 && dst <= SLJIT_NO_REGISTERS)
		return push_inst(compiler, MFLR | D(dst));
	else if (dst & SLJIT_MEM) {
		FAIL_IF(push_inst(compiler, MFLR | D(TMP_REG2)));
		return emit_op(compiler, SLJIT_MOV, WORD_DATA, dst, dstw, TMP_REG1, 0, TMP_REG2, 0);
	}

	/* SLJIT_UNUSED is also possible, although highly unlikely. */
	return SLJIT_SUCCESS;
}

SLJIT_API_FUNC_ATTRIBUTE sljit_si sljit_emit_fast_return(struct sljit_compiler *compiler, sljit_si src, sljit_sw srcw)
{
	CHECK_ERROR();
	check_sljit_emit_fast_return(compiler, src, srcw);
	ADJUST_LOCAL_OFFSET(src, srcw);

	if (src >= SLJIT_TEMPORARY_REG1 && src <= SLJIT_NO_REGISTERS)
		FAIL_IF(push_inst(compiler, MTLR | S(src)));
	else {
		if (src & SLJIT_MEM)
			FAIL_IF(emit_op(compiler, SLJIT_MOV, WORD_DATA, TMP_REG2, 0, TMP_REG1, 0, src, srcw));
		else if (src & SLJIT_IMM)
			FAIL_IF(load_immediate(compiler, TMP_REG2, srcw));
		FAIL_IF(push_inst(compiler, MTLR | S(TMP_REG2)));
	}
	return push_inst(compiler, BLR);
}

/* --------------------------------------------------------------------- */
/*  Conditional instructions                                             */
/* --------------------------------------------------------------------- */

SLJIT_API_FUNC_ATTRIBUTE struct sljit_label* sljit_emit_label(struct sljit_compiler *compiler)
{
	struct sljit_label *label;

	CHECK_ERROR_PTR();
	check_sljit_emit_label(compiler);

	if (compiler->last_label && compiler->last_label->size == compiler->size)
		return compiler->last_label;

	label = (struct sljit_label*)ensure_abuf(compiler, sizeof(struct sljit_label));
	PTR_FAIL_IF(!label);
	set_label(label, compiler);
	return label;
}

static sljit_ins get_bo_bi_flags(sljit_si type)
{
	switch (type) {
	case SLJIT_C_EQUAL:
		return (12 << 21) | (2 << 16);

	case SLJIT_C_NOT_EQUAL:
		return (4 << 21) | (2 << 16);

	case SLJIT_C_LESS:
	case SLJIT_C_FLOAT_LESS:
		return (12 << 21) | ((4 + 0) << 16);

	case SLJIT_C_GREATER_EQUAL:
	case SLJIT_C_FLOAT_GREATER_EQUAL:
		return (4 << 21) | ((4 + 0) << 16);

	case SLJIT_C_GREATER:
	case SLJIT_C_FLOAT_GREATER:
		return (12 << 21) | ((4 + 1) << 16);

	case SLJIT_C_LESS_EQUAL:
	case SLJIT_C_FLOAT_LESS_EQUAL:
		return (4 << 21) | ((4 + 1) << 16);

	case SLJIT_C_SIG_LESS:
		return (12 << 21) | (0 << 16);

	case SLJIT_C_SIG_GREATER_EQUAL:
		return (4 << 21) | (0 << 16);

	case SLJIT_C_SIG_GREATER:
		return (12 << 21) | (1 << 16);

	case SLJIT_C_SIG_LESS_EQUAL:
		return (4 << 21) | (1 << 16);

	case SLJIT_C_OVERFLOW:
	case SLJIT_C_MUL_OVERFLOW:
		return (12 << 21) | (3 << 16);

	case SLJIT_C_NOT_OVERFLOW:
	case SLJIT_C_MUL_NOT_OVERFLOW:
		return (4 << 21) | (3 << 16);

	case SLJIT_C_FLOAT_EQUAL:
		return (12 << 21) | ((4 + 2) << 16);

	case SLJIT_C_FLOAT_NOT_EQUAL:
		return (4 << 21) | ((4 + 2) << 16);

	case SLJIT_C_FLOAT_UNORDERED:
		return (12 << 21) | ((4 + 3) << 16);

	case SLJIT_C_FLOAT_ORDERED:
		return (4 << 21) | ((4 + 3) << 16);

	default:
		SLJIT_ASSERT(type >= SLJIT_JUMP && type <= SLJIT_CALL3);
		return (20 << 21);
	}
}

SLJIT_API_FUNC_ATTRIBUTE struct sljit_jump* sljit_emit_jump(struct sljit_compiler *compiler, sljit_si type)
{
	struct sljit_jump *jump;
	sljit_ins bo_bi_flags;

	CHECK_ERROR_PTR();
	check_sljit_emit_jump(compiler, type);

	bo_bi_flags = get_bo_bi_flags(type & 0xff);
	if (!bo_bi_flags)
		return NULL;

	jump = (struct sljit_jump*)ensure_abuf(compiler, sizeof(struct sljit_jump));
	PTR_FAIL_IF(!jump);
	set_jump(jump, compiler, type & SLJIT_REWRITABLE_JUMP);
	type &= 0xff;

	/* In PPC, we don't need to touch the arguments. */
	if (type >= SLJIT_JUMP)
		jump->flags |= UNCOND_B;

	PTR_FAIL_IF(emit_const(compiler, TMP_REG1, 0));
	PTR_FAIL_IF(push_inst(compiler, MTCTR | S(TMP_REG1)));
	jump->addr = compiler->size;
	PTR_FAIL_IF(push_inst(compiler, BCCTR | bo_bi_flags | (type >= SLJIT_FAST_CALL ? 1 : 0)));
	return jump;
}

SLJIT_API_FUNC_ATTRIBUTE sljit_si sljit_emit_ijump(struct sljit_compiler *compiler, sljit_si type, sljit_si src, sljit_sw srcw)
{
	struct sljit_jump *jump = NULL;
	sljit_si src_r;

	CHECK_ERROR();
	check_sljit_emit_ijump(compiler, type, src, srcw);
	ADJUST_LOCAL_OFFSET(src, srcw);

	if (src >= SLJIT_TEMPORARY_REG1 && src <= SLJIT_NO_REGISTERS)
		src_r = src;
	else if (src & SLJIT_IMM) {
		jump = (struct sljit_jump*)ensure_abuf(compiler, sizeof(struct sljit_jump));
		FAIL_IF(!jump);
		set_jump(jump, compiler, JUMP_ADDR | UNCOND_B);
		jump->u.target = srcw;

		FAIL_IF(emit_const(compiler, TMP_REG2, 0));
		src_r = TMP_REG2;
	}
	else {
		FAIL_IF(emit_op(compiler, SLJIT_MOV, WORD_DATA, TMP_REG2, 0, TMP_REG1, 0, src, srcw));
		src_r = TMP_REG2;
	}

	FAIL_IF(push_inst(compiler, MTCTR | S(src_r)));
	if (jump)
		jump->addr = compiler->size;
	return push_inst(compiler, BCCTR | (20 << 21) | (type >= SLJIT_FAST_CALL ? 1 : 0));
}

/* Get a bit from CR, all other bits are zeroed. */
#define GET_CR_BIT(bit, dst) \
	FAIL_IF(push_inst(compiler, MFCR | D(dst))); \
	FAIL_IF(push_inst(compiler, RLWINM | S(dst) | A(dst) | ((1 + (bit)) << 11) | (31 << 6) | (31 << 1)));

#define INVERT_BIT(dst) \
	FAIL_IF(push_inst(compiler, XORI | S(dst) | A(dst) | 0x1));

SLJIT_API_FUNC_ATTRIBUTE sljit_si sljit_emit_cond_value(struct sljit_compiler *compiler, sljit_si op, sljit_si dst, sljit_sw dstw, sljit_si type)
{
	sljit_si reg;

	CHECK_ERROR();
	check_sljit_emit_cond_value(compiler, op, dst, dstw, type);
	ADJUST_LOCAL_OFFSET(dst, dstw);

	if (dst == SLJIT_UNUSED)
		return SLJIT_SUCCESS;

	reg = (op == SLJIT_MOV && dst >= SLJIT_TEMPORARY_REG1 && dst <= SLJIT_NO_REGISTERS) ? dst : TMP_REG2;

	switch (type) {
	case SLJIT_C_EQUAL:
		GET_CR_BIT(2, reg);
		break;

	case SLJIT_C_NOT_EQUAL:
		GET_CR_BIT(2, reg);
		INVERT_BIT(reg);
		break;

	case SLJIT_C_LESS:
	case SLJIT_C_FLOAT_LESS:
		GET_CR_BIT(4 + 0, reg);
		break;

	case SLJIT_C_GREATER_EQUAL:
	case SLJIT_C_FLOAT_GREATER_EQUAL:
		GET_CR_BIT(4 + 0, reg);
		INVERT_BIT(reg);
		break;

	case SLJIT_C_GREATER:
	case SLJIT_C_FLOAT_GREATER:
		GET_CR_BIT(4 + 1, reg);
		break;

	case SLJIT_C_LESS_EQUAL:
	case SLJIT_C_FLOAT_LESS_EQUAL:
		GET_CR_BIT(4 + 1, reg);
		INVERT_BIT(reg);
		break;

	case SLJIT_C_SIG_LESS:
		GET_CR_BIT(0, reg);
		break;

	case SLJIT_C_SIG_GREATER_EQUAL:
		GET_CR_BIT(0, reg);
		INVERT_BIT(reg);
		break;

	case SLJIT_C_SIG_GREATER:
		GET_CR_BIT(1, reg);
		break;

	case SLJIT_C_SIG_LESS_EQUAL:
		GET_CR_BIT(1, reg);
		INVERT_BIT(reg);
		break;

	case SLJIT_C_OVERFLOW:
	case SLJIT_C_MUL_OVERFLOW:
		GET_CR_BIT(3, reg);
		break;

	case SLJIT_C_NOT_OVERFLOW:
	case SLJIT_C_MUL_NOT_OVERFLOW:
		GET_CR_BIT(3, reg);
		INVERT_BIT(reg);
		break;

	case SLJIT_C_FLOAT_EQUAL:
		GET_CR_BIT(4 + 2, reg);
		break;

	case SLJIT_C_FLOAT_NOT_EQUAL:
		GET_CR_BIT(4 + 2, reg);
		INVERT_BIT(reg);
		break;

	case SLJIT_C_FLOAT_UNORDERED:
		GET_CR_BIT(4 + 3, reg);
		break;

	case SLJIT_C_FLOAT_ORDERED:
		GET_CR_BIT(4 + 3, reg);
		INVERT_BIT(reg);
		break;

	default:
		SLJIT_ASSERT_STOP();
		break;
	}

	if (GET_OPCODE(op) == SLJIT_OR)
		return emit_op(compiler, SLJIT_OR, GET_FLAGS(op) ? ALT_SET_FLAGS : 0, dst, dstw, dst, dstw, TMP_REG2, 0);

	return (reg == TMP_REG2) ? emit_op(compiler, SLJIT_MOV, WORD_DATA, dst, dstw, TMP_REG1, 0, TMP_REG2, 0) : SLJIT_SUCCESS;
}

SLJIT_API_FUNC_ATTRIBUTE struct sljit_const* sljit_emit_const(struct sljit_compiler *compiler, sljit_si dst, sljit_sw dstw, sljit_sw init_value)
{
	struct sljit_const *const_;
	sljit_si reg;

	CHECK_ERROR_PTR();
	check_sljit_emit_const(compiler, dst, dstw, init_value);
	ADJUST_LOCAL_OFFSET(dst, dstw);

	const_ = (struct sljit_const*)ensure_abuf(compiler, sizeof(struct sljit_const));
	PTR_FAIL_IF(!const_);
	set_const(const_, compiler);

	reg = (dst >= SLJIT_TEMPORARY_REG1 && dst <= SLJIT_NO_REGISTERS) ? dst : TMP_REG2;

	PTR_FAIL_IF(emit_const(compiler, reg, init_value));

	if (dst & SLJIT_MEM)
		PTR_FAIL_IF(emit_op(compiler, SLJIT_MOV, WORD_DATA, dst, dstw, TMP_REG1, 0, TMP_REG2, 0));
	return const_;
}
