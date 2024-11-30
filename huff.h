#ifndef HUFF_H
#define HUFF_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <arpa/inet.h>

#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte)  \
  ((byte) & 0x80 ? '1' : '0'), \
  ((byte) & 0x40 ? '1' : '0'), \
  ((byte) & 0x20 ? '1' : '0'), \
  ((byte) & 0x10 ? '1' : '0'), \
  ((byte) & 0x08 ? '1' : '0'), \
  ((byte) & 0x04 ? '1' : '0'), \
  ((byte) & 0x02 ? '1' : '0'), \
  ((byte) & 0x01 ? '1' : '0') 

#define BIT_MASK(bit)             (1 << (bit))
#define SET_BIT(value,bit)        ((value) |= BIT_MASK(bit))
#define CLEAR_BIT(value,bit)      ((value) &= ~BIT_MASK(bit))
#define TEST_BIT(value,bit)       (((value) & BIT_MASK(bit)) ? 1 : 0)

#define SET_ERROR(mes) do { strcpy(h->error_msg, (mes)); \
                         h->error = 1; } while (0)
#define EXIT_ON_ERROR { if (h->error) {return;} } 
#define EXIT_ON_ERROR { if (h->error) {return;} } 
#define GOTO_ERROR do { if (h->error) { \
                       exit_status = EXIT_FAILURE; \
                       goto Error; } } while (0)

#define NO_CHILD 0
#define HAS_CHILD 1

typedef struct node {
    size_t id;
    unsigned char value;
    size_t weight;
    struct node* left;
    struct node* right;
    struct node* up;
    struct node* next;
} Node;

typedef struct {
    Node* head;
    Node* first;
    size_t nodes_count;
    uint32_t message_size;
    FILE* input_file;
    FILE* output_file;
    char error;
    char error_msg[100];
    int quiet;
    int export_tree;
    unsigned char io_buffer;
    int buff_pos;
    unsigned char mode;
} Huff;

typedef struct {
    char identifier[4];
    uint32_t message_size;
} Header;

typedef struct {
    char child_status;
    const unsigned char value;
} Element;

Huff* init(void);
void cleanup(Huff*);
void compress(Huff*);
void decompress(Huff*);
void open_input(Huff*, const char*);
void open_output(Huff*, const char*);

#endif
