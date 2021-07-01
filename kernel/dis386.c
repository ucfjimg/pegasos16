#include <stdio.h>
#include <string.h>

#include "dis386.h"
#include "panic.h"

#include <string.h>

extern int sprintf(char *dst, char __far *fmt, ...);

#define SEG_PFX (SEGCS | SEGDS | SEGES | SEGFS | SEGDS | SEGSS)
#define REP_PFX (REP | REPNE) 

#define _A 0
#define _C 1
#define _D 2
#define _E 3
#define _F 4
#define _G 5
#define _I 6
#define _J 7
#define _M 8
#define _O 9
#define _R 10
#define _S 11
#define _T 12
#define _X 13
#define _Y 14
#define _REG 15
#define _REGB 16
#define _REGS 17
#define _CONST 18
#define _NONE 31

#define _a 0
#define _b 1
#define _c 2
#define _d 3
#define _p 4
#define _s 5
#define _v 6
#define _w 7

/* _REG */
#define _AX 0
#define _CX 1
#define _DX 2
#define _BX 3
#define _SP 4
#define _BP 5
#define _SI 6
#define _DI 7

/* _REGS */
#define _ES 0
#define _CS 1
#define _SS 2
#define _DS 3
#define _FS 4
#define _GS 5

/* _REGB */
#define _AL 0
#define _CL 1
#define _DL 2
#define _BL 3
#define _AH 0
#define _CH 1
#define _DH 2
#define _BH 3

#define OP(x,y) (((x) << 4) | (y))

#define MODRM_MOD(x) (((x) >> 6) & 3)
#define MODRM_REG(x) (((x) >> 3) & 7)
#define MODRM_RM(x)  ((x) & 7)

#define SIB_SS(x)    (((x) >> 6) & 3)
#define SIB_INDEX(x) (((x) >> 3) & 7)
#define SIB_BASE(x)  ((x) & 7)

typedef struct opcode_t opcode_t;
struct opcode_t {
    const char *mne;
    unsigned d_add : 5;
    unsigned d_opreg : 4;
    unsigned s_add : 5;
    unsigned s_opreg : 4;
    unsigned op3_add : 5;
    unsigned op3_opreg : 4;
    unsigned modop : 1;                 /* op stored in modrm reg bits */
};

typedef struct addop_t addop_t;
struct addop_t {
    unsigned add;
    unsigned opreg;
};

enum segs_t {
    SEG_NONE,
    SEG_CS,
    SEG_DS,
    SEG_ES,
    SEG_SS,
    SEG_FS,
    SEG_GS,
};

enum rep_t {
    REP_NONE,
    REP_REP,
    REP_REPNE
};

typedef struct distate_t distate_t;
struct distate_t {
    uint8_t __far *code;
    char *args;
    uint8_t modrm;
    uint8_t sib;
    int addr32;
    int data32;
    int lock;
    enum segs_t seg;
    enum rep_t rep;
};

#define INVALID_OP { NULL, 0, 0, 0, 0, 0, 0, 0 }
#define IMP(m) { m, 0, 0, 0, 0, 0, 0, 0 }
#define ONE_OP(m, dad, dop) { m, dad, dop, 0, 0, 0, 0, 0 }
#define TWO_OP(m, dad, dop, sad, sop) { m, dad, dop, sad, sop, 0, 0, 0 }
#define TRE_OP(m, dad, dop, sad, sop, tad, top) { m, dad, dop, sad, sop, tad, top, 0 }
#define J_OP(m) { m, _J, _b, 0, 0, 0, 0, 0 }
#define JV_OP(m) { m, _J, _v, 0, 0, 0, 0, 0 }
#define M_ONE_OP(m, dad, dop) { (const char *)m, dad, dop, 0, 0, 0, 0, 1 }
#define M_TWO_OP(m, dad, dop, sad, sop) { (const char *)m, dad, dop, sad, sop, 0, 0, 1 }


static char *group1[8] = {
    "ADD",
    "OR",
    "ADC",
    "SBB",
    "AND",
    "SUB",
    "XOR",
    "CMP"
};

static char *group2[8] = {
    "ROL",
    "ROR",
    "RCL",
    "RCR",
    "SHL",
    "SHR",
    "SHL",
    "SAR"
};

static char *group3[8] = {
    "TEST",
    "TEST",
    "NOT",
    "NEG",
    "MUL",
    "IMUL",
    "DIV",
    "IDIV"
};

static char *group4[8] = {
    "INC",
    "DEC",
    "???",
    "???",
    "???",
    "???",
    "???",
    "???"
};

static char *group5[8] = {
    "INC",
    "DEC",
    "CALL",
    "CALL",
    "JMP",
    "JMP",
    "PUSH",
    "???"
};

static char *group6[8] = {
    "SLDT",
    "STR",
    "LLDT",
    "LTR",
    "VERR",
    "VERW",
    "???",
    "???"
};

static char *group7[8] = {
    "SGDT",
    "SIDT",
    "LGDT",
    "LIDT",
    "SMSW",
    "???",
    "LMSW",
    "???"
};

static char *group8[8] = {
    "???",
    "???",
    "???",
    "???",
    "BT",
    "BTS",
    "BTS",
    "BTC"
};

static opcode_t opcodes[] = 
{
    /* 00 */ TWO_OP("ADD",   _E, _b, _G, _b),
    /* 01 */ TWO_OP("ADD",   _E, _v, _G, _v),
    /* 02 */ TWO_OP("ADD",   _G, _b, _E, _b),
    /* 03 */ TWO_OP("ADD",   _G, _v, _E, _v),
    /* 04 */ TWO_OP("ADD",   _REGB, _AL, _I, _b),
    /* 05 */ TWO_OP("ADD",   _REG,  _AX, _I, _v),
    /* 06 */ ONE_OP("PUSH",  _REGS, _ES),
    /* 07 */ ONE_OP("POP",   _REGS, _ES),
    /* 08 */ TWO_OP("OR",    _E, _b, _G, _b),
    /* 09 */ TWO_OP("OR",    _E, _v, _G, _v),
    /* 0a */ TWO_OP("OR",    _G, _b, _E, _b),
    /* 0b */ TWO_OP("OR",    _G, _v, _E, _v),
    /* 0c */ TWO_OP("OR",    _REGB, _AL, _I, _b),
    /* 0d */ TWO_OP("OR",    _REG,  _AX, _I, _v),
    /* 0e */ ONE_OP("PUSH",  _REGS, _CS),
    /* 0f */ ONE_OP("POP",   _REGS, _CS),

    /* 10 */ TWO_OP("ADC",   _E, _b, _G, _b),
    /* 11 */ TWO_OP("ADC",   _E, _v, _G, _v),
    /* 12 */ TWO_OP("ADC",   _G, _b, _E, _b),
    /* 13 */ TWO_OP("ADC",   _G, _v, _E, _v),
    /* 14 */ TWO_OP("ADC",   _REGB, _AL, _I, _b),
    /* 15 */ TWO_OP("ADC",   _REG,  _AX, _I, _v),
    /* 16 */ ONE_OP("PUSH",  _REGS, _SS),
    /* 17 */ ONE_OP("POP",   _REGS, _SS),
    /* 18 */ TWO_OP("SBB",   _E, _b, _G, _b),
    /* 19 */ TWO_OP("SBB",   _E, _v, _G, _v),
    /* 1a */ TWO_OP("SBB",   _G, _b, _E, _b),
    /* 1b */ TWO_OP("SBB",   _G, _v, _E, _v),
    /* 1c */ TWO_OP("SBB",   _REGB, _AL, _I, _b),
    /* 1d */ TWO_OP("SBB",   _REG,  _AX, _I, _v),
    /* 1e */ ONE_OP("PUSH",  _REGS, _DS),
    /* 1f */ ONE_OP("POP",   _REGS, _DS),

    /* 20 */ TWO_OP("AND",   _E, _b, _G, _b),
    /* 21 */ TWO_OP("AND",   _E, _v, _G, _v),
    /* 22 */ TWO_OP("AND",   _G, _b, _E, _b),
    /* 23 */ TWO_OP("AND",   _G, _v, _E, _v),
    /* 24 */ TWO_OP("AND",   _REGB, _AL, _I, _b),
    /* 25 */ TWO_OP("AND",   _REG,  _AX, _I, _v),
    /* 26 */ IMP("ES:"),
    /* 27 */ IMP("DAA"),
    /* 28 */ TWO_OP("SUB",   _E, _b, _G, _b),
    /* 29 */ TWO_OP("SUB",   _E, _v, _G, _v),
    /* 2a */ TWO_OP("SUB",   _G, _b, _E, _b),
    /* 2b */ TWO_OP("SUB",   _G, _v, _E, _v),
    /* 2c */ TWO_OP("SUB",   _REGB, _AL, _I, _b),
    /* 2d */ TWO_OP("SUB",   _REG,  _AX, _I, _v),
    /* 2e */ IMP("CS:"),
    /* 2f */ IMP("DAS"),

    /* 30 */ TWO_OP("XOR",   _E, _b, _G, _b),
    /* 31 */ TWO_OP("XOR",   _E, _v, _G, _v),
    /* 32 */ TWO_OP("XOR",   _G, _b, _E, _b),
    /* 33 */ TWO_OP("XOR",   _G, _v, _E, _v),
    /* 34 */ TWO_OP("XOR",   _REGB, _AL, _I, _b),
    /* 35 */ TWO_OP("XOR",   _REG,  _AX, _I, _v),
    /* 36 */ IMP("SS:"),
    /* 37 */ IMP("AAA"),
    /* 38 */ TWO_OP("CMP",   _E, _b, _G, _b),
    /* 39 */ TWO_OP("CMP",   _E, _v, _G, _v),
    /* 3a */ TWO_OP("CMP",   _G, _b, _E, _b),
    /* 3b */ TWO_OP("CMP",   _G, _v, _E, _v),
    /* 3c */ TWO_OP("CMP",   _REGB, _AL, _I, _b),
    /* 3d */ TWO_OP("CMP",   _REG,  _AX, _I, _v),
    /* 3e */ IMP("DS:"),
    /* 3f */ IMP("AAS"),

    /* 40 */ ONE_OP("INC", _REG, _AX),
    /* 41 */ ONE_OP("INC", _REG, _CX),
    /* 42 */ ONE_OP("INC", _REG, _DX),
    /* 43 */ ONE_OP("INC", _REG, _BX),
    /* 44 */ ONE_OP("INC", _REG, _SP),
    /* 45 */ ONE_OP("INC", _REG, _BP),
    /* 46 */ ONE_OP("INC", _REG, _SI),
    /* 47 */ ONE_OP("INC", _REG, _DI),
    /* 48 */ ONE_OP("DEC", _REG, _AX),
    /* 49 */ ONE_OP("DEC", _REG, _CX),
    /* 4a */ ONE_OP("DEC", _REG, _DX),
    /* 4b */ ONE_OP("DEC", _REG, _BX),
    /* 4c */ ONE_OP("DEC", _REG, _SP),
    /* 4d */ ONE_OP("DEC", _REG, _BP),
    /* 4e */ ONE_OP("DEC", _REG, _SI),
    /* 4f */ ONE_OP("DEC", _REG, _DI),

    /* 50 */ ONE_OP("PUSH", _REG, _AX),
    /* 51 */ ONE_OP("PUSH", _REG, _CX),
    /* 52 */ ONE_OP("PUSH", _REG, _DX),
    /* 53 */ ONE_OP("PUSH", _REG, _BX),
    /* 54 */ ONE_OP("PUSH", _REG, _SP),
    /* 55 */ ONE_OP("PUSH", _REG, _BP),
    /* 56 */ ONE_OP("PUSH", _REG, _SI),
    /* 57 */ ONE_OP("PUSH", _REG, _DI),
    /* 58 */ ONE_OP("POP",  _REG, _AX),
    /* 59 */ ONE_OP("POP",  _REG, _CX),
    /* 5a */ ONE_OP("POP",  _REG, _DX),
    /* 5b */ ONE_OP("POP",  _REG, _BX),
    /* 5c */ ONE_OP("POP",  _REG, _SP),
    /* 5d */ ONE_OP("POP",  _REG, _BP),
    /* 5e */ ONE_OP("POP",  _REG, _SI),
    /* 5f */ ONE_OP("POP",  _REG, _DI),

    /* 60 */ IMP("PUSHA"),
    /* 61 */ IMP("POPA"),
    /* 62 */ TWO_OP("BOUND", _G, _v, _M, _a),
    /* 63 */ TWO_OP("ARPL",  _E, _w, _R, _w),
    /* 64 */ IMP("FS:"),
    /* 65 */ IMP("GS:"),
    /* 66 */ IMP("OPSIZE"),
    /* 67 */ IMP("ADSIZE"),
    /* 68 */ ONE_OP("PUSH", _I, _v),
    /* 69 */ TRE_OP("IMIL", _G, _v, _E, _v, _I, _v),
    /* 6a */ ONE_OP("PUSH", _I, _b),
    /* 6b */ TRE_OP("IMIL", _G, _v, _E, _v, _I, _b),
    /* 6c */ TWO_OP("INSB", _Y, _b, _REG, _DX),
    /* 6d */ TWO_OP("INSW", _Y, _v, _REG, _DX),
    /* 6e */ TWO_OP("OUTSB",_REG, _DX, _X, _b),
    /* 6f */ TWO_OP("OUTSW",_REG, _DX, _X, _v),

    /* 70 */ J_OP("JO"),
    /* 71 */ J_OP("JNO"),
    /* 72 */ J_OP("JB"),
    /* 73 */ J_OP("JNB"),
    /* 74 */ J_OP("JZ"),
    /* 75 */ J_OP("JNZ"),
    /* 76 */ J_OP("JBE"),
    /* 77 */ J_OP("JNBE"),
    /* 78 */ J_OP("JS"),
    /* 79 */ J_OP("JNS"),
    /* 7a */ J_OP("JP"),
    /* 7b */ J_OP("JNP"),
    /* 7c */ J_OP("JL"),
    /* 7d */ J_OP("JNL"),
    /* 7e */ J_OP("JLE"),
    /* 7f */ J_OP("JNLE"),

    /* 80, 81, and 83 are group 1 - operation determined by mod r/m */
    /* 80 */ M_TWO_OP(group1, _E, _b, _I, _b),
    /* 81 */ M_TWO_OP(group1, _E, _v, _I, _v),
    /* 82 */ TWO_OP("MOV", _REGB, _AL, _I, _b),
    /* 83 */ M_TWO_OP(group1, _E, _v, _I, _b),
    /* 84 */ TWO_OP("TEST", _E, _b, _G, _b),
    /* 85 */ TWO_OP("TEST", _E, _v, _G, _v),
    /* 86 */ TWO_OP("XCHG", _E, _b, _G, _b),
    /* 87 */ TWO_OP("XCHG", _E, _v, _G, _v),
    /* 88 */ TWO_OP("MOV", _E, _b, _G, _b),
    /* 89 */ TWO_OP("MOV", _E, _v, _G, _v),
    /* 8a */ TWO_OP("MOV", _G, _b, _E, _b),
    /* 8b */ TWO_OP("MOV", _G, _v, _E, _v),
    /* 8c */ TWO_OP("MOV", _E, _w, _S, _w),
    /* 8d */ TWO_OP("LEA", _G, _v, _M, 0),
    /* 8e */ TWO_OP("MOV", _S, _w, _E, _w),
    /* 8f */ ONE_OP("POP", _E, _v),

    /* 90 */ IMP("NOP"),
    /* 91 */ TWO_OP("XCHG", _REG, _AX, _REG, _CX),    
    /* 92 */ TWO_OP("XCHG", _REG, _AX, _REG, _DX),    
    /* 93 */ TWO_OP("XCHG", _REG, _AX, _REG, _BX),    
    /* 94 */ TWO_OP("XCHG", _REG, _AX, _REG, _SP),    
    /* 95 */ TWO_OP("XCHG", _REG, _AX, _REG, _BP),    
    /* 96 */ TWO_OP("XCHG", _REG, _AX, _REG, _SI),    
    /* 97 */ TWO_OP("XCHG", _REG, _AX, _REG, _DI),    
    /* 98 */ IMP("CBW"),
    /* 99 */ IMP("CWD"),
    /* 9a */ ONE_OP("CALL", _A, _p),
    /* 9b */ IMP("WAIT"),
    /* 9c */ IMP("PUSHF"),
    /* 9d */ IMP("POPF"),
    /* 9e */ IMP("SAHF"),
    /* 9f */ IMP("LAHF"),

    /* a0 */ TWO_OP("MOV", _REGB, _AL, _O, _b),
    /* a1 */ TWO_OP("MOV", _REG,  _AX, _O, _v),
    /* a2 */ TWO_OP("MOV", _O, _b, _REGB, _AL),
    /* a3 */ TWO_OP("MOV", _O, _v, _REG , _AX),
    /* a4 */ TWO_OP("MOVS", _Y, _b, _X, _b),
    /* a5 */ TWO_OP("MOVS", _Y, _v, _X, _v),
    /* a6 */ TWO_OP("CMPS", _X, _b, _Y, _b),
    /* a7 */ TWO_OP("CMPS", _X, _v, _Y, _v),
    /* a8 */ TWO_OP("TEST", _REGB, _AL, _I, _b),
    /* a9 */ TWO_OP("TEST", _REG , _AX, _I, _v),
    /* aa */ TWO_OP("STOS", _Y, _b, _REGB, _AL),
    /* ab */ TWO_OP("STOS", _Y, _v, _REG,  _AX),
    /* ac */ TWO_OP("LODS", _REGB, _AL, _X, _b),
    /* ad */ TWO_OP("LODS", _REG,  _AX, _X, _v),
    /* ae */ TWO_OP("SCAS", _REGB, _AL, _X, _b),
    /* af */ TWO_OP("SCAS", _REG,  _AX, _X, _v),

    /* b0 */ TWO_OP("MOV", _REGB, _AL, _I, _b),
    /* b1 */ TWO_OP("MOV", _REGB, _CL, _I, _b),
    /* b2 */ TWO_OP("MOV", _REGB, _DL, _I, _b),
    /* b3 */ TWO_OP("MOV", _REGB, _BL, _I, _b),
    /* b4 */ TWO_OP("MOV", _REGB, _AH, _I, _b),
    /* b5 */ TWO_OP("MOV", _REGB, _CH, _I, _b),
    /* b6 */ TWO_OP("MOV", _REGB, _DH, _I, _b),
    /* b7 */ TWO_OP("MOV", _REGB, _BH, _I, _b),
    /* b8 */ TWO_OP("MOV", _REG,  _AX, _I, _v),
    /* b9 */ TWO_OP("MOV", _REG,  _CX, _I, _v),
    /* ba */ TWO_OP("MOV", _REG,  _DX, _I, _v),
    /* bb */ TWO_OP("MOV", _REG,  _BX, _I, _v),
    /* bc */ TWO_OP("MOV", _REG,  _SP, _I, _v),
    /* bd */ TWO_OP("MOV", _REG,  _BP, _I, _v),
    /* be */ TWO_OP("MOV", _REG,  _SI, _I, _v),
    /* bf */ TWO_OP("MOV", _REG,  _DI, _I, _v),
    
    /* c0 and c1 are group 2 - operation determined by mod r/m */
    /* c0 */ M_TWO_OP(group2, _E, _b, _I, _b),
    /* c1 */ M_TWO_OP(group2, _E, _v, _I, _b),
    /* c2 */ ONE_OP("RET", _I, _w),
    /* c3 */ IMP("RET"),
    /* c4 */ TWO_OP("LES", _G, _v, _M, _p),
    /* c5 */ TWO_OP("LDS", _G, _v, _M, _p),
    /* c6 */ TWO_OP("MOV", _E, _b, _I, _b),
    /* c7 */ TWO_OP("MOV", _E, _v, _I, _v),
    /* c8 */ TWO_OP("ENTER", _I, _w, _I, _b),
    /* c9 */ IMP("LEAVE"),
    /* ca */ ONE_OP("RETF", _I, _w),
    /* cb */ IMP("RETF"),
    /* cc */ IMP("INT3"),
    /* cd */ ONE_OP("INT", _I, _b),
    /* ce */ IMP("INTO"),
    /* cf */ IMP("IRET"),

    /* d0 thru d3 are group 2 - operation determined by mod r/m */
    /* d0 */ M_TWO_OP(group2, _E, _b, _CONST, 1), 
    /* d1 */ M_TWO_OP(group2, _E, _v, _CONST, 1), 
    /* d2 */ M_TWO_OP(group2, _E, _b, _REGB, _CL), 
    /* d3 */ M_TWO_OP(group2, _E, _v, _REGB, _CL),
    /* d4 */ IMP("AAM"),
    /* d5 */ IMP("AAD"),
    /* d6 */ IMP("???"),
    /* d7 */ IMP("XLAT"),
    /* d8 */ ONE_OP("ESC0", _E, _d),    /* these are x87 opcodes */
    /* d9 */ ONE_OP("ESC1", _E, _d),
    /* da */ ONE_OP("ESC2", _E, _d),
    /* db */ ONE_OP("ESC3", _E, _d),
    /* dc */ ONE_OP("ESC4", _E, _d),
    /* dd */ ONE_OP("ESC5", _E, _d),
    /* de */ ONE_OP("ESC6", _E, _d),
    /* df */ ONE_OP("ESC7", _E, _d),

    /* e0 */ J_OP("LOOPNE"),
    /* e1 */ J_OP("LOOPE"),
    /* e2 */ J_OP("LOOP"),
    /* e3 */ J_OP("JCXZ"),
    /* e4 */ TWO_OP("IN",  _REGB, _AL, _I, _b),
    /* e5 */ TWO_OP("IN",  _REG,  _AX, _I, _v),
    /* e6 */ TWO_OP("OUT", _I, _b, _REGB, _AL),
    /* e7 */ TWO_OP("OUT", _I, _v, _REG,  _AX),
    /* e8 */ ONE_OP("CALL", _J, _v),
    /* e9 */ ONE_OP("JMP",  _J, _v),
    /* ea */ ONE_OP("JMP",  _A, _p),
    /* eb */ ONE_OP("JMP",  _J, _b),
    /* ec */ TWO_OP("IN",   _REGB, _AL, _REG, _DX),
    /* ed */ TWO_OP("IN",   _REG,  _AX, _REG, _DX),
    /* ee */ TWO_OP("OUT",  _REG, _DX, _REGB, _AL),
    /* ef */ TWO_OP("OUT",  _REG, _DX, _REG,  _AX),

    /* f6 and f7 are group 3. fe and ff are group 4/5 (INC and DEC). */
    /* f0 */ IMP("LOCK"),
    /* f1 */ IMP("???"),
    /* f2 */ IMP("REPNE"),
    /* f3 */ IMP("REP"),
    /* f4 */ IMP("HLT"),
    /* f5 */ IMP("CMC"),
    /* f6 */ M_ONE_OP(group3, _E, _b),
    /* f7 */ M_ONE_OP(group3, _E, _v),
    /* f8 */ IMP("CLC"),
    /* f9 */ IMP("STC"),
    /* fa */ IMP("CLI"),
    /* fb */ IMP("STI"),
    /* fc */ IMP("CLD"),
    /* fd */ IMP("STD"),
    /* fe */ M_ONE_OP(group4, _E, _b),
    /* ff */ M_ONE_OP(group5, _E, _v),
};

/* opcodes prefixed with 0f
 */
static opcode_t extopcodes[] = 
{
    /* 00 */ M_ONE_OP(group6, _E, _w),
    /* 01 */ M_ONE_OP(group7, _M, _s),
    /* 02 */ TWO_OP("LAR", _G, _v, _E, _w),
    /* 03 */ TWO_OP("LSL", _G, _v, _E, _w),
    /* 04 */ IMP("???"),
    /* 05 */ IMP("???"),
    /* 06 */ IMP("CLTS"),
    /* 07 */ IMP("???"),
    /* 08 */ IMP("???"),
    /* 09 */ IMP("???"),
    /* 0a */ IMP("???"),
    /* 0b */ IMP("???"),
    /* 0c */ IMP("???"),
    /* 0d */ IMP("???"),
    /* 0e */ IMP("???"),
    /* 0f */ IMP("???"),

    /* 10 */ IMP("???"),
    /* 11 */ IMP("???"),
    /* 12 */ IMP("???"),
    /* 13 */ IMP("???"),
    /* 14 */ IMP("???"),
    /* 15 */ IMP("???"),
    /* 16 */ IMP("???"),
    /* 17 */ IMP("???"),
    /* 18 */ IMP("???"),
    /* 19 */ IMP("???"),
    /* 1a */ IMP("???"),
    /* 1b */ IMP("???"),
    /* 1c */ IMP("???"),
    /* 1d */ IMP("???"),
    /* 1e */ IMP("???"),
    /* 1f */ IMP("???"),

    /* 20 */ TWO_OP("MOV", _R, _d, _C, _d),
    /* 21 */ TWO_OP("MOV", _R, _d, _D, _d),
    /* 22 */ TWO_OP("MOV", _C, _d, _R, _d),
    /* 23 */ TWO_OP("MOV", _D, _d, _R, _d),
    /* 24 */ TWO_OP("MOV", _R, _d, _T, _d),
    /* 25 */ IMP("???"),
    /* 26 */ TWO_OP("MOV", _T, _d, _R, _d),
    /* 27 */ IMP("???"),
    /* 28 */ IMP("???"),
    /* 29 */ IMP("???"),
    /* 2a */ IMP("???"),
    /* 2b */ IMP("???"),
    /* 2c */ IMP("???"),
    /* 2d */ IMP("???"),
    /* 2e */ IMP("???"),
    /* 2f */ IMP("???"),

    /* 30 */ IMP("???"),
    /* 31 */ IMP("???"),
    /* 32 */ IMP("???"),
    /* 33 */ IMP("???"),
    /* 34 */ IMP("???"),
    /* 35 */ IMP("???"),
    /* 36 */ IMP("???"),
    /* 37 */ IMP("???"),
    /* 38 */ IMP("???"),
    /* 39 */ IMP("???"),
    /* 3a */ IMP("???"),
    /* 3b */ IMP("???"),
    /* 3c */ IMP("???"),
    /* 3d */ IMP("???"),
    /* 3e */ IMP("???"),
    /* 3f */ IMP("???"),

    /* 40 */ IMP("???"),
    /* 41 */ IMP("???"),
    /* 42 */ IMP("???"),
    /* 43 */ IMP("???"),
    /* 44 */ IMP("???"),
    /* 45 */ IMP("???"),
    /* 46 */ IMP("???"),
    /* 47 */ IMP("???"),
    /* 48 */ IMP("???"),
    /* 49 */ IMP("???"),
    /* 4a */ IMP("???"),
    /* 4b */ IMP("???"),
    /* 4c */ IMP("???"),
    /* 4d */ IMP("???"),
    /* 4e */ IMP("???"),
    /* 4f */ IMP("???"),

    /* 50 */ IMP("???"),
    /* 51 */ IMP("???"),
    /* 52 */ IMP("???"),
    /* 53 */ IMP("???"),
    /* 54 */ IMP("???"),
    /* 55 */ IMP("???"),
    /* 56 */ IMP("???"),
    /* 57 */ IMP("???"),
    /* 58 */ IMP("???"),
    /* 59 */ IMP("???"),
    /* 5a */ IMP("???"),
    /* 5b */ IMP("???"),
    /* 5c */ IMP("???"),
    /* 5d */ IMP("???"),
    /* 5e */ IMP("???"),
    /* 5f */ IMP("???"),

    /* 60 */ IMP("???"),
    /* 61 */ IMP("???"),
    /* 62 */ IMP("???"),
    /* 63 */ IMP("???"),
    /* 64 */ IMP("???"),
    /* 65 */ IMP("???"),
    /* 66 */ IMP("???"),
    /* 67 */ IMP("???"),
    /* 68 */ IMP("???"),
    /* 69 */ IMP("???"),
    /* 6a */ IMP("???"),
    /* 6b */ IMP("???"),
    /* 6c */ IMP("???"),
    /* 6d */ IMP("???"),
    /* 6e */ IMP("???"),
    /* 6f */ IMP("???"),

    /* 70 */ IMP("???"),
    /* 71 */ IMP("???"),
    /* 72 */ IMP("???"),
    /* 73 */ IMP("???"),
    /* 74 */ IMP("???"),
    /* 75 */ IMP("???"),
    /* 76 */ IMP("???"),
    /* 77 */ IMP("???"),
    /* 78 */ IMP("???"),
    /* 79 */ IMP("???"),
    /* 7a */ IMP("???"),
    /* 7b */ IMP("???"),
    /* 7c */ IMP("???"),
    /* 7d */ IMP("???"),
    /* 7e */ IMP("???"),
    /* 7f */ IMP("???"),

    /* 80 */ JV_OP("JO"),
    /* 81 */ JV_OP("JNO"),
    /* 82 */ JV_OP("JB"),
    /* 83 */ JV_OP("JNB"),
    /* 84 */ JV_OP("JZ"),
    /* 85 */ JV_OP("JNZ"),
    /* 86 */ JV_OP("JBE"),
    /* 87 */ JV_OP("JNBE"),
    /* 88 */ JV_OP("JS"),
    /* 89 */ JV_OP("JNS"),
    /* 8a */ JV_OP("JP"),
    /* 8b */ JV_OP("JNP"),
    /* 8c */ JV_OP("JL"),
    /* 8d */ JV_OP("JNL"),
    /* 8e */ JV_OP("JLE"),
    /* 8f */ JV_OP("JNLE"),

    /* 90 */ ONE_OP("SETO",  _E, _b),
    /* 91 */ ONE_OP("SETNO", _E, _b),
    /* 92 */ ONE_OP("SETB",  _E, _b),
    /* 93 */ ONE_OP("SETNB", _E, _b),
    /* 94 */ ONE_OP("SETZ",  _E, _b),
    /* 95 */ ONE_OP("SETNZ", _E, _b),
    /* 96 */ ONE_OP("SETBE", _E, _b),
    /* 97 */ ONE_OP("SETNBE",_E, _b),
    /* 98 */ ONE_OP("SETS",  _E, _b),
    /* 99 */ ONE_OP("SETNS", _E, _b),
    /* 9a */ ONE_OP("SETP",  _E, _b),
    /* 9b */ ONE_OP("SETNP", _E, _b),
    /* 9c */ ONE_OP("SETL",  _E, _b),
    /* 9d */ ONE_OP("SETNL", _E, _b),
    /* 9e */ ONE_OP("SETLE", _E, _b),
    /* 9f */ ONE_OP("SETNLE",_E, _b),

    /* a0 */ ONE_OP("PUSH",  _REGS, _FS),
    /* a1 */ ONE_OP("POP",   _REGS, _FS),
    /* a2 */ IMP("???"),
    /* a3 */ IMP("???"),
    /* a4 */ TWO_OP("BT",    _E, _v, _G, _v),
    /* a5 */ TRE_OP("SHLD",  _E, _v, _G, _v, _I, _b),
    /* a6 */ TRE_OP("SHLD",  _E, _v, _G, _v, _REGB, _CL),
    /* a7 */ IMP("???"),
    /* a8 */ ONE_OP("PUSH",  _REGS, _GS),
    /* a9 */ ONE_OP("POP",   _REGS, _GS),
    /* aa */ IMP("???"),
    /* ab */ TWO_OP("BT",    _E, _v, _G, _v),
    /* ac */ TRE_OP("SHLD",  _E, _v, _G, _v, _I, _b),
    /* ad */ TRE_OP("SHLD",  _E, _v, _G, _v, _REGB, _CL),
    /* ae */ IMP("???"),
    /* af */ TWO_OP("IMUL",  _G, _v, _E, _v),

    /* b0 */ IMP("???"),
    /* b1 */ IMP("???"),
    /* b2 */ ONE_OP("LSS",   _M, _p),
    /* b3 */ TWO_OP("BTR",   _E, _v, _G, _v),
    /* b4 */ ONE_OP("LFS",   _M, _p),
    /* b5 */ ONE_OP("LGS",   _M, _p),
    /* b6 */ TWO_OP("MOVZX", _G, _v, _E, _b),
    /* b7 */ TWO_OP("MOVZX", _G, _v, _E, _w),
    /* b8 */ IMP("???"),
    /* b9 */ IMP("???"),
    /* ba */ M_TWO_OP(group8,_E, _v, _I, _b),
    /* bb */ TWO_OP("BTC",   _E, _v, _G, _v),
    /* bc */ TWO_OP("BSF",   _E, _v, _G, _v),
    /* bd */ TWO_OP("BSR",   _E, _v, _G, _v),
    /* be */ TWO_OP("MOVSX", _G, _v, _E, _b),
    /* bf */ TWO_OP("MOVSX", _G, _v, _E, _w),

    /* c0 */ IMP("???"),
    /* c1 */ IMP("???"),
    /* c2 */ IMP("???"),
    /* c3 */ IMP("???"),
    /* c4 */ IMP("???"),
    /* c5 */ IMP("???"),
    /* c6 */ IMP("???"),
    /* c7 */ IMP("???"),
    /* c8 */ IMP("???"),
    /* c9 */ IMP("???"),
    /* ca */ IMP("???"),
    /* cb */ IMP("???"),
    /* cc */ IMP("???"),
    /* cd */ IMP("???"),
    /* ce */ IMP("???"),
    /* cf */ IMP("???"),

    /* d0 */ IMP("???"),
    /* d1 */ IMP("???"),
    /* d2 */ IMP("???"),
    /* d3 */ IMP("???"),
    /* d4 */ IMP("???"),
    /* d5 */ IMP("???"),
    /* d6 */ IMP("???"),
    /* d7 */ IMP("???"),
    /* d8 */ IMP("???"),
    /* d9 */ IMP("???"),
    /* da */ IMP("???"),
    /* db */ IMP("???"),
    /* dc */ IMP("???"),
    /* dd */ IMP("???"),
    /* de */ IMP("???"),
    /* df */ IMP("???"),

    /* e0 */ IMP("???"),
    /* e1 */ IMP("???"),
    /* e2 */ IMP("???"),
    /* e3 */ IMP("???"),
    /* e4 */ IMP("???"),
    /* e5 */ IMP("???"),
    /* e6 */ IMP("???"),
    /* e7 */ IMP("???"),
    /* e8 */ IMP("???"),
    /* e9 */ IMP("???"),
    /* ea */ IMP("???"),
    /* eb */ IMP("???"),
    /* ec */ IMP("???"),
    /* ed */ IMP("???"),
    /* ee */ IMP("???"),
    /* ef */ IMP("???"),

    /* f0 */ IMP("???"),
    /* f1 */ IMP("???"),
    /* f2 */ IMP("???"),
    /* f3 */ IMP("???"),
    /* f4 */ IMP("???"),
    /* f5 */ IMP("???"),
    /* f6 */ IMP("???"),
    /* f7 */ IMP("???"),
    /* f8 */ IMP("???"),
    /* f9 */ IMP("???"),
    /* fa */ IMP("???"),
    /* fb */ IMP("???"),
    /* fc */ IMP("???"),
    /* fd */ IMP("???"),
    /* fe */ IMP("???"),
    /* ff */ IMP("???"),
};

static char *wregs[8] = {
    "AX", "CX", "DX", "BX", "SP", "BP", "SI", "DI"
};

static char *bregs[8] = {
    "AL", "CL", "DL", "BL", "AH", "CH", "DH", "BH"
};

static char *sregs[8] = {
    "ES", "CS", "SS", "DS", "FS", "GS", "??", "??"
};

/* 16-bit mod r/m reg combos */
static char *baseindex[8] = {
    "BX+SI",
    "BX+DI",
    "BP+SI",
    "BP+DI",
    "SI",
    "DI",
    "BP",
    "BX",
};

#ifndef HAVE_STPCPY
static char *stpcpy(char *dst, char *src)
{
    while ((*dst++ = *src++) != 0)
        ;
    return dst-1;
}
#endif

static uint32_t extract(distate_t *state, int bytes)
{
    uint32_t val = 0;
    uint32_t byte;
    int i;

    for (i = 0; i < bytes; i++) {
        byte = *state->code++;
        val |= (byte << (i * 8));
    }

    return val;
}

static int32_t sextract(distate_t *state, int bytes)
{
    uint32_t val = extract(state, bytes);

    if (bytes == 1) {
        return (int8_t)val;
    } else if (bytes == 2) {
        return (int16_t)val;
    } else {
        return (int32_t)val; 
    }
}

static int prefix(distate_t *state)
{
    int done = 0;
    int flags = 0;

    state->addr32 = 0;
    state->data32 = 0;
    state->lock = 0;
    state->seg = SEG_NONE;
    state->rep = REP_NONE;

    while (!done) {
        switch (*state->code) {
        case 0x2e: state->seg = SEG_CS; break;
        case 0x36: state->seg = SEG_SS; break;
        case 0x3e: state->seg = SEG_DS; break;
        case 0x26: state->seg = SEG_ES; break;
        case 0x64: state->seg = SEG_FS; break;
        case 0x65: state->seg = SEG_GS; break;
        case 0x66: state->data32 = 1; break;
        case 0x67: state->addr32 = 1; break;
        case 0xf0: state->lock = 1; break;
        case 0xf2: state->rep = REP_REPNE; break;
        case 0xf3: state->rep = REP_REP; break;
        default: done = 1; break;
        }

        if (!done) {
            state->code++;
        }
    }

    return flags;
}

static void get_opadd(opcode_t *popc, int index, addop_t *p)
{
    switch (index) {
    case 0:
        p->add = popc->d_add;
        p->opreg = popc->d_opreg;
        break;

    case 1:
        p->add = popc->s_add;
        p->opreg = popc->s_opreg;
        break;

    case 2:
        p->add = popc->op3_add;
        p->opreg = popc->op3_opreg;
        break;

    default:
        p->add = 0;
        p->opreg = 0;
    }
}

static int need_modrm(opcode_t *popc)
{
    int i;
    addop_t addop;

    for (i = 0; i < 3; i++) {
        get_opadd(popc, i, &addop);
        if (addop.add == 0 && addop.opreg == 0) {
            break;
        }

        if (
            addop.add == _C ||
            addop.add == _D ||
            addop.add == _E ||
            addop.add == _G ||
            addop.add == _M ||
            addop.add == _R ||
            addop.add == _S ||
            addop.add == _T
        ) {
            return 1;
        }
    }

    return 0;
}

static void decode_sib(distate_t *state)
{
    unsigned sib = *state->code++;
    unsigned ss = SIB_SS(sib);
    unsigned index = SIB_INDEX(sib);
    unsigned base =  SIB_BASE(sib);
    unsigned mod = MODRM_MOD(state->modrm);
    int plus = 1;

    ss = 1 << ss;

    *state->args++ = '[';
    if (mod == 0 && base == 5) {
        plus = 0;
    } else {
        *state->args++ = 'E';
        state->args = stpcpy(state->args, wregs[base]);
    }
    
    if (index != 4) {
        if (plus) {
            *state->args++ = '+';
        }
        *state->args++ = 'E';
        state->args = stpcpy(state->args, wregs[index]);
    
        if (ss != 1) {
            *state->args++ = '*';
            state->args += sprintf(state->args, "%u", ss);
        }
        plus = 1;
    }

    if (mod == 1 || mod == 2) {
        /* there's a displacement */
        uint32_t disp = extract(state, mod == 1 ? 1 : 4);
        if (disp) {
            if (plus) {
                *state->args++ = '+';
            }
            if (mod == 1) {
                state->args += sprintf(state->args, "%02x", (uint16_t)disp);
            } else {
                state->args += sprintf(state->args, "%08lx", (uint32_t)disp);
            }
        }
    }

    *state->args++ = ']';
}

static char *seg_override(distate_t *state)
{
    switch (state->seg) {
    case SEG_CS: return "CS:"; 
    case SEG_DS: return "DS:";
    case SEG_ES: return "ES:";
    case SEG_SS: return "SS:";
    case SEG_FS: return "FS:";
    case SEG_GS: return "GS:";
    }
    return "";
}

static void decode_modrm(distate_t *state, int oper)
{
    unsigned mod = MODRM_MOD(state->modrm);
    unsigned rm = MODRM_RM(state->modrm);
    uint32_t disp;
    int addr32 = (state->addr32) != 0;

    /* mod == 3 is the same regardless of addr32 */
    if (mod == 3) {
        if (oper == _b) {
            state->args = stpcpy(state->args, bregs[rm]);
        } else {
            if (oper == _d || (oper == _v && (state->data32))) {
                *state->args++ = 'E';
            }
            state->args = stpcpy(state->args, wregs[rm]);
        } 
        return;
    }

    if (oper == _b) {
        state->args = stpcpy(state->args, "BYTE PTR ");
    } else if ((oper == _w) || (oper == _v && (state->data32) == 0)) {
        state->args = stpcpy(state->args, "WORD PTR ");
    } else if ((oper == _d) || (oper == _v && (state->data32) != 0)) {
        state->args = stpcpy(state->args, "DWORD PTR ");
    }
    
    state->args = stpcpy(state->args, seg_override(state));

    /* check special case of just a disp, which is slightly
     * different depending on 16 or 32 bit addressing
     */
    if (mod == 0) {
        if (addr32 && rm == 5) {
            disp = extract(state, 4);
            state->args += sprintf(state->args, "[%08lx]", disp);
            return;
        }
        
        if (!addr32 && rm == 6) {
            disp = extract(state, 2);
            state->args += sprintf(state->args, "[%04x]", (uint16_t)disp);
            return;
        }
    }

    if (addr32 && mod != 3 && rm == 4) {
        decode_sib(state);
        return;
    }

    *state->args++ = '[';
    if (addr32) {
        *state->args++ = 'E';
        state->args = stpcpy(state->args, wregs[rm]);
    } else {
        state->args = stpcpy(state->args, baseindex[rm]);
    }

    if (mod == 1 || mod == 2) {
        if (addr32 && mod == 2) {
            /* mod 2 in 32-bit addressing means a 32-bit displacement */
            mod = 4;
        }
        disp = extract(state, mod);
        if (disp) {
            if (mod == 1) {
                state->args += sprintf(state->args, "+%02x", (uint16_t)disp);
            } else if (addr32) {
                state->args += sprintf(state->args, "+%08lx", (uint32_t)disp);
            } else {
                state->args += sprintf(state->args, "+%04x", (uint16_t)disp);
            }
        }
    }
    *state->args++ = ']';
}

void disasm(uint8_t __far *csip, uint32_t offset, disinstr_t *out, int is32)
{
    distate_t state;
    uint8_t op;
    uint8_t op2 = 0;
    opcode_t *popc;
    addop_t addop;
    int i;
    int bytes;
    uint32_t seg, offs;
    int32_t disp;
    char *pmne;

    state.code = csip;
    state.args = out->args;
    state.modrm = 0;
    state.sib = 0;
    prefix(&state);

    op = *state.code++;
    popc = &opcodes[op];
    if (op == 0x0f) {
        op2 = *state.code++;
        popc = &extopcodes[op2];
    }

    if (need_modrm(popc) || popc->modop) {
        state.modrm = *state.code++;
    }

    if (is32) {
        state.addr32 = !state.addr32;
        state.data32 = !state.data32;
    }

    pmne = out->mne;

    if (state.lock) {
        pmne = stpcpy(pmne, "LOCK ");
    }

    if (state.rep == REP_REPNE) {
        pmne = stpcpy(pmne, "REPNE ");
    } else if (state.rep == REP_REP) {
        pmne = stpcpy(pmne, "REP ");
    }

    if (popc->modop) {
        const char **ops = (const char **)popc->mne;
        strcpy(pmne, ops[MODRM_REG(state.modrm)]);
    } else {
        strcpy(pmne, popc->mne);
    } 
    
    for (i = 0; i < 3; i++) {
        get_opadd(popc, i, &addop);

        /* exceptions for group 3 encoding */
        if ((op == 0xf6 || op == 0xf7) && MODRM_REG(state.modrm) < 2 && i == 1) {
            addop.add = _I;
            addop.opreg = (op & 1) ? _v : _b;
        }

        /* exceptions for group 5 encoding */
        if (op == 0xff && (MODRM_REG(state.modrm) == 3 || MODRM_REG(state.modrm) == 5)) {
            addop.opreg = _p;
        }

        /* exceptions for group 7 encoding */
        if (op == 0x0f && op2 == 0x01 && MODRM_REG(state.modrm) > 3) {
            addop.add = _E;
            addop.opreg = _w;
        }

        if (addop.add == 0 && addop.opreg == 0) {
            break;
        }

        if (i) {
            *state.args++ = ',';
            *state.args++ = ' ';
        }

        switch (addop.add) {
        case _A: /* only appears with _p */
            offs = extract(&state, 2);
            seg = extract(&state, 2);
            state.args += sprintf(state.args, "%04x:%04x", (uint16_t)seg, (uint16_t)offs); 
            break;


        case _C:
            state.args += sprintf(state.args, "CR%u", MODRM_REG(state.modrm));
            break;

        case _D:
            state.args += sprintf(state.args, "DR%u", MODRM_REG(state.modrm));
            break;

        case _E:
        case _M:
            /* TODO
             * _p
             * use FAR rather than BYTE/WORD PTR for call and jump targets
             */
            decode_modrm(&state, addop.opreg);
            break;

        case _G:
            if (addop.opreg == _b) {
                state.args = stpcpy(state.args, bregs[MODRM_REG(state.modrm)]);
            } else if (addop.opreg == _v) {
                if (state.data32) {
                    *state.args++ = 'E';
                }
                state.args = stpcpy(state.args, wregs[MODRM_REG(state.modrm)]);
            }
            break;

        case _I:
            if (addop.opreg == _b) {
                offs = extract(&state, 1);
                state.args += sprintf(state.args, "%02x", (uint16_t)offs); 
            } else if (addop.opreg == _w) {
                offs = extract(&state, 2);
                state.args += sprintf(state.args, "%04x", (uint16_t)offs); 
            } else if (addop.opreg == _v) {
                offs = extract(&state, state.data32 ? 4 : 2);
                state.args += sprintf(state.args, state.data32 ? "%08lx" : "%04x", offs); 
            }
            break;

        case _J:
            bytes = 1;
            if (addop.opreg == _v) {
                bytes = state.addr32 ? 4 : 2;
            }

            disp = sextract(&state, bytes);
            offset += state.code - csip;
            state.args += sprintf(state.args, state.data32 ? "%08lx" : "%04x", disp + offset);
                
            break;

        case _O:
            if (addop.opreg == _b) {
                state.args = stpcpy(state.args, "BYTE PTR ");
            } else if ((addop.opreg == _w) || (addop.opreg == _v && (state.data32) == 0)) {
                state.args = stpcpy(state.args, "WORD PTR ");
            } else if ((addop.opreg == _d) || (addop.opreg == _v && (state.data32) != 0)) {
                state.args = stpcpy(state.args, "DWORD PTR ");
            }

            if (state.addr32) {
                offs = extract(&state, 4);
                state.args += sprintf(state.args, "[%08lx]", offs);
            } else {
                offs = extract(&state, 2);
                state.args += sprintf(state.args, "[%04x]", (uint16_t)offs);
            }
            break;

        case _R:
            /* NB the docs say mod of MODRM but they mean r/m */
            if (addop.opreg == _b) {
                state.args = stpcpy(state.args, bregs[MODRM_RM(state.modrm)]);
            } else if (addop.opreg == _v || addop.opreg == _d) {
                if (addop.opreg == _d || state.data32) {
                    *state.args++ = 'E';
                }
                state.args = stpcpy(state.args, wregs[MODRM_RM(state.modrm)]);
            }
            break;

        case _S:
            state.args = stpcpy(state.args, sregs[MODRM_REG(state.modrm)]);
            break;

        case _T:
            state.args += sprintf(state.args, "TR%u", MODRM_REG(state.modrm));
            break;

        case _X:
            if (addop.opreg == _b) {
                state.args = stpcpy(state.args, "BYTE PTR ");
            } else if (addop.opreg == _v) {
                if (state.data32) {
                    *state.args++ = 'D';
                }
                state.args = stpcpy(state.args, "WORD PTR ");
            }
            state.args = stpcpy(state.args, seg_override(&state));
            state.args = stpcpy(state.args, "[SI]");
            break;

        case _Y:
            if (addop.opreg == _b) {
                state.args = stpcpy(state.args, "BYTE PTR");
            } else if (addop.opreg == _v) {
                if (state.data32) {
                    *state.args++ = 'D';
                }
                state.args = stpcpy(state.args, "WORD PTR");
            }
            state.args = stpcpy(state.args, " [DI]");
            break;

        case _REG:
            if (state.data32) {
                *state.args++ = 'E';
            }
            state.args = stpcpy(state.args, wregs[addop.opreg]);
            break;

        case _REGB:
            state.args = stpcpy(state.args, bregs[addop.opreg]);
            break;
            
        case _REGS:
            state.args = stpcpy(state.args, sregs[addop.opreg]);
            break;
        }
    }

    *state.args = 0;
    if (state.args - out->args >= sizeof(out->args)) {
       panic("disassembler overrun");
    }
    
    out->bytes = state.code - csip;
}
