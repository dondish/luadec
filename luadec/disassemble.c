#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include "lua.h"
#include "lauxlib.h"
#include "ldebug.h"
#include "lobject.h"
#include "lopcodes.h"
#include "lundump.h"
#include "lstring.h"

#include "lua-compat.h"
#include "StringBuffer.h"
#include "proto.h"
#include "disassemble.h"

extern int process_sub;           /* process sub functions? */

Proto* findSubFunction(Proto* f, const char* funcnumstr, char* realfuncnumstr);

void luadec_disassembleSubFunction(Proto* f, int dflag, const char* funcnumstr) {
	char* realfuncnumstr = (char*)calloc(strlen(funcnumstr) + 10, sizeof(char));
	Proto* cf = findSubFunction(f, funcnumstr, realfuncnumstr);
	if (cf == NULL) {
		fprintf(stderr, "No such sub function num : %s , use -pn option to get available num.\n", funcnumstr);
		free(realfuncnumstr);
		return;
	}
	luadec_disassemble(cf, dflag, realfuncnumstr);
	free(realfuncnumstr);
}

char* RegOrConst(const Proto* f, int r) {
	if (ISK(r)) {
		return DecompileConstant(f, INDEXK(r));
	} else {
		char* tmp = (char*)calloc(10, sizeof(char*));
		sprintf(tmp, "R%d", r);
		return tmp;
	}
}

static inline const char* get_comp_op_i(int to_invert, int operator) {
	switch (operator)
	{
	case OP_EQI:
		if (to_invert) {
			return "~=";
		}
		return "==";
	case OP_LTI:
		if (to_invert) {
			return ">=";
		}
		return "<";
	case OP_LEI:
		if (to_invert) {
			return ">";
		}
		return "<=";
	case OP_GTI:
		if (to_invert) {
			return "<=";
		}
		return ">";
	case OP_GEI:
		if (to_invert) {
			return "<";
		}
		return ">=";
	default:
		return "UnkOp";
	}
}

static inline const char* get_mm_func_name_from_tm(TMS tm) {
	switch (tm)
	{
	case TM_INDEX:
		return "__index";
	case TM_NEWINDEX:
		return "__newindex";
	case TM_GC: 
		return "__gc";
	case TM_MODE:
		return "__mode";
	case TM_LEN:
		return "__len";
	case TM_EQ:
		return "__eq";
	case TM_ADD:
		return "__add";
	case TM_SUB:
		return "__sub";
	case TM_MUL:
		return "__mul";
	case TM_MOD:
		return "__mod";
	case TM_POW:
		return "__pow";
	case TM_DIV:
		return "__div";
	case TM_IDIV:
		return "__idiv";
	case TM_BAND:
		return "__band";
	case TM_BOR:
		return "__bor";
	case TM_BXOR:
		return "__bxor";
	case TM_SHL:
		return "__shl";
	case TM_SHR:
		return "__shr";
	case TM_UNM:
		return "__unm";
	case TM_BNOT:
		return "__bnot";
	case TM_LT:
		return "__lt";
	case TM_LE:
		return "__le";
	case TM_CONCAT:
		return "__concat";
	case TM_CALL:
		return "__call";
	case TM_CLOSE:
		return "__close";
	default:
		return NULL;
	}

}

void luadec_disassemble(Proto* fwork, int dflag, const char* name) {
	char line[128];
	StringBuffer* lend = StringBuffer_newBySize(MAXCONSTSIZE + 128);

	char tmp[MAXCONSTSIZE+128];
	char tmp2[MAXCONSTSIZE+128];
	char* tmpconstant1 = NULL;
	char* tmpconstant2 = NULL;

	Proto* f = fwork;
	int pc;
	int name_len = name ? strlen(name) : 0;
	int ignoreNext = 0;

	printf("; Function:        %s\n", name);
	printf("; Defined at line: %d\n", f->linedefined);
	printf("; #Upvalues:       %d\n", NUPS(f));
	printf("; #Parameters:     %d\n", f->numparams);
	printf("; Is_vararg:       %d\n", f->is_vararg);
	printf("; Max Stack Size:  %d\n", f->maxstacksize);
	printf("\n");

	ignoreNext = 0;
	for (pc=0; pc<f->sizecode; pc++) {
		Instruction i = f->code[pc];
		OpCode o = GET_OPCODE(i);
		int a = GETARG_A(i);
		int b = GETARG_B(i);
		int c = GETARG_C(i);
		int bc = GETARG_Bx(i);
		int sbc = GETARG_sBx(i);
#if LUA_VERSION_NUM == 504
		int bx = GETARG_Bx(i);
		int sb = GETARG_sB(i);
		int sc = GETARG_sC(i);
		int sj = GETARG_sJ(i);
		int k = GETARG_k(i);
#endif
		int dest;
		sprintf(line,"");
		StringBuffer_set(lend,"");

		if (ignoreNext) {
			ignoreNext--;
			printf("%5d [-]: %u\n", pc, i);
			continue;
		}
		switch (o) {
		case OP_MOVE:
			/*	A B	R(A) := R(B)					*/
			sprintf(line,"R%d R%d",a,b);
			StringBuffer_printf(lend,"R%d := R%d",a,b);
			break;
		case OP_LOADK:
			/*	A Bx	R(A) := Kst(Bx)					*/
			sprintf(line,"R%d K%d",a,bc);
			tmpconstant1 = DecompileConstant(f,bc);
			StringBuffer_printf(lend,"R%d := %s",a,tmpconstant1);
			break;
#if LUA_VERSION_NUM == 504
		case OP_LOADI:
			/*	A sBx	R[A] := sBx					*/
			sprintf(line,"R%d %d",a,bc);
			StringBuffer_printf(lend,"R%d := %s",a,bc);
			break;
		case OP_LOADF:
			/*	A sBx	R[A] := (lua_Number)sBx				*/
			sprintf(line,"R%d "LUA_NUMBER_FMT,a,cast_num(bc));
			StringBuffer_printf(lend,"R%d := "LUA_NUMBER_FMT,a,cast_num(bc));
			break;
#endif
#if LUA_VERSION_NUM == 502 || LUA_VERSION_NUM == 503 || LUA_VERSION_NUM == 504
		case OP_LOADKX:
		{
			/*	A 	R(A) := Kst(extra arg)				*/
			int ax = GETARG_Ax(f->code[pc+1]);
			sprintf(line,"R%d",a);
			tmpconstant1 = DecompileConstant(f,ax);
			StringBuffer_printf(lend,"R%d := K%d , %s",a,ax,tmpconstant1);
			break;
		}
		case OP_EXTRAARG:
		{
			/*	Ax	extra (larger) argument for previous opcode	*/
			int ax = GETARG_Ax(i);
			sprintf(line,"%d",ax);
			break;
		}
#endif
#if LUA_VERSION_NUM == 504
		case OP_LOADFALSE:
			sprintf(line, "R%d",a);
			StringBuffer_printf(lend, "R%d := false", a);
			break;
		case OP_LFALSESKIP:
			sprintf(line, "R%d",a);
			StringBuffer_printf(lend, "R%d := false; goto %d", a, pc+2);
			break;
		case OP_LOADTRUE:
			sprintf(line, "R%d",a);
			StringBuffer_printf(lend, "R%d := true", a);
			break;
#else
		case OP_LOADBOOL:
			/*	A B C	R(A) := (Bool)B; if (C) pc++			*/
			sprintf(line,"R%d %d %d",a,b,c);
			if (c) {
				StringBuffer_printf(lend,"R%d := %s; goto %d",a,(b?"true":"false"),pc+2);
			} else {
				StringBuffer_printf(lend,"R%d := %s",a,(b?"true":"false"));
			}
			break;
#endif
		case OP_LOADNIL:
		{
#if LUA_VERSION_NUM == 501
			/*	A B	R(A), ..., R(B) := nil		*/
			int rb = b;
			sprintf(line, "R%d R%d", a, b);
#endif
#if LUA_VERSION_NUM == 502 || LUA_VERSION_NUM == 503 || LUA_VERSION_NUM == 504
			/*	A B	R(A), ..., R(A+B) := nil		*/
			int rb = a + b;
			sprintf(line, "R%d %d", a, b);
#endif
			if (rb > a) {
				StringBuffer_printf(lend, "R%d to R%d := nil", a, rb);
			} else if (rb <= a) {
				StringBuffer_printf(lend, "R%d := nil", rb);
			}
			break;
		}
		case OP_VARARG:
			/*	A B	R(A), R(A+1), ..., R(A+B-2) = vararg		*/
			// lua-5.1 and ANoFrillsIntroToLua51VMInstructions.pdf are wrong
#if LUA_VERSION_NUM == 504
			int to_reg = c;
#else
			int to_reg = b;
#endif
			sprintf(line, "R%d %d", a, to_reg);
			if (to_reg > 2) {
				StringBuffer_printf(lend, "R%d to R%d := ...", a, a+to_reg-2);
			} else if (to_reg == 2) {
				StringBuffer_printf(lend, "R%d := ...", a);
			} else if (to_reg == 0) {
				StringBuffer_printf(lend, "R%d to top := ...", a);
			}
			break;
#if LUA_VERSION_NUM == 504
		case OP_VARARGPREP:
			sprintf(line, "%d", a);
			StringBuffer_printf(lend, "Adjust vararg parameters");
			break;
#endif
		case OP_GETUPVAL:
			/*	A B	R(A) := UpValue[B]				*/
			sprintf(line,"R%d U%d",a,b);
			StringBuffer_printf(lend,"R%d := U%d",a,b);
			break;
#if LUA_VERSION_NUM == 501
		case OP_GETGLOBAL:
			/*	A Bx	R(A) := Gbl[Kst(Bx)]				*/
			sprintf(line,"R%d K%d",a,bc);
			StringBuffer_printf(lend,"R%d := %s",a,GLOBAL(bc));
			break;
#endif
#if LUA_VERSION_NUM == 502 || LUA_VERSION_NUM == 503
		case OP_GETTABUP:
			/*	A B C	R(A) := UpValue[B][RK(C)]			*/
			sprintf(line,"R%d U%d %c%d",a,b,CC(c),CV(c));
			tmpconstant1 = RK(c);
			StringBuffer_printf(lend,"R%d := U%d[%s]",a,b,tmpconstant1);
			break;
#elif LUA_VERSION_NUM == 504
		case OP_GETTABUP:
			/*	A B C	R[A] := UpValue[B][K[C]:string]			*/
			sprintf(line, "R%d U%d K%d", a, b, c);
			tmpconstant1 = DecompileConstant(f, c);
			StringBuffer_printf(lend, "R%d := U%d[%s]", a, b, tmpconstant1);
			break;
#endif
#if LUA_VERSION_NUM == 504
		case OP_GETTABLE:
			/*	A B C	R(A) := R(B)[R(C)]				*/
			sprintf(line,"R%d R%d R%d",a,b,c);
			StringBuffer_printf(lend,"R%d := R%d[R%d]",a,b,c);
			break;
#else
		case OP_GETTABLE:
			/*	A B C	R(A) := R(B)[RK(C)]				*/
			sprintf(line,"R%d R%d %c%d",a,b,CC(c),CV(c));
			tmpconstant1 = RK(c);
			StringBuffer_printf(lend,"R%d := R%d[%s]",a,b,tmpconstant1);
			break;
#endif
#if LUA_VERSION_NUM == 501
		case OP_SETGLOBAL:
			/*	A Bx	Gbl[Kst(Bx)] := R(A)				*/
			sprintf(line,"R%d K%d",a,bc);
			StringBuffer_printf(lend,"%s := R%d",GLOBAL(bc),a);
			break;
#endif
#if LUA_VERSION_NUM == 502 || LUA_VERSION_NUM == 503
		case OP_SETTABUP:
			/*	A B C	UpValue[A][RK(B)] := RK(C)			*/
			sprintf(line,"U%d %c%d %c%d",a,CC(b),CV(b),CC(c),CV(c));
			tmpconstant1 = RK(b);
			tmpconstant2 = RK(c);
			StringBuffer_printf(lend,"U%d[%s] := %s",a,tmpconstant1,tmpconstant2);
			break;
#elif LUA_VERSION_NUM == 504
		case OP_SETTABUP:
			/*	A B C	UpValue[A][K[B]:string] := RK(C)		*/
			sprintf(line,"U%d K%d %c%d", a, b, CC(c), CV(c));
			tmpconstant1 = DecompileConstant(f, b);
			tmpconstant2 = RK(c);
			StringBuffer_printf(lend, "U%d[%s] := %s", a, tmpconstant1, tmpconstant2);
			break;
#endif
		case OP_SETUPVAL:
			/*	A B	UpValue[B] := R(A)				*/
			sprintf(line,"R%d U%d",a,b);
			StringBuffer_printf(lend,"U%d := R%d",b,a);
			break;
#if LUA_VERSION_NUM == 504
		case OP_SETTABLE:
			/*	A B C	R(A)[R(B)] := RK(C)				*/
			sprintf(line,"R%d R%d %c%d",a,b,CC(c),CV(c));
			tmpconstant1 = RK(c);
			StringBuffer_printf(lend,"R%d[R%d] := %s",a,b,tmpconstant1);
			break;
#else
		case OP_SETTABLE:
			/*	A B C	R(A)[RK(B)] := RK(C)				*/
			sprintf(line,"R%d %c%d %c%d",a,CC(b),CV(b),CC(c),CV(c));
			tmpconstant1 = RK(b);
			tmpconstant2 = RK(c);
			StringBuffer_printf(lend,"R%d[%s] := %s",a,tmpconstant1,tmpconstant2);
			break;
#endif
		case OP_NEWTABLE:
			/*	A B C	R(A) := {} (size = B,C)				*/
			sprintf(line,"R%d %d %d",a,b,c);
			int realc = c;
#if LUA_VERSION_NUM == 504
			if (k) { // Extra argument should be added
				realc += GETARG_Ax(f->code[pc+1]) * (MAXARG_C + 1);
				ignoreNext = 1;
			}
#endif
			StringBuffer_printf(lend,"R%d := {} (size = %d,%d)",a,b,realc);
			break;
		case OP_SELF:
			/*	A B C	R(A+1) := R(B); R(A) := R(B)[RK(C)]		*/
			sprintf(line,"R%d R%d %c%d",a,b,CC(c),CV(c));
			tmpconstant1 = RK(c);
			StringBuffer_printf(lend,"R%d := R%d; R%d := R%d[%s]",a+1,b,a,b,tmpconstant1);
			break;
		case OP_ADD:
			/*	A B C	R(A) := RK(B) + RK(C)				*/
			/* 5.4: 	R(A) := R(B) + R(C)                 */
		case OP_SUB:
			/*	A B C	R(A) := RK(B) - RK(C)				*/
			/* 5.4: 	R(A) := R(B) - R(C)                 */
		case OP_MUL:
			/*	A B C	R(A) := RK(B) * RK(C)				*/
			/* 5.4: 	R(A) := R(B) * R(C)                 */
		case OP_DIV:
			/*	A B C	R(A) := RK(B) / RK(C)				*/
			/* 5.4: 	R(A) := R(B) / R(C)                 */
		case OP_POW:
			/*	A B C	R(A) := RK(B) ^ RK(C)				*/
			/* 5.4: 	R(A) := R(B) ^ R(C)                 */
		case OP_MOD:
			/*	A B C	R(A) := RK(B) % RK(C)				*/
			/* 5.4: 	R(A) := R(B) % R(C)                 */
#if LUA_VERSION_NUM == 503 || LUA_VERSION_NUM == 504
		case OP_IDIV:
			/*	A B C	R(A) := RK(B) // RK(C)				*/
			/* 5.4: 	R(A) := R(B) // R(C)                */
		case OP_BAND:
			/*	A B C	R(A) := RK(B) & RK(C)				*/
			/* 5.4: 	R(A) := R(B) & R(C)                 */
		case OP_BOR:
			/*	A B C	R(A) := RK(B) | RK(C)				*/
			/* 5.4: 	R(A) := R(B) | R(C)                 */
		case OP_BXOR:
			/*	A B C	R(A) := RK(B) ~ RK(C)				*/
			/* 5.4: 	R(A) := R(B) ~ R(C)                 */
		case OP_SHL:
			/*	A B C	R(A) := RK(B) << RK(C)				*/
			/* 5.4: 	R(A) := R(B) << R(C)                */
		case OP_SHR:
			/*	A B C	R(A) := RK(B) >> RK(C)				*/
			/* 5.4: 	R(A) := R(B) >> R(C)                */
#endif
#if LUA_VERSION_NUM == 504
			sprintf(line,"R%d R%d R%d",a,b,c);
			StringBuffer_printf(lend,"R%d := R%d %s R%d",a,b,operators[o],c);
#else
			sprintf(line,"R%d %c%d %c%d",a,CC(b),CV(b),CC(c),CV(c));
			tmpconstant1 = RK(b);
			tmpconstant2 = RK(c);
			StringBuffer_printf(lend,"R%d := %s %s %s",a,tmpconstant1,operators[o],tmpconstant2);
#endif
			break;
#if LUA_VERSION_NUM == 504
		case OP_ADDI:
		/*	A B sC	R[A] := R[B] + sC				*/
		case OP_SHRI:
		/*	A B sC	R[A] := R[B] >> sC				*/
			sprintf(line, "R%d R%d %d", a, b, sc);
			StringBuffer_printf(lend, "R%d := R%d %s %d", a, b, operators[o], sc);
			break;
		case OP_SHLI:
		/*	A B sC	R[A] := sC << R[B]				*/
			sprintf(line, "R%d R%d %d", a, b, sc);
			StringBuffer_printf(lend, "R%d := %d %s R%d", a, sc, operators[o], b);
			break;
		case OP_ADDK:
		/*	A B C	R[A] := R[B] + K[C]				*/
		case OP_SUBK:
		/*	A B C	R[A] := R[B] - K[C]				*/
		case OP_MULK:
		/*	A B C	R[A] := R[B] * K[C]				*/
		case OP_MODK:
		/*	A B C	R[A] := R[B] % K[C]				*/
		case OP_POWK:
		/*	A B C	R[A] := R[B] ^ K[C]				*/
		case OP_DIVK:
		/*	A B C	R[A] := R[B] / K[C]				*/
		case OP_IDIVK:
		/*	A B C	R[A] := R[B] // K[C]				*/
		case OP_BANDK:
		/*	A B C	R[A] := R[B] & K[C]:integer			*/
		case OP_BORK:
		/*	A B C	R[A] := R[B] | K[C]:integer			*/
		case OP_BXORK:
		/*	A B C	R[A] := R[B] ~ K[C]:integer			*/
			sprintf(line,"R%d R%d K%d",a,b,c);
			tmpconstant1 = DecompileConstant(f, c);
			StringBuffer_printf(lend,"R%d := R%d %s %s",a,b,operators[o],tmpconstant1);
			break;
#endif
		case OP_UNM:
			/*	A B	R(A) := -R(B)					*/
		case OP_NOT:
			/*	A B	R(A) := not R(B)				*/
		case OP_LEN:
			/*	A B	R(A) := length of R(B)				*/
#if LUA_VERSION_NUM == 503
		case OP_BNOT:
			/*	A B	R(A) := ~R(B)					*/
#endif
			sprintf(line,"R%d R%d",a,b);
			StringBuffer_printf(lend,"R%d := %sR%d",a,operators[o],b);
			break;
		case OP_CONCAT:
			/*	A B C	R(A) := R(B).. ... ..R(C)			*/
			sprintf(line,"R%d R%d R%d",a,b,c);
			StringBuffer_printf(lend,"R%d := concat(R%d to R%d)",a,b,c);
			break;
		case OP_JMP:
#if LUA_VERSION_NUM == 504
			/*  sJ pc+=sJ                   */
			dest = pc + sj + 1;
			sprintf(line, "%d", sj);
			StringBuffer_printf(lend, "PC += %d (goto %d)", sj, dest);
#else
			/*	sBx	pc+=sBx					*/
			dest = pc + sbc + 1;
			sprintf(line, "%d", sbc);
			StringBuffer_printf(lend, "PC += %d (goto %d)", sbc, dest);
#endif
#if LUA_VERSION_NUM == 502 || LUA_VERSION_NUM == 503
			// instead OP_CLOSE in 5.2 : if (A) close all upvalues >= R(A-1)
			// lua-5.2/src/lopcodes.h line 199 is wrong. See lua-5.2/src/lvm.c line 504:
			// if (a > 0) luaF_close(L, ci->u.l.base + a - 1);
			sprintf(line, "R%d %d", a, sbc);
			if (a>0) {
				StringBuffer_addPrintf(lend,"; close all upvalues in R%d to top",a-1);
			}
#endif
			break;
#if LUA_VERSION_NUM == 504
		case OP_TBC:
			sprintf(line, "%d", a);
			StringBuffer_printf("Mark var %d to be closed", a);
			break;

#endif
		case OP_EQ:
			/*	A B C	if ((RK(B) == RK(C)) ~= A) then pc++		*/
			/*	5.4: A B k	if ((R(A) == R(B)) ~= k) then pc++		*/
			
		case OP_LT:
			/*	A B C	if ((RK(B) <  RK(C)) ~= A) then pc++  		*/
			/*	5.4: A B k	if ((R(A) <  R(B)) ~= k) then pc++		*/
		case OP_LE:
			/*	A B C	if ((RK(B) <= RK(C)) ~= A) then pc++  		*/
			/*	5.4: A B k	if ((R(A) <= R(B)) ~= k) then pc++		*/
#if LUA_VERSION_NUM == 504
			dest = GETARG_sJ(f->code[pc+1]) + pc + 2;
			sprintf(line,"R%d R%d %d",a,b,k);
			StringBuffer_printf(lend,"if R%d %s R%d then goto %d else goto %d",a,(k?invopstr(o):opstr(o)),b,pc+2,dest);
#else
			dest = GETARG_sBx(f->code[pc+1]) + pc + 2;
			sprintf(line,"%d %c%d %c%d",a,CC(b),CV(b),CC(c),CV(c));
			tmpconstant1 = RK(b);
			tmpconstant2 = RK(c);
			StringBuffer_printf(lend,"if %s %s %s then goto %d else goto %d",tmpconstant1,(a?invopstr(o):opstr(o)),tmpconstant2,pc+2,dest);
#endif
			break;
#if LUA_VERSION_NUM == 504
		case OP_EQK:
			/*	A B k	if ((R(A) == K(B)) ~= k) then pc++		*/
			dest = GETARG_sJ(f->code[pc+1]) + pc + 2;
			sprintf(line,"R%d K%d %d",a,b,k);
			tmpconstant1 = DecompileConstant(f, b);
			StringBuffer_printf(lend,"if R%d %s %s then goto %d else goto %d",a,(k?"~=":"=="),tmpconstant1,pc+2,dest);
			break;
		case OP_EQI:
			/*	A sB k	if ((R[A] == sB) ~= k) then pc++		*/
		case OP_LTI:
			/*	A sB k	if ((R[A] < sB) ~= k) then pc++			*/
		case OP_LEI:
			/*	A sB k	if ((R[A] <= sB) ~= k) then pc++		*/
		case OP_GTI:
			/*	A sB k	if ((R[A] > sB) ~= k) then pc++			*/
		case OP_GEI:
			/*	A sB k	if ((R[A] >= sB) ~= k) then pc++		*/
            dest = GETARG_sJ(f->code[pc+1]) + pc + 2;
			sprintf(line, "R%d %d %d", a, sb, k);
			StringBuffer_printf(lend,"if R%d %s %s then goto %d else goto %d",a,get_comp_op_i(k, o),sb,pc+2,dest);
			break;
#endif
		case OP_TEST:
#if LUA_VERSION_NUM == 504
			/*	A k	if (not R[A] == k) then pc++			*/
			dest = GETARG_sJ(f->code[pc+1]) + pc + 2;
			sprintf(line,"R%d %d",a,k);
			StringBuffer_printf(lend,"if %sR%d then goto %d else goto %d",(k?"not ":""),a,pc+2,dest);
#else
			/*	A C	if not (R(A) <=> C) then pc++			*/
			dest = GETARG_sBx(f->code[pc+1]) + pc + 2;
			sprintf(line,"R%d %d",a,c);
			StringBuffer_printf(lend,"if %sR%d then goto %d else goto %d",(c?"not ":""),a,pc+2,dest);
#endif
			break;
		case OP_TESTSET:
#if LUA_VERSION_NUM == 504
			/*	A B k	if (not R[B] == k) then pc++ else R[A] := R[B]	*/
			dest = GETARG_sJ(f->code[pc+1]) + pc + 2;
			sprintf(line,"R%d R%d %d",a,b,k);
			StringBuffer_printf(lend,"if %sR%d then R%d := R%d ; goto %d else goto %d",(k?"":"not "),b,a,b,dest,pc+2);

#else
			/*	A B C	if (R(B) <=> C) then R(A) := R(B) else pc++	*/ 
			dest = GETARG_sBx(f->code[pc+1]) + pc + 2;
			sprintf(line,"R%d R%d %d",a,b,c);
			StringBuffer_printf(lend,"if %sR%d then R%d := R%d ; goto %d else goto %d",(c?"":"not "),b,a,b,dest,pc+2);
#endif
			break;
		case OP_CALL:
			/*	A B C	R(A), ... ,R(A+C-2) := R(A)(R(A+1), ... ,R(A+B-1)) */
#if LUA_VERSION_NUM != 504
		case OP_TAILCALL:
			/*	A B C	return R(A)(R(A+1), ... ,R(A+B-1))		*/
#endif
			sprintf(line,"R%d %d %d",a,b,c);
			if (b>2) {
				sprintf(tmp,"R%d to R%d", a+1, a+b-1);
			} else if (b==2) {
				sprintf(tmp,"R%d", a+1);
			} else if (b==1) {
				sprintf(tmp,"");
			} else if (b==0) {
				sprintf(tmp,"R%d to top",a+1);
			}

			if (c>2) {
				sprintf(tmp2, "R%d to R%d", a, a+c-2);
			} else if (c==2) {
				sprintf(tmp2,"R%d",a);
			} else if (c==1) {
				sprintf(tmp2,"");
			} else if (c==0) {
				sprintf(tmp2,"R%d to top",a);
			}
			StringBuffer_printf(lend,"%s := R%d(%s)",tmp2,a,tmp);
			break;
#if LUA_VERSION_NUM == 504
		case OP_TAILCALL:
			/*	A B C k	return R[A](R[A+1], ... ,R[A+B-1])		*/
			// C is the number of fixed params of this function (for cleanup purposes)
			// k is whether the function has upvalues, both are not relevant
			sprintf(line,"R%d %d %d %d",a,b,c,k);
			if (b>2) {
				sprintf(tmp,"R%d to R%d", a+1, a+b-1);
			} else if (b==2) {
				sprintf(tmp,"R%d", a+1);
			} else if (b==1) {
				sprintf(tmp,"");
			} else if (b==0) {
				sprintf(tmp,"R%d to top",a+1);
			}
			StringBuffer_printf(lend,"return R%d(%s)",a,tmp);
			break;
#endif
		case OP_RETURN:
			/*	A B	return R(A), ... ,R(A+B-2)	(see note)	*/
			sprintf(line,"R%d %d",a,b);
			if (b > 2) {
				sprintf(tmp, "R%d to R%d", a, a+b-2);
			} else if (b == 2) {
				sprintf(tmp, "R%d", a);
			} else if (b == 1) {
				sprintf(tmp, "");
			} else if (b == 0) {
				sprintf(tmp, "R%d to top", a);
			}

			StringBuffer_printf(lend,"return %s",tmp);
			break;
#if LUA_VERSION_NUM == 504
		case OP_RETURN0:
			sprintf(line, "");
			StringBuffer_printf(lend, "return");
			break;
		case OP_RETURN1:
			sprintf(line, "R%d", a);
			StringBuffer_printf(lend, "return R%d", a);
			break;

#endif
		case OP_FORLOOP:
			/*	A sBx	R(A)+=R(A+2);
				if R(A) <?= R(A+1) then { pc+=sBx; R(A+3)=R(A) }*/
			/* Since 5.4, sBx was replaced by Bx */
#if LUA_VERSION_NUM == 504
			int jump_len = bx;
#else
			int jump_len = sbc;
#endif
			dest = pc + jump_len + 1;
			sprintf(line, "R%d %d", a, jump_len);
			StringBuffer_printf(lend, "R%d += R%d; if R%d <= R%d then R%d := R%d; PC += %d , goto %d end", a, a+2, a, a+1, a+3, a, jump_len, dest);
			break;
		case OP_FORPREP:
			/*	A sBx	R(A)-=R(A+2); pc+=sBx				*/
			/* Since 5.4, sBx was replaced by Bx */
#if LUA_VERSION_NUM == 504
			int jump_len = bx;
#else
			int jump_len = sbc;
#endif
			dest = pc + jump_len + 1;
			sprintf(line,"R%d %d",a,jump_len);
			StringBuffer_printf(lend,"R%d -= R%d; pc += %d (goto %d)",a,a+2,jump_len,dest);
			break;
#if LUA_VERSION_NUM == 504
		case OP_TFORPREP:
			/*	A Bx	create upvalue for R[A + 3]; pc+=Bx		*/
			sprintf(line,"R%d %d",a,bx);
			StringBuffer_printf(lend,"Create Upvalue for R%d, pc += %d (goto %d)",a+3,bx,pc+bx+1);
			break;

#endif
		case LUADEC_TFORLOOP:
			/*	A C	R(A+3), ... ,R(A+2+C) := R(A)(R(A+1), R(A+2));	*/
			/*	Since 5.4, the return registers start from A+4, as A+3 holds the to-be-closed variable */
#if LUA_VERSION_NUM == 504
			int from_reg = a+4;
			int to_reg = a+c+3;
#else
			int from_reg = a+3;
			int to_reg = a+c+2;
#endif
			sprintf(line,"R%d %d",a,c);
			if (c == 1) {
				sprintf(tmp2, "R%d", from_reg);
			}else if (c>1) {
				sprintf(tmp2, "R%d to R%d", from_reg, to_reg);
			} else {
				sprintf(tmp2,"ERROR c=0");
			}
			StringBuffer_printf(lend, "%s := R%d(R%d,R%d)", tmp2, a, a+1, a+2);
#if LUA_VERSION_NUM == 501
			// only in 5.1 : if R(A+3) ~= nil then R(A+2)=R(A+3) else pc++
			StringBuffer_addPrintf(lend, "; if R%d ~= nil then R%d := R%d else goto %d", a+3, a+2, a+3, pc+2);
#endif
			break;
#if LUA_VERSION_NUM == 502 || LUA_VERSION_NUM == 503 || LUA_VERSION_NUM == 504
		case OP_TFORLOOP:
			/*	A sBx	if R(A+1) ~= nil then { R(A)=R(A+1); pc += sBx }*/
			/*	5.4 the return registers start from A+4, as A+3 holds the to-be-closed variable */
#if LUA_VERSION_NUM == 504
			int jump_len = bx;
			int next_val_reg = a+2;
#else
			int jump_len = sbc;
			int next_val_reg = a+1;
#endif
			dest = pc + jump_len + 1;
			sprintf(line,"R%d %d",a,jump_len);
			StringBuffer_printf(lend,"if R%d ~= nil then { R%d := R%d ; pc += %d (goto %d) }",next_val_reg,a, next_val_reg, jump_len, dest);
			break;
#endif
		case OP_SETLIST:
		{
#if LUA_VERSION_NUM == 504
			/*	A B C k	R[A][C+i] := R[A+i], 1 <= i <= B		*/
			sprintf(line, "R%d %d %d %d", a, b, c, k);
			if (b==0) {
				sprintf(tmp, "1 <= i <= top");
			} else {
				sprintf(tmp, "1 <= i <= %d", b);
			}
			int realc = c;
			if (k) { // Extra argument should be added
				realc += GETARG_Ax(f->code[pc+1]) * (MAXARG_C + 1);
				ignoreNext = 1;
			}
			sprintf(lend, "R%d[%d+i] := R[%d+i], %s", a, realc, a, tmp);
#else
			/*	A B C	R(A)[(C-1)*FPF+i] := R(A+i), 1 <= i <= B	*/
			int next_is_extraarg = 1;
			unsigned int realc = c, startindex;
			if (c == 0) {
				Instruction i_next_arg = f->code[pc + 1];
#if LUA_VERSION_NUM == 501
				realc = i_next_arg;
				ignoreNext = 1;
#endif
#if LUA_VERSION_NUM == 502 || LUA_VERSION_NUM == 503 || LUA_VERSION_NUM == 504
				if (GET_OPCODE(i_next_arg) == OP_EXTRAARG) {
					realc = GETARG_Ax(i_next_arg);
				} else {
					next_is_extraarg = 0;
				}
#endif
			}
			startindex = (realc - 1)*LFIELDS_PER_FLUSH;
			sprintf(line, "R%d %d %d", a, b, c);
			if (b == 0) {
				StringBuffer_printf(lend, "R%d[%d] to R%d[top] := R%d to top", a, startindex, a, a + 1);
			} else if (b == 1) {
				StringBuffer_printf(lend, "R%d[%d] := R%d", a, startindex, a + 1);
			} else if (b > 1) {
				StringBuffer_printf(lend, "R%d[%d] to R%d[%d] := R%d to R%d", a, startindex, a, startindex + b - 1, a + 1, a + b);
			}
			if (c != 0) {
				StringBuffer_addPrintf(lend, " ; R(a)[(c-1)*FPF+i] := R(a+i), 1 <= i <= b, a=%d, b=%d, c=%d, FPF=%d", a, b, c, LFIELDS_PER_FLUSH);
			} else {
				StringBuffer_addPrintf(lend, " ; R(a)[(realc-1)*FPF+i] := R(a+i), 1 <= i <= b, a=%d, b=%d, c=%d, realc=%u, FPF=%d", a, b, c, realc, LFIELDS_PER_FLUSH);
#if LUA_VERSION_NUM == 502 || LUA_VERSION_NUM == 503
				if (!next_is_extraarg) {
					StringBuffer_add(lend, " ; Error: SETLIST with c==0, but not followed by EXTRAARG.");
				}
#endif
				if (realc == 0) {
					StringBuffer_add(lend, " ; Error: SETLIST with c==0, but realc==0.");
				}
			}
#endif // __if LUA_VERSION_NUM == 504
			break;
		}
#if LUA_VERSION_NUM == 501 || LUA_VERSION_NUM == 504
		case OP_CLOSE:
			/*	A 	close all variables in the stack up to (>=) R(A)*/
			sprintf(line,"R%d",a);
			StringBuffer_printf(lend,"close all upvalues in R%d to top",a);
			break;
#endif
		case OP_CLOSURE:
			/*	A Bx	R(A) := closure(KPROTO[Bx])		*/
			sprintf(line,"R%d %d",a,bc);
			if (name_len>0) {
				StringBuffer_printf(lend, "R%d := closure(Function #%s_%d)", a, name, bc);
			} else {
				StringBuffer_printf(lend, "R%d := closure(Function #%d)", a, bc);
			}
			break;
#if LUA_VERSION_NUM == 504
		case OP_MMBIN:
			sprintf(line, "R%d R%d %d", a, b, c);
			StringBuffer_printf(lend, "%s(R%d, R%d)", get_mm_func_name_from_tm(c), a, b);
			break;
		case OP_MMBINI:
			sprintf(line, "R%d %d %d %d", a, sb, c, k);
			if (k) {
				StringBuffer_printf(lend, "%s(%d, R%d)", get_mm_func_name_from_tm(c), sb, a);
			} else {
				StringBuffer_printf(lend, "%s(R%d, %d)", get_mm_func_name_from_tm(c), a, sb);
			}
			break;
		case OP_MMBINK:
			sprintf(line, "R%d K%d %d %d", a, b, c, k);
			tmpconstant1 = DecompileConstant(f, b);
			if (k) {
				StringBuffer_printf(lend, "%s(%s, R%d)", get_mm_func_name_from_tm(c), tmpconstant1, a);
			} else {
				StringBuffer_printf(lend, "%s(R%d, %s)", get_mm_func_name_from_tm(c), a, tmpconstant1);
			}
			break;
#endif
		default:
			break;
		}
		printf("%5d [-]: %-9s %-13s; %s\n",pc,luaP_opnames[o],line,StringBuffer_getRef(lend));
		if (tmpconstant1) {
			free(tmpconstant1);
			tmpconstant1 = NULL;
		}
		if (tmpconstant2) {
			free(tmpconstant2);
			tmpconstant2 = NULL;
		}
	}
	StringBuffer_delete(lend);
	lend = NULL;
	printf("\n\n");
	if (process_sub && f->sizep != 0) {
		char* subname = (char*)calloc(name_len + 10, sizeof(char));
		for (pc=0; pc < f->sizep; pc++) {
			if (name_len > 0) {
				sprintf(subname, "%s_%d", name, pc);
			} else {
				sprintf(subname, "%d", pc);
			}
			luadec_disassemble(f->p[pc], dflag, subname);
		}
		free(subname);
	}
}
