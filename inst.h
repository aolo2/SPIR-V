static const u32 WORDCOUNT_MASK     = 0xFFFF0000;
static const u32 OPCODE_MASK        = 0x0000FFFF;

enum opcode_t {
    OpSourceContinued = 2, // enum only, is not parsed
    OpSource = 3,          // enum only, is not parsed
    OpSourceExtension = 4, // enum only, is not parsed
    OpName = 5,
    OpString = 7,          // enum only, is not parsed
    OpExecutionMode = 16,  // enum only, is not parsed
    OpTypePointer = 32,
    OpVariable = 59,
    OpLoad = 61,
    OpStore = 62,
    OpCopyObject = 83,
    OpSNegate = 126,
    OpFNegate = 127,
    OpIAdd = 128,
    OpFAdd = 129,
    OpISub = 130,
    OpFSub = 131,
    OpIMul = 132,
    OpFMul = 133,
    OpUDiv = 134,
    OpSDiv = 135,
    OpFDiv = 136,
    OpUMod = 137,
    OpSRem = 138,
    OpSMod = 139,
    OpFRem = 140,
    OpFMod = 141,
    
    /* Enum only */
    OpIEqual = 170,
    OpINotEqual = 171,
    OpUGreaterThan = 172,
    OpSGreaterThan = 173,
    OpUGreaterThanEqual = 174,
    OpSGreaterThanEqual = 175,
    OpULessThan = 176,
    OpSLessThan = 177,
    OpULessThanEqual = 178,
    OpSLessThanEqual = 179,
    OpFOrdEqual = 180,
    OpFUnordEqual = 181,
    OpFOrdNotEqual = 182,
    OpFUnordNotEqual = 183,
    OpFOrdLessThan = 184,
    OpFUnordLessThan = 185,
    OpFOrdGreaterThan = 186,
    OpFUnordGreaterThan = 187,
    OpFOrdLessThanEqual = 188,
    OpFUnordLessThanEqual = 189 ,
    OpFOrdGreaterThanEqual = 190,
    OpFUnordGreaterThanEqual = 191,
    
    /*************/
    
    OpPhi = 245,
    OpLoopMerge = 246,
    OpSelectionMerge = 247,
    OpLabel = 248,
    OpBranch = 249,
    OpBranchConditional = 250,
    OpReturn = 253,
};

struct opname_t {
    u32 target_id;
    char *name;
};

struct oplabel_t {
    u32 result_id;
};

struct opvariable_t {
    u32 result_type;
    u32 result_id;
    u32 storage_class;
    u32 initializer; // optional
};

struct optypepointer_t {
    u32 result_id;
    u32 storage_class;
    u32 type;
};

struct opbranch_t {
    u32 target_label;
};

struct opbranchconditional_t {
    u32 condition;
    u32 true_label;
    u32 false_label;
    // does not support label weights
};

struct opselectionmerge_t {
    u32 merge_block;
    u32 selection_control;
};

struct opphi_t {
    u32 result_type;
    u32 result_id;
    u32 *variables;
    u32 *parents;
};

struct oploopmerge_t {
    u32 merge_block;
    u32 continue_block;
    u32 loop_control;
};

struct opstore_t {
    u32 pointer;
    u32 object;
    u32 memory_access; // optional
};

struct opload_t {
    u32 result_type;
    u32 result_id;
    u32 pointer;
    u32 memory_access; // optional
};

struct opcopyobject_t {
    u32 result_type;
    u32 result_id;
    u32 operand;
};

struct unary_arithmetics_layout {
    u32 result_type;
    u32 result_id;
    u32 operand;
};

struct binary_arithmetics_layout {
    u32 result_type;
    u32 result_id;
    u32 operand_1;
    u32 operand_2;
};

struct instruction_t {
    enum opcode_t opcode;
    u32 wordcount;
    u32 *unparsed_words;
    
    union {
        struct opname_t OpName;
        struct oplabel_t OpLabel;
        struct opvariable_t OpVariable;
        struct optypepointer_t OpTypePointer;
        struct opbranch_t OpBranch;
        struct opbranchconditional_t OpBranchConditional;
        struct opphi_t OpPhi;
        struct opselectionmerge_t OpSelectionMerge;
        struct oploopmerge_t OpLoopMerge;
        struct opstore_t OpStore;
        struct opload_t OpLoad;
        struct opcopyobject_t OpCopyObject;
        struct unary_arithmetics_layout unary_arithmetics;
        struct binary_arithmetics_layout binary_arithmetics;
    };
};
