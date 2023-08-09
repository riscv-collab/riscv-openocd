// This generator was taken from
// http://gitlab.dev.syntacore.com/verification-team/priveleged_isa/-/tree/development/submodule_tools/mmu_gen
// Authored by Mikhail Modin (@mikhail.modin)
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//
#define RISCV_PGSHIFT (12)
#define PTE_PPN_SHIFT (10)
#define PTE_V (1 << 0) /*Valid*/
#define PTE_R (1 << 1) /*Read*/
#define PTE_W (1 << 2) /*Write*/
#define PTE_X (1 << 3) /*Execute*/
#define PTE_U (1 << 4) /*User*/
#define PTE_G (1 << 5) /*Global*/
#define PTE_A (1 << 6) /*Accessed*/
#define PTE_D (1 << 7) /*Dirty*/
//
struct vms_settings {
    unsigned int levels;
    bool mem_x4;
};
struct pte_description {
    uint64_t pa;
    uint64_t vpn[5];
    uint64_t attr;
    uint64_t page_size;
};
//
int parse_config_file(void *config_file_ptr);
void calc_vpn(void);
void process_vpn(struct pte_description *pte_desc_table_ptr, int pte_num, int level, uint64_t previous_level_pte[512]);
int create_header_file(void *file_ptr, void *section_name);
//
struct vms_settings vms_settings_const[7] = {
    {
        /*Sv39*/
        .levels = 3,
        .mem_x4 = false,
    },
    {
        /*Sv48*/
        .levels = 4,
        .mem_x4 = false,
    },
    {
        /*Sv57*/
        .levels = 5,
        .mem_x4 = false,
    },
    {
        /*Sv39x4*/
        .levels = 3,
        .mem_x4 = true,
    },
    {
        /*Sv48x4*/
        .levels = 4,
        .mem_x4 = true,
    },
    {
        /*Sv57x4*/
        .levels = 5,
        .mem_x4 = true,
    },
};
struct pte_description description_table[512];
struct vms_settings *using_mem_sch;
uint64_t pt[512][512] = {0};
uint64_t root_table[2048] = {0};
int glob_counter, pt_counter = 0;
uint64_t mem_base;
//
int main(int argc, char *argv[]) {
    int retval;
    printf("mmu_gen built: %s\n", __DATE__);
#ifdef DEBUG_PRINTF
    for (int i = 0; i < argc; i++) {
        printf("%d: %s\n", i, argv[i]);
    }
#else
    (void)argc;
#endif
    if (argv[1] == NULL) {
        printf("config file not defined\n");
        return __LINE__;
    }
    retval = parse_config_file(argv[1]);
    if (retval != 0) {
        return retval;
    }
    calc_vpn();
    process_vpn(description_table, glob_counter, using_mem_sch->levels - 1, root_table);
    retval = create_header_file(argv[2], argv[3]);
    if (retval != 0) {
        return retval;
    }
    return 0;
}
int parse_config_file(void *config_file_ptr) {
    FILE *config_file;
    char line[1024];
    glob_counter = 0;
    config_file = fopen(config_file_ptr, "r");
    if (config_file == NULL) {
        printf("can't open config file\n");
        return __LINE__;
    }
    fgets(line, sizeof(line), config_file);
    if (strcmp(line, "sv39\n") == 0) {
        using_mem_sch = &vms_settings_const[0];
    } else if (strcmp(line, "sv48\n") == 0) {
        using_mem_sch = &vms_settings_const[1];
    } else if (strcmp(line, "sv57\n") == 0) {
        using_mem_sch = &vms_settings_const[2];
    } else if (strcmp(line, "sv39x4\n") == 0) {
        using_mem_sch = &vms_settings_const[3];
    } else if (strcmp(line, "sv48x4\n") == 0) {
        using_mem_sch = &vms_settings_const[4];
    } else if (strcmp(line, "sv57x4\n") == 0) {
        using_mem_sch = &vms_settings_const[5];
    } else {
        printf("unknown memory scheme\n");
        fclose(config_file);
        return __LINE__;
    }
    printf("mem scheme is %s", line);
    fscanf(config_file, "%" PRIx64 "\n", &mem_base);
#ifdef DEBUG_PRINTF
    printf("mem base %" PRIx64 "\n", mem_base);
#endif
    while (feof(config_file) != true) {
        if (fscanf(config_file, "%" PRIx64 ",%" PRIx64 ",%" PRIx64 ",%" PRIx64"\n",
                &description_table[glob_counter].vpn[0], &description_table[glob_counter].pa,
                &description_table[glob_counter].page_size,
                &description_table[glob_counter].attr) != 4) {
            break;
        }
        glob_counter++;
        if (glob_counter > 511) {
            printf("too much pages\n");
            fclose(config_file);
            return __LINE__;
        }
    }
    fclose(config_file);
    return 0;
}
void calc_vpn(void) {
    uint64_t start_va, start_pa, mask, va;
    for (int num = 0; num < glob_counter; num++) {
        start_va = description_table[num].vpn[0];
        start_pa = description_table[num].pa;
        if (using_mem_sch->levels <= description_table[num].page_size) {
            printf("skipped - super page is too big for choosen memory scheme\n");
            memset(&description_table[num], 0xff, sizeof(struct pte_description));
            continue;
        }
        mask = 0xfff;
        for (size_t i = 0; i < description_table[num].page_size; i++) {
            mask |= 0x1ffull << ((i * 9)+12);
        }
        if (start_va & mask) {
            printf("fixed - va %" PRIx64 " was misailgned\n", start_va);
            start_va = start_va & ~mask;
        }
        if (start_pa & mask) {
            printf("fixed - pa %" PRIx64 " was misailgned\n", start_pa);
            start_pa = start_pa & ~mask;
        }
        va = start_va >> (12+9*description_table[num].page_size);
        description_table[num].pa = start_pa >> 12;
        for (size_t i = 0; i < description_table[num].page_size; i++) {
            description_table[num].vpn[i] = 0xffff;
        }
        for (size_t i = description_table[num].page_size; i < (using_mem_sch->levels - 1); i++) {
            description_table[num].vpn[i] = va & 0x1ff;
            va = va >> 9;
        }
        if (using_mem_sch->mem_x4) {
            description_table[num].vpn[using_mem_sch->levels - 1] = va & 0x7ff;
            va = va >> 11;
        }
        else
        {
            description_table[num].vpn[using_mem_sch->levels - 1] = va & 0x1ff;
            va = va >> 9;
        }
        if (va != 0) {
            printf("skipped - va more than space\n");
            memset(&description_table[num], 0xff, sizeof(struct pte_description));
            continue;
        }
#ifdef DEBUG_PRINTF
        printf("va 0x%016" PRIx64 " -> pa 0x%016" PRIx64 "\n", start_va, start_pa);
        for (unsigned int i = 0; i < using_mem_sch->levels; i++) {
            printf("\tvpn[%d] %d\n", i, (int)description_table[num].vpn[i]);
        }
#endif
    }
}
void process_vpn(struct pte_description *pte_desc_table_ptr, int pte_num, int level,
                 uint64_t previous_level_pte[512]) {
    int check[2048] = {0};
    struct pte_description pte_temp[2048];
    int counter_temp;
    uint64_t temp_loc;
    uint64_t superpage;
#ifdef DEBUG_PRINTF
    printf("[%d] in %d pte\n", level, pte_num);
#endif
    if (level > 0) {
        for (int i = 0; i < pte_num; i++) {
            if (pte_desc_table_ptr[i].vpn[level] < 2048) {
                check[pte_desc_table_ptr[i].vpn[level]]++;
            }
        }
        for (int i = 0; i < 2048; i++) {
            if (check[i] != 0) {
                previous_level_pte[i] = (uint64_t)pt[pt_counter];
                temp_loc = (uint64_t)pt[pt_counter];
                pt_counter++;
#ifdef DEBUG_PRINTF
                printf("[%d] %d pte for %d next level\n", level, check[i], i);
#endif
                counter_temp = 0;
                superpage = 0;
                for (int j = 0; j < pte_num; j++) {
                    if ((int)pte_desc_table_ptr[j].vpn[level] == i) {
                        if ((int)pte_desc_table_ptr[j].page_size == level) {
                            superpage = j;
                            break;
                        } else {
                            memcpy(&pte_temp[counter_temp], &pte_desc_table_ptr[j],
                                   sizeof(struct pte_description));
                            counter_temp++;
                        }
                    }
                }
                if (superpage == 0) {
                    process_vpn(pte_temp, check[i], level - 1, (uint64_t *)previous_level_pte[i]);
                    previous_level_pte[i] = (mem_base + 2048*8 + temp_loc - (uint64_t)pt) >> RISCV_PGSHIFT;
                    previous_level_pte[i] = (previous_level_pte[i] << PTE_PPN_SHIFT) | PTE_V | PTE_G;
                } else {
                    previous_level_pte[i] = (pte_desc_table_ptr[superpage].pa << PTE_PPN_SHIFT) | pte_desc_table_ptr[superpage].attr;
#ifdef DEBUG_PRINTF
                    printf("[%d] pt[][%d]->pa 0x%" PRIx64 "\n", level, i, pte_desc_table_ptr[superpage].pa);
#endif
                }
            }
        }
    } else {
        for (int i = 0; i < pte_num; i++) {
            previous_level_pte[pte_desc_table_ptr[i].vpn[0]] =
                (pte_desc_table_ptr[i].pa << PTE_PPN_SHIFT) | pte_desc_table_ptr[i].attr;
#ifdef DEBUG_PRINTF
            printf("[0] pt[][%d]->pa 0x%" PRIx64 "\n", (int)pte_desc_table_ptr[i].vpn[0],
                pte_desc_table_ptr[i].pa);
#endif
        }
    }
}
int create_header_file(void *file_ptr, void *section_name) {
    FILE *header_file;
    if (file_ptr == NULL) {
        printf("no output file specifed\n");
        header_file = fopen("./pte.h", "w+");
        section_name = NULL;
    } else {
        header_file = fopen(file_ptr, "w+");
    }
    if (header_file == NULL) {
        printf("can't create header file\n");
        return __LINE__;
    }
    fprintf(header_file, "volatile uint64_t page_table[%d]", 2048 + 512 * pt_counter);
    fprintf(header_file, " __attribute__((aligned(0x4000)");
    if (section_name == NULL) {
        printf("no section specifed\n");
    } else {
        fprintf(header_file, ", section(\".%s\")", (char *)section_name);
    }
    fprintf(header_file, ")) = {\n");
    for (int j = 0; j < 2048; j++) {
        if (root_table[j] != 0) {
            fprintf(header_file, "\t[%d] = 0x%" PRIx64 ", /* 0x%" PRIx64 "*/\n", j,
                    root_table[j], (root_table[j] >> 10) << 12);
#ifdef DEBUG_PRINTF
            printf("[%d] 0x%" PRIx64 "\n", j, root_table[j]);
#endif
        }
    }
    for (int i = 0; i < pt_counter; i++) {
        for (int j = 0; j < 512; j++) {
            if (pt[i][j] != 0) {
                fprintf(header_file, "\t[%d] = 0x%" PRIx64 ", /* 0x%" PRIx64 "*/\n", 2048 + i*512 + j,
                        pt[i][j], (pt[i][j] >> 10) << 12);
#ifdef DEBUG_PRINTF
                printf("[%d][%d] 0x%" PRIx64 "\n", i, j, pt[i][j]);
#endif
            }
        }
    }
    fprintf(header_file, "};\n");
    fclose(header_file);
    return 0;
}
