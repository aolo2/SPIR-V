#include "common.h"

// NOTE: SPIR-V file constants
static const u32 SPIRV_HEADER_WORDS = 4;
static const u32 WORDCOUNT_MASK     = 0xFFFF0000;
static const u32 OPCODE_MASK        = 0x0000FFFF;

// NOTE: instruction operand offset constants
static const u32 OPLABEL_RESID_INDEX = 1;
static const u32 OPBRANCHCOND_COND_INDEX = 1;
static const u32 OPBRANCHCOND_TLABEL_INDEX = 2;
static const u32 OPBRANCHCOND_FLABEL_INDEX = 3;
static const u32 OPSELECTIONMERGE_WORDCOUNT = 3;
static const u32 OPSELECTIONMERGE_MERGEBLOCK_INDEX = 1;
static const u32 OPRETURNVALUE_VALUE_INDEX = 1;
static const u32 OPBRANCH_TARGETLABEL_INDEX = 1;
static const u32 OPLOOPMERGE_MERGE_INDEX = 1;
static const u32 OPLOOPMERGE_CONTINUE_INDEX = 2;
static const u32 CONSTANTS_RESID_INDEX = 2;
static const u32 OPVARIABLE_RESID_INDEX = 2;
static const u32 OPSTORE_POINTER_INDEX = 1;
static const u32 OPSTORE_OBJECT_INDEX = 2;
static const u32 OPLOAD_RESID_INDEX = 2;
static const u32 OPLOAD_POINTER_INDEX = 3;
static const u32 ARITHMETIC_OPS_RESID_INDEX = 2;
static const u32 ARITHMETIC_OPS_OPERANDS_START_INDEX = 3;
static const u32 OPSTORE_OPS_OPERANDS_START_INDEX = 1;

struct uint_vector;
struct int_queue;
struct cfgc;
struct dfs_result;
struct bfs_result;
struct int_stack;
struct spirv_header;
struct instruction;
struct basic_block;
struct parse_result;
struct list;
struct hashset;
struct ir;
struct lim_data;

// NOTE: utils
static u32 *get_binary(const char *filename, u32 *size);
static inline s32 search_item_u32(u32 *array, u32 count, u32 item);
static inline void show_array_u32(u32 *array, u32 size, const char *header);
static inline void show_array_s32(s32 *array, u32 size, const char *header);
static inline void show_vector_u32(struct uint_vector *v, const char *header);
static void show_cfg(struct ir *file, struct cfgc *cfg, u32 *bb_labels, u32 num_bb);

// NOTE: SPIRV parsing and CFG construction
static bool is_constant_insruction(u32 opcode);
static bool is_termination_instruction(u32 opcode);
static bool is_result_id_producer(u32 opcode);
static struct parse_result parse_words(u32 *words, u32 num_words, u32 *num_instructions, u32 *num_bb);
static struct cfgc construct_cfg(struct ir *file, u32 *bb_labels, u32 num_instructions, u32 num_bb);
static u32 extract_res_id(struct instruction *inst, u32 *words);
static u32 extract_operand_count(struct instruction *inst);
static u32 extract_nth_operand(struct instruction *inst, u32 *words, u32 n);
static bool matters_in_cycle(u32 opcode);

// NOTE: SPIR-V names
static const char *names_opcode(u32 code);

// NOTE: CFG
static struct dfs_result cfgc_dfs(struct cfgc *graph);
static struct uint_vector cfgc_bfs_(struct cfgc *graph, u32 root, s32 terminate);
//static struct bfs_result cfg_bfs(struct cfgc *graph);
static struct uint_vector cfgc_bfs_restricted(struct cfgc *graph, u32 header, u32 merge);
static void dfs_result_free(struct dfs_result *res);
static void bfs_result_free(struct bfs_result *res);
static bool cfgc_preorder_less(u32 *preorder, u32 a, u32 b);
static u32 cfgc_find_min(u32 *preorder, u32 *sdom, u32 *label, s32 *ancestor, u32 v);
static s32 *cfgc_dominators(struct cfgc *input);
static void cfgc_free(struct cfgc *graph);
static void add_incoming_edges(struct cfgc *cfg);

// NOTE: stack
static struct int_stack stack_init();
static void stack_push(struct int_stack *s, s32 x);
static s32 stack_pop(struct int_stack *s);
static void stack_clear(struct int_stack *s);
static void stack_free(struct int_stack *s);

// NOTE: queue
static struct int_queue queue_init();
static void queue_push(struct int_queue *queue, s32 x);
static s32 queue_pop(struct int_queue *queue);
static void queue_free(struct int_queue *queue);

// NOTE: vector
static struct uint_vector vector_init();
static struct uint_vector vector_init_sized(u32 size);
static void vector_free(struct uint_vector *v);
static void vector_push(struct uint_vector *v, u32 item);
static struct uint_vector vector_copy(struct uint_vector *v);

// NOTE: list
static struct list *list_make_item(u32 key, char *val, u32 width);
static struct list *list_add_after(struct list *add_after_me, struct list *add_me);
static bool list_add_to_end(struct list *head, struct list *add_me);
static void list_del_after(struct list *del_after_me);
static void list_free_item(struct list *item);
static void list_del_head(struct list **data, u32 index);
static void list_free(struct list *head);

// NOTE: set
static u32 murmur3_32(const char *key, u64 len);
static struct hashset set_init(u32 width);
static void set_free(struct hashset *set);
static bool set_add(struct hashset *set, char *item);
static struct list *set_seek(struct hashset *set, u32 key);
static bool set_del(struct hashset *set, u32 key);
static bool set_has(struct hashset *set, void *item);

// NOTE: optimizations
//static bool operand_invariant(struct ir *file, struct lim_data *loop, u32 operand);
//static s32 invariant_or_locate(struct ir *file. struct lim_data *loop, u32 object_id);
static void optimize_lim(struct uint_vector *bfs_order, struct ir *file);

// NOTE: this must be 4 byte-aligned, is written to as a pointer
struct spirv_header {
    u32 magic_number;
    u32 version;
    u32 generator_mn;
    u32 bound;
    u32 zero;
};

struct instruction {
    u32 wordcount;
    u32 opcode;
    u32 words_from;
};

struct basic_block {
    u32 id;
    u32 from;
    u32 to; 
};

struct parse_result {
    struct instruction *instructions;
    u32 *bb_labels;
};

struct ir {
    struct uint_vector constants;
    struct basic_block *blocks;
    struct instruction *instructions;
    u32 *words;
};

struct lim_data {
    struct uint_vector invariants;
    struct uint_vector result_ids;
    struct uint_vector operand_count;
    struct uint_vector operands;
    struct uint_vector opcodes;
};

#include "names.c"
#include "cfg.c"
#include "utils.c"
#include "spirv.c"
#include "optimize.c"