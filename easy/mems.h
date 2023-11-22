#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

#define PAGE_SIZE 4096

typedef struct SubChainNode {
    size_t size;
    int type;
    void* v_ptr;
    struct SubChainNode* next;
    struct SubChainNode* prev;
} SubChainNode;

typedef struct MainChainNode {
    size_t psize;
    size_t used;
    SubChainNode* sub_chain;
    struct MainChainNode* next;
    struct MainChainNode* prev;
} MainChainNode;

MainChainNode* main_chain_head = NULL;
void* v_ptr = (void*)1000;

MainChainNode* addToMainChain(size_t size) {
    size_t main_node_size = size;
    size_t allocation_size = (main_node_size + PAGE_SIZE - 1) / PAGE_SIZE * PAGE_SIZE;

    MainChainNode* new_main_node = (MainChainNode*)mmap(NULL, allocation_size,
                                        PROT_READ | PROT_WRITE,
                                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (new_main_node == MAP_FAILED) {
        perror("mmap failed");
        exit(1);
    }

    new_main_node->psize = allocation_size;
    new_main_node->used = 0;
    new_main_node->sub_chain = NULL;
    new_main_node->next = main_chain_head;
    new_main_node->prev = NULL;

    if (main_chain_head) {
        main_chain_head->prev = new_main_node;
    }

    main_chain_head = new_main_node;
    return new_main_node;
}

SubChainNode* addToSubChain(MainChainNode* main_node, size_t size, int type) {
    SubChainNode* new_sub_node = (SubChainNode*)mmap(NULL, sizeof(SubChainNode),
                                        PROT_READ | PROT_WRITE,
                                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (new_sub_node == MAP_FAILED) {
        perror("mmap failed");
        exit(1);
    }

    new_sub_node->size = size;
    new_sub_node->type = type;
    new_sub_node->v_ptr = v_ptr;
    new_sub_node->next = NULL;
    new_sub_node->prev = NULL;

    if (main_node->sub_chain) {
        new_sub_node->next = main_node->sub_chain;
        main_node->sub_chain->prev = new_sub_node;
    }

    main_node->sub_chain = new_sub_node;
    
    v_ptr = v_ptr + size;

    return new_sub_node;
}

void mems_init() {
    MainChainNode* main_chain_head = NULL;
    v_ptr = (void*)1000;
}

void mems_finish() {
    MainChainNode* main_node = main_chain_head;
    MainChainNode* next_main_node;

    while (main_node) {
        next_main_node = main_node->next;
        munmap(main_node, main_node->psize);
        main_node = next_main_node;
    }

    main_chain_head = NULL;
}

void* mems_malloc(size_t size) {
    MainChainNode* main_node = main_chain_head;

    if (main_chain_head == NULL || main_chain_head->used + size > main_chain_head->psize) {
        main_node = addToMainChain(size);
    }

    while (main_node) {
        if ((main_node->psize - main_node->used) >= size) {
            return addToSubChain(main_node, size, 1)->v_ptr;
        }

        if (main_node->next == NULL || main_node->next->used + size > main_node->next->psize) {
            main_node->next = addToMainChain(size);
        }

        main_node = main_node->next;
    }

    return NULL;
}

void mems_print_stats() {
    MainChainNode* main_node = main_chain_head;
    int totalPagesUsed = 0;
    int totalUnusedMemory = 0;
    int main_count = 0;

    while (main_node) {
        totalPagesUsed += main_node->psize / PAGE_SIZE;
        totalUnusedMemory += main_node->psize - main_node->used;

        printf("Main Chain Node: psize=%zu, used=%zu\n",
            main_node->psize, main_node->used);

        SubChainNode* sub_node = main_node->sub_chain;
        int segmentCount = 0;

        while (sub_node) {
            printf("  Segment %d: size=%zu, type=%d, v_ptr=%p\n",
                segmentCount, sub_node->size, sub_node->type, sub_node->v_ptr);
            sub_node = sub_node->next;
            segmentCount++;
        }

        printf("Length of subchain %d\n", segmentCount);
        main_node = main_node->next;
        main_count++;
    }

    printf("Total pages utilized: %d\n", totalPagesUsed);
    printf("Total unused memory: %d\n", totalUnusedMemory);
    printf("Length of mainchain %d\n", main_count);
}

void* mems_get(void* v_ptr) {
    MainChainNode* main_node = main_chain_head;

    while (main_node) {
        SubChainNode* sub_node = main_node->sub_chain;

        while (sub_node) {
            if (sub_node->v_ptr == v_ptr) {
                size_t offset = (size_t)(v_ptr - sub_node->v_ptr);
                return (void*)((char*)main_node + offset);
            }

            sub_node = sub_node->next;
        }

        main_node = main_node->next;
    }

    return NULL;
}

void combineFreeSubNodes(MainChainNode* main_node) {
    SubChainNode* current_sub_node = main_node->sub_chain;

    while (current_sub_node && current_sub_node->next) {
        if (current_sub_node->type == 0 && current_sub_node->next->type == 0) {
            current_sub_node->size += current_sub_node->next->size;
            current_sub_node->next = current_sub_node->next->next;

            if (current_sub_node->next) {
                current_sub_node->next->prev = current_sub_node;
            }
        } else {
            current_sub_node = current_sub_node->next;
        }
    }
}

void mems_free(void* v_ptr) {
    MainChainNode* main_node = main_chain_head;

    while (main_node) {
        SubChainNode* sub_node = main_node->sub_chain;

        while (sub_node) {
            if (sub_node->v_ptr == v_ptr) {
                sub_node->type = 0;
                main_node->used -= sub_node->size;
                combineFreeSubNodes(main_node);
                return;
            }

            sub_node = sub_node->next;
        }

        main_node = main_node->next;
    }
}

