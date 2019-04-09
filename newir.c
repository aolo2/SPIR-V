#include "common.h"

#include "newinst.c"
#include "newcfg.c"
#include "newopt.c"

struct ir_header {
    u32 magic_number;
    u32 version;
    u32 generator_mn;
    u32 bound;
    u32 zero;
};

struct instruction_list {
    struct instruction_t data;
    struct instruction_list *next;
    struct instruction_list *prev;
};

struct basic_block {
    u32 count;
    struct instruction_list *instructions;
};

struct ir {
    struct ir_header header;
    struct ir_cfg cfg;
    struct basic_block *blocks;
    struct instruction_list *pre_cfg;
    struct instruction_list *post_cfg;
};

static u32 *
get_binary(const char *filename, u32 *size)
{
    FILE *file = fopen(filename, "rb");
    
    if (!file) {
        fprintf(stderr, "[ERROR] File could not be opened\n");
        return(NULL);
    }
    
    fseek(file, 0L, SEEK_END);
    *size = ftell(file);
    rewind(file);
    
    // NOTE: size % sizeof(u32) is always zero
    u32 *buffer = (u32 *) malloc(*size);
    fread((u8 *) buffer, *size, 1, file);
    
    *size /= sizeof(u32);
    
    fclose(file);
    
    return(buffer);
}


static struct ir
eat_ir(u32 *data, u32 size)
{
    u32 labels[100] = { 0 };
    u32 bb_count = 0;
    
    struct ir file;
    file.header = *((struct ir_header *) data);
    
    struct instruction_list *all_instructions;
    struct instruction_list *inst;
    struct instruction_list *last = NULL;
    
    u32 offset = sizeof(struct ir_header) / 4;
    u32 *word = data + offset;
    
    // NOTE: get all instructions in a list, count basic blocks
    do {
        inst = malloc(sizeof(struct instruction_list));
        inst->data = get_instruction(word);
        inst->prev = last;
        
        if (inst->data.opcode == OpLabel) {
            labels[bb_count++] = inst->data.OpLabel.result_id;
        }
        
        offset += inst->data.wordcount;
        
        if (last) {
            last->next = inst;
        } else {
            all_instructions = inst;
        }
        
        last = inst;
        word = data + offset;
    } while (offset != size);
    
    last->next = NULL;
    // =======================
    
    
    file.blocks = malloc(sizeof(struct basic_block) * bb_count);
    
    
    // NOTE: all pre-cfg instructions
    file.pre_cfg = all_instructions;
    inst = all_instructions;
    do {
        inst = inst->next;
    } while (inst->data.opcode != OpLabel);
    
    inst->prev->next = NULL;
    inst->prev = NULL;
    // =======================
    
    
    // NOTE: reconstruct the cfg
    file.cfg = cfg_init(labels, bb_count);
    
    struct basic_block block;
    u32 block_number = 0;
    
    while (inst->data.opcode == OpLabel) {
        block.count = 0;
        struct instruction_list *start = inst->next;
        inst = inst->next; // NOTE: skip OpLabel
        
        while (!terminal(inst->data.opcode)) {
            inst = inst->next;
            ++block.count;
        }
        
        block.instructions = (block.count > 0 ? start : NULL);
        
        if (inst->data.opcode == OpBranch) {
            u32 edge_id = inst->data.OpBranch.target_label;
            u32 edge_index = search_item_u32(labels, bb_count, edge_id);
            cfg_add_edge(&file.cfg, block_number, edge_index);
        } else if (inst->data.opcode == OpBranchConditional) {
            file.cfg.conditions[block_number] = inst->data.OpBranchConditional.condition;
            u32 true_edge = search_item_u32(labels, bb_count, inst->data.OpBranchConditional.true_label);
            u32 false_edge = search_item_u32(labels, bb_count, inst->data.OpBranchConditional.false_label);
            cfg_add_edge(&file.cfg, block_number, true_edge);
            cfg_add_edge(&file.cfg, block_number, false_edge);
        }
        
        struct instruction_list *save = inst;
        inst->prev->next = NULL;
        inst->next->prev = NULL;
        inst = inst->next;
        
        free(save);
        
        file.blocks[block_number++] = block;
    }
    
    file.cfg.dominators = cfg_dominators(&file.cfg);
    
    // =======================
    
    
    // NOTE: all post-cfg instructions
    file.post_cfg = inst;
    // =======================
    
    return(file);
}

static void
dump_ir(struct ir *file, const char *filename)
{
    FILE *stream = fopen(filename, "wb");
    u32 buffer[100];
    
    if (!file) {
        fprintf(stderr, "[ERROR] Can not write output\n");
        exit(1);
    }
    
    fwrite(&file->header, sizeof(struct ir_header), 1, stream);
    
    struct instruction_list *inst = file->pre_cfg;
    u32 *words;
    
    do {
        words = dump_instruction(&inst->data, buffer);
        fwrite(words, inst->data.wordcount * 4, 1, stream);
        inst = inst->next;
    } while (inst);
    
    for (u32 i = 0; i < file->cfg.labels.size; ++i) {
        if (file->cfg.labels.data[i] == 0) {
            continue;
        }
        
        struct basic_block block = file->blocks[i];
        struct oplabel_t label_operand = {
            .result_id = file->cfg.labels.data[i]
        };
        
        struct instruction_t label_inst = {
            .opcode = OpLabel,
            .wordcount = 2,
            .OpLabel = label_operand
        };
        
        words = dump_instruction(&label_inst, buffer);
        fwrite(words, label_inst.wordcount * 4, 1, stream);
        
        inst = block.instructions;
        while (inst) {
            words = dump_instruction(&inst->data, buffer);
            fwrite(words, inst->data.wordcount * 4, 1, stream);
            inst = inst->next;
        };
        
        struct instruction_t termination_inst;
        struct edge_list *edge = file->cfg.out[i];
        u32 edge_count = 0;
        
        while (edge) {
            edge = edge->next;
            ++edge_count;
        }
        
        if (edge_count == 0) {
            termination_inst.opcode = OpReturn;
            termination_inst.wordcount = 1;
        } else if (edge_count == 1) {
            termination_inst.opcode = OpBranch;
            termination_inst.wordcount = 2;
            termination_inst.OpBranch.target_label = file->cfg.labels.data[file->cfg.out[i]->data];
        } else if (edge_count == 2) {
            termination_inst.opcode = OpBranchConditional;
            termination_inst.wordcount = 4;
            termination_inst.OpBranchConditional.condition = file->cfg.conditions[i];
            termination_inst.OpBranchConditional.true_label =
                file->cfg.labels.data[file->cfg.out[i]->data];
            termination_inst.OpBranchConditional.false_label =
                file->cfg.labels.data[file->cfg.out[i]->next->data];
        } else {
            ASSERT(false);
        }
        
        words = dump_instruction(&termination_inst, buffer);
        fwrite(buffer, termination_inst.wordcount * 4, 1, stream);
    }
    
    inst = file->post_cfg;
    do {
        words = dump_instruction(&inst->data, buffer);
        fwrite(words, inst->data.wordcount * 4, 1, stream);
        inst = inst->next;
    } while (inst);
    
    fclose(stream);
}

// TODO: return status?
static void
delete_instruction(/*struct basic_block *block, */struct instruction_list **inst)
{
    struct instruction_list *vinst = *inst;
    
    if (vinst->prev) {
        vinst->prev->next = vinst->next;
    }
    
    if (vinst->next) {
        vinst->next->prev = vinst->prev;
    }
    
    *inst = NULL;
    free(vinst);
}

// TODO: return status?
static void
append_instruction(struct basic_block *block, struct instruction_t instruction)
{
    struct instruction_list *last = block->instructions;
    
    if (!last) {
        last = malloc(sizeof(struct instruction_list));
        last->data = instruction;
        last->prev = NULL;
        last->next = NULL;
        block->instructions = last;
    } else {
        do {
            last = last->next;
        } while (last->next);
        
        last->next = malloc(sizeof(struct instruction_list));
        last->next->data = instruction;
        last->next->prev = last;
        last->next->next = NULL;
    }
}

static void
instruction_list_free(struct instruction_list *list)
{
    struct instruction_list *next;
    
    while (list) {
        next = list->next;
        free(list->data.unparsed_words);
        free(list);
        list = next;
    }
}

static void
destroy_ir(struct ir *file)
{
    for (u32 i = 0; i < file->cfg.labels.size; ++i) {
        instruction_list_free(file->blocks[i].instructions);
    }
    
    free(file->blocks);
    cfg_free(&file->cfg);
    instruction_list_free(file->pre_cfg);
    instruction_list_free(file->post_cfg);
}

static struct basic_block *
add_basic_block(struct ir *file)
{
    u32 label = file->header.bound;
    file->header.bound++;
    
    cfg_add_vertex(&file->cfg, label);
    
    struct basic_block new_block = {
        .count = 0,
        .instructions = NULL
    };
    
    file->blocks = realloc(file->blocks, file->cfg.labels.size * sizeof(struct basic_block));
    file->blocks[file->cfg.labels.size - 1] = new_block;
    
    return(file->blocks + file->cfg.labels.size - 1);
}

s32
main()
{
    const char *filename = "W:/vlk/data/cycle.frag.spv";
    
    u32 num_words;
    u32 *words = get_binary(filename, &num_words);
    
    struct ir file = eat_ir(words, num_words);
    free(words);
    
    dump_ir(&file, "W:/vlk/data/cycle2.frag.spv");
    destroy_ir(&file);
}