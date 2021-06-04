#include "lmnt/opcodes.h"

#include "lmnt/interpreter.h"
#include "lmnt/ops_bounds.h"
#include "lmnt/ops_branch.h"
#include "lmnt/ops_fncall.h"
#include "lmnt/ops_math.h"
#include "lmnt/ops_misc.h"
#include "lmnt/ops_trig.h"

LMNT_ATTR_FAST lmnt_op_fn lmnt_op_functions[LMNT_OP_END] = {
    lmnt_op_noop,
    lmnt_op_return,
    lmnt_op_assignss,
    lmnt_op_assignvv,
    lmnt_op_assignsv,
    lmnt_op_assigniis,
    lmnt_op_assignibs,
    lmnt_op_assigniiv,
    lmnt_op_assignibv,
    lmnt_op_dloadiis,
    lmnt_op_dloadiiv,
    lmnt_op_dloadirs,
    lmnt_op_dloadirv,
    lmnt_op_dseclen,
    lmnt_op_addss,
    lmnt_op_addvv,
    lmnt_op_subss,
    lmnt_op_subvv,
    lmnt_op_mulss,
    lmnt_op_mulvv,
    lmnt_op_divss,
    lmnt_op_divvv,
    lmnt_op_modss,
    lmnt_op_modvv,
    lmnt_op_sin,
    lmnt_op_cos,
    lmnt_op_tan,
    lmnt_op_asin,
    lmnt_op_acos,
    lmnt_op_atan,
    lmnt_op_atan2,
    lmnt_op_sincos,
    lmnt_op_powss,
    lmnt_op_powvv,
    lmnt_op_powvs,
    lmnt_op_sqrts,
    lmnt_op_sqrtv,
    lmnt_op_log,
    lmnt_op_ln,
    lmnt_op_log2,
    lmnt_op_log10,
    lmnt_op_abss,
    lmnt_op_absv,
    lmnt_op_sumv,
    lmnt_op_minss,
    lmnt_op_minvv,
    lmnt_op_maxss,
    lmnt_op_maxvv,
    lmnt_op_minvs,
    lmnt_op_maxvs,
    lmnt_op_floors,
    lmnt_op_floorv,
    lmnt_op_rounds,
    lmnt_op_roundv,
    lmnt_op_ceils,
    lmnt_op_ceilv,
    lmnt_op_truncs,
    lmnt_op_truncv,
    lmnt_op_indexris,
    lmnt_op_indexrir,
    lmnt_op_cmp,
    lmnt_op_cmpz,
    lmnt_op_branch,
    lmnt_op_branchceq,
    lmnt_op_branchcne,
    lmnt_op_branchclt,
    lmnt_op_branchcle,
    lmnt_op_branchcgt,
    lmnt_op_branchcge,
    lmnt_op_branchcun,
    lmnt_op_branchz,
    lmnt_op_branchnz,
    lmnt_op_branchpos,
    lmnt_op_branchneg,
    lmnt_op_branchun,
    lmnt_op_extcall,
};

const lmnt_op_info lmnt_opcode_info[LMNT_OP_END] = {
    { "NOOP",      LMNT_OPERAND_UNUSED,   LMNT_OPERAND_UNUSED,   LMNT_OPERAND_UNUSED   },
    { "RETURN",    LMNT_OPERAND_UNUSED,   LMNT_OPERAND_UNUSED,   LMNT_OPERAND_UNUSED   },
    { "ASSIGNSS",  LMNT_OPERAND_STACK1,   LMNT_OPERAND_UNUSED,   LMNT_OPERAND_STACK1   },
    { "ASSIGNVV",  LMNT_OPERAND_STACK4,   LMNT_OPERAND_UNUSED,   LMNT_OPERAND_STACK4   },
    { "ASSIGNSV",  LMNT_OPERAND_STACK1,   LMNT_OPERAND_UNUSED,   LMNT_OPERAND_STACK4   },
    { "ASSIGNIIS", LMNT_OPERAND_IMM,      LMNT_OPERAND_IMM,      LMNT_OPERAND_STACK1   },
    { "ASSIGNIBS", LMNT_OPERAND_IMM,      LMNT_OPERAND_IMM,      LMNT_OPERAND_STACK1   },
    { "ASSIGNIIV", LMNT_OPERAND_IMM,      LMNT_OPERAND_IMM,      LMNT_OPERAND_STACK4   },
    { "ASSIGNIBV", LMNT_OPERAND_IMM,      LMNT_OPERAND_IMM,      LMNT_OPERAND_STACK4   },
    { "DLOADIIS",  LMNT_OPERAND_IMM,      LMNT_OPERAND_IMM,      LMNT_OPERAND_STACK1   },
    { "DLOADIIV",  LMNT_OPERAND_IMM,      LMNT_OPERAND_IMM,      LMNT_OPERAND_STACK4   },
    { "DLOADIRS",  LMNT_OPERAND_IMM,      LMNT_OPERAND_STACK1,   LMNT_OPERAND_STACK1   },
    { "DLOADIRV",  LMNT_OPERAND_IMM,      LMNT_OPERAND_STACK1,   LMNT_OPERAND_STACK4   },
    { "DSECLEN",   LMNT_OPERAND_IMM,      LMNT_OPERAND_UNUSED,   LMNT_OPERAND_STACK1   },
    { "ADDSS",     LMNT_OPERAND_STACK1,   LMNT_OPERAND_STACK1,   LMNT_OPERAND_STACK1   },
    { "ADDVV",     LMNT_OPERAND_STACK4,   LMNT_OPERAND_STACK4,   LMNT_OPERAND_STACK4   },
    { "SUBSS",     LMNT_OPERAND_STACK1,   LMNT_OPERAND_STACK1,   LMNT_OPERAND_STACK1   },
    { "SUBVV",     LMNT_OPERAND_STACK4,   LMNT_OPERAND_STACK4,   LMNT_OPERAND_STACK4   },
    { "MULSS",     LMNT_OPERAND_STACK1,   LMNT_OPERAND_STACK1,   LMNT_OPERAND_STACK1   },
    { "MULVV",     LMNT_OPERAND_STACK4,   LMNT_OPERAND_STACK4,   LMNT_OPERAND_STACK4   },
    { "DIVSS",     LMNT_OPERAND_STACK1,   LMNT_OPERAND_STACK1,   LMNT_OPERAND_STACK1   },
    { "DIVVV",     LMNT_OPERAND_STACK4,   LMNT_OPERAND_STACK4,   LMNT_OPERAND_STACK4   },
    { "MODSS",     LMNT_OPERAND_STACK1,   LMNT_OPERAND_STACK1,   LMNT_OPERAND_STACK1   },
    { "MODVV",     LMNT_OPERAND_STACK4,   LMNT_OPERAND_STACK4,   LMNT_OPERAND_STACK4   },
    { "SIN",       LMNT_OPERAND_STACK1,   LMNT_OPERAND_UNUSED,   LMNT_OPERAND_STACK1   },
    { "COS",       LMNT_OPERAND_STACK1,   LMNT_OPERAND_UNUSED,   LMNT_OPERAND_STACK1   },
    { "TAN",       LMNT_OPERAND_STACK1,   LMNT_OPERAND_UNUSED,   LMNT_OPERAND_STACK1   },
    { "ASIN",      LMNT_OPERAND_STACK1,   LMNT_OPERAND_UNUSED,   LMNT_OPERAND_STACK1   },
    { "ACOS",      LMNT_OPERAND_STACK1,   LMNT_OPERAND_UNUSED,   LMNT_OPERAND_STACK1   },
    { "ATAN",      LMNT_OPERAND_STACK1,   LMNT_OPERAND_UNUSED,   LMNT_OPERAND_STACK1   },
    { "ATAN2",     LMNT_OPERAND_STACK1,   LMNT_OPERAND_STACK1,   LMNT_OPERAND_STACK1   },
    { "SINCOS",    LMNT_OPERAND_STACK1,   LMNT_OPERAND_STACK1,   LMNT_OPERAND_STACK1   },
    { "POWSS",     LMNT_OPERAND_STACK1,   LMNT_OPERAND_STACK1,   LMNT_OPERAND_STACK1   },
    { "POWVV",     LMNT_OPERAND_STACK4,   LMNT_OPERAND_STACK4,   LMNT_OPERAND_STACK4   },
    { "POWVS",     LMNT_OPERAND_STACK4,   LMNT_OPERAND_STACK1,   LMNT_OPERAND_STACK4   },
    { "SQRTS",     LMNT_OPERAND_STACK1,   LMNT_OPERAND_UNUSED,   LMNT_OPERAND_STACK1   },
    { "SQRTV",     LMNT_OPERAND_STACK4,   LMNT_OPERAND_UNUSED,   LMNT_OPERAND_STACK4   },
    { "LOG",       LMNT_OPERAND_STACK1,   LMNT_OPERAND_STACK1,   LMNT_OPERAND_STACK1   },
    { "LN",        LMNT_OPERAND_STACK1,   LMNT_OPERAND_UNUSED,   LMNT_OPERAND_STACK1   },
    { "LOG2",      LMNT_OPERAND_STACK1,   LMNT_OPERAND_UNUSED,   LMNT_OPERAND_STACK1   },
    { "LOG10",     LMNT_OPERAND_STACK1,   LMNT_OPERAND_UNUSED,   LMNT_OPERAND_STACK1   },
    { "ABSS",      LMNT_OPERAND_STACK1,   LMNT_OPERAND_UNUSED,   LMNT_OPERAND_STACK1   },
    { "ABSV",      LMNT_OPERAND_STACK4,   LMNT_OPERAND_UNUSED,   LMNT_OPERAND_STACK4   },
    { "SUMV",      LMNT_OPERAND_STACK4,   LMNT_OPERAND_UNUSED,   LMNT_OPERAND_STACK1   },
    { "MINSS",     LMNT_OPERAND_STACK1,   LMNT_OPERAND_STACK1,   LMNT_OPERAND_STACK1   },
    { "MINVV",     LMNT_OPERAND_STACK4,   LMNT_OPERAND_STACK4,   LMNT_OPERAND_STACK4   },
    { "MAXSS",     LMNT_OPERAND_STACK1,   LMNT_OPERAND_STACK1,   LMNT_OPERAND_STACK1   },
    { "MAXVV",     LMNT_OPERAND_STACK4,   LMNT_OPERAND_STACK4,   LMNT_OPERAND_STACK4   },
    { "MINVS",     LMNT_OPERAND_STACK4,   LMNT_OPERAND_STACK1,   LMNT_OPERAND_STACK4   },
    { "MAXVS",     LMNT_OPERAND_STACK4,   LMNT_OPERAND_STACK1,   LMNT_OPERAND_STACK4   },
    { "FLOORS",    LMNT_OPERAND_STACK1,   LMNT_OPERAND_UNUSED,   LMNT_OPERAND_STACK1   },
    { "FLOORV",    LMNT_OPERAND_STACK4,   LMNT_OPERAND_UNUSED,   LMNT_OPERAND_STACK4   },
    { "ROUNDS",    LMNT_OPERAND_STACK1,   LMNT_OPERAND_UNUSED,   LMNT_OPERAND_STACK1   },
    { "ROUNDV",    LMNT_OPERAND_STACK4,   LMNT_OPERAND_UNUSED,   LMNT_OPERAND_STACK4   },
    { "CEILS",     LMNT_OPERAND_STACK1,   LMNT_OPERAND_UNUSED,   LMNT_OPERAND_STACK1   },
    { "CEILV",     LMNT_OPERAND_STACK4,   LMNT_OPERAND_UNUSED,   LMNT_OPERAND_STACK4   },
    { "TRUNCS",    LMNT_OPERAND_STACK1,   LMNT_OPERAND_UNUSED,   LMNT_OPERAND_STACK1   },
    { "TRUNCV",    LMNT_OPERAND_STACK4,   LMNT_OPERAND_UNUSED,   LMNT_OPERAND_STACK4   },
    { "INDEXRIS",  LMNT_OPERAND_STACKREF, LMNT_OPERAND_IMM,      LMNT_OPERAND_STACK1   },
    { "INDEXRIR",  LMNT_OPERAND_STACKREF, LMNT_OPERAND_IMM,      LMNT_OPERAND_STACKREF },
    { "CMP",       LMNT_OPERAND_STACK1,   LMNT_OPERAND_STACK1,   LMNT_OPERAND_UNUSED   },
    { "CMPZ",      LMNT_OPERAND_STACK1,   LMNT_OPERAND_UNUSED,   LMNT_OPERAND_UNUSED   },
    { "BRANCH",    LMNT_OPERAND_UNUSED,   LMNT_OPERAND_CODEPTR,  LMNT_OPERAND_CODEPTR  },
    { "BRANCHCEQ", LMNT_OPERAND_UNUSED,   LMNT_OPERAND_CODEPTR,  LMNT_OPERAND_CODEPTR  },
    { "BRANCHCNE", LMNT_OPERAND_UNUSED,   LMNT_OPERAND_CODEPTR,  LMNT_OPERAND_CODEPTR  },
    { "BRANCHCLT", LMNT_OPERAND_UNUSED,   LMNT_OPERAND_CODEPTR,  LMNT_OPERAND_CODEPTR  },
    { "BRANCHCLE", LMNT_OPERAND_UNUSED,   LMNT_OPERAND_CODEPTR,  LMNT_OPERAND_CODEPTR  },
    { "BRANCHCGT", LMNT_OPERAND_UNUSED,   LMNT_OPERAND_CODEPTR,  LMNT_OPERAND_CODEPTR  },
    { "BRANCHCGE", LMNT_OPERAND_UNUSED,   LMNT_OPERAND_CODEPTR,  LMNT_OPERAND_CODEPTR  },
    { "BRANCHCUN", LMNT_OPERAND_UNUSED,   LMNT_OPERAND_CODEPTR,  LMNT_OPERAND_CODEPTR  },
    { "BRANCHZ",   LMNT_OPERAND_STACK1,   LMNT_OPERAND_CODEPTR,  LMNT_OPERAND_CODEPTR  },
    { "BRANCHNZ",  LMNT_OPERAND_STACK1,   LMNT_OPERAND_CODEPTR,  LMNT_OPERAND_CODEPTR  },
    { "BRANCHPOS", LMNT_OPERAND_STACK1,   LMNT_OPERAND_CODEPTR,  LMNT_OPERAND_CODEPTR  },
    { "BRANCHNEG", LMNT_OPERAND_STACK1,   LMNT_OPERAND_CODEPTR,  LMNT_OPERAND_CODEPTR  },
    { "BRANCHUN",  LMNT_OPERAND_STACK1,   LMNT_OPERAND_CODEPTR,  LMNT_OPERAND_CODEPTR  },
    { "EXTCALL",   LMNT_OPERAND_DEFPTR,   LMNT_OPERAND_DEFPTR,   LMNT_OPERAND_STACKN   },
};

const lmnt_op_info* lmnt_get_opcode_info(lmnt_opcode op)
{
    return LMNT_LIKELY(op < LMNT_OP_END) ? &(lmnt_opcode_info[op]) : NULL;
}

lmnt_op_fn lmnt_interrupt_functions[LMNT_OP_END] = {
    lmnt_op_interrupt,
    lmnt_op_interrupt,
    lmnt_op_interrupt,
    lmnt_op_interrupt,
    lmnt_op_interrupt,
    lmnt_op_interrupt,
    lmnt_op_interrupt,
    lmnt_op_interrupt,
    lmnt_op_interrupt,
    lmnt_op_interrupt,
    lmnt_op_interrupt,
    lmnt_op_interrupt,
    lmnt_op_interrupt,
    lmnt_op_interrupt,
    lmnt_op_interrupt,
    lmnt_op_interrupt,
    lmnt_op_interrupt,
    lmnt_op_interrupt,
    lmnt_op_interrupt,
    lmnt_op_interrupt,
    lmnt_op_interrupt,
    lmnt_op_interrupt,
    lmnt_op_interrupt,
    lmnt_op_interrupt,
    lmnt_op_interrupt,
    lmnt_op_interrupt,
    lmnt_op_interrupt,
    lmnt_op_interrupt,
    lmnt_op_interrupt,
    lmnt_op_interrupt,
    lmnt_op_interrupt,
    lmnt_op_interrupt,
    lmnt_op_interrupt,
    lmnt_op_interrupt,
    lmnt_op_interrupt,
    lmnt_op_interrupt,
    lmnt_op_interrupt,
    lmnt_op_interrupt,
    lmnt_op_interrupt,
    lmnt_op_interrupt,
    lmnt_op_interrupt,
    lmnt_op_interrupt,
    lmnt_op_interrupt,
    lmnt_op_interrupt,
    lmnt_op_interrupt,
    lmnt_op_interrupt,
    lmnt_op_interrupt,
    lmnt_op_interrupt,
    lmnt_op_interrupt,
    lmnt_op_interrupt,
    lmnt_op_interrupt,
    lmnt_op_interrupt,
    lmnt_op_interrupt,
    lmnt_op_interrupt,
    lmnt_op_interrupt,
    lmnt_op_interrupt,
    lmnt_op_interrupt,
    lmnt_op_interrupt,
    lmnt_op_interrupt,
    lmnt_op_interrupt,
    lmnt_op_interrupt,
    lmnt_op_interrupt,
    lmnt_op_interrupt,
    lmnt_op_interrupt,
    lmnt_op_interrupt,
    lmnt_op_interrupt,
    lmnt_op_interrupt,
    lmnt_op_interrupt,
    lmnt_op_interrupt,
    lmnt_op_interrupt,
    lmnt_op_interrupt,
    lmnt_op_interrupt,
    lmnt_op_interrupt,
    lmnt_op_interrupt,
    lmnt_op_interrupt,
    lmnt_op_interrupt,
};
