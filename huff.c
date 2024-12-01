#include "huff.h"

/*! Create node
 *
 * Allocate mem for new node and propagate it with default values
 */
static Node* new_node(Huff* h, Node* left, Node* right)
{
    Node* new = malloc(sizeof(Node));
    if (!new)
    {
        SET_ERROR("Memory allocation failed");
        return NULL;
    }

    new->value = '&';
    new->weight = 0;
    new->left = left;
    new->right = right;
    new->up = NULL;
    new->next = h->first;
    h->first = new;
    new->id = h->nodes_count++;

    if (left && right)
    {
        new->weight = left->weight + right->weight;
        left->up = new;
        right->up = new;
    } 

    return new;
}

/*! Build binaty tree
 *
 * Merge wto nodes with lowest weight.
 * Repeat recursivelly until all nodes are part of tree. 
 */
static void merge_nodes(Huff* h)
{
    // find node with lowest weight
    Node* min_a = NULL;
    for (Node* n = h->first; n != NULL; n = n->next)
    {
        if (n->up) continue; // this node is already attached to tree

        if (min_a == NULL)
        {
            min_a = n;
            continue;
        }

        if (min_a->weight > n->weight)
            min_a = n;
    }

    if (!min_a) return;  // no nore unattached nodes found

    // find second node with lowest weight
    Node* min_b = NULL;
    for (Node* n = h->first; n != NULL; n = n->next)
    {
        if (n->up) continue; // this node is already attached to tree

        if (n == min_a) continue;

        if (min_b == NULL)
        {
            min_b = n;
            continue;
        }

        if (min_b->weight > n->weight)
            min_b = n;
    }

    if (min_b == NULL)
    {
        h->head = min_a;   // only one uncoded node left, make it head of the tree and finish.
        return;
    }

    if (min_a->weight == 133 || min_b->weight == 133) printf("hi there\n");
    new_node(h, min_a, min_b);
    EXIT_ON_ERROR;

    merge_nodes(h); // go recursivelly
}

/*! Find bottom node with byte
 *
 * Search all nodes until match with value.
 * Node needs to be leaf (no childen).
 */
static Node* find_node(const Huff* h, uint8_t a)
{
    for (Node* n = h->first; n != NULL; n = n->next)
        if (n->value == a && n->left == NULL) return n;

    return NULL;
}

/*! Create all nodes
 *
 * Scan input file and create all nodes for each new byte value in file.
 * If node with value exists, increase node weight
 */
static void  initialize_nodes(Huff* h)
{
    uint8_t byte;
    while (fread(&byte, sizeof(byte), 1, h->input_file) != 0)
    {
        h->message_size++; // [FIXME] this is uint32_t? Anything to worry?
        Node* n = find_node(h, byte);
        if (!n)
        {   
            // it's byte without node yet, create new
            n = new_node(h, NULL, NULL);
            if (!n) return;  // exit with problem on node allocation

            n->value = byte;
        }
        else
            // node with this byte exists, increase its weight
            n->weight++;
    }
    merge_nodes(h);
}

/*! Serialize tree
 *
 * Starting from head, traverse all nodes in tree
 * Recursive funcition
 */
static void serialize(Huff* h, Node* n)
{
    if (n == NULL) return;

    Element e = {
        .child_status = NO_CHILD,
        .value = n->value,
    };

    if (n->left)
        e.child_status = HAS_CHILD;

    if (fwrite(&e, sizeof(e), 1, h->output_file) == 0)
    {        
        SET_ERROR("Can't write header nodes to file.");
        return;
    }

    serialize(h, n->left);
    serialize(h, n->right);
}

/*! Write IO buffer to disk
 *
 * Help function, writes IO buffer to disk
 */
static void write_io_buffer(Huff* h)
{
    if (fwrite(&h->io_buffer, sizeof(h->io_buffer), 1, h->output_file) == 0)
    {        
        SET_ERROR("Can't write io_buffer to file.");
        return;
    }
    h->io_buffer = 0;
}

/*! Write single bit to IO buffer
 *
 * works in MSB. If buffer is full, it flushes it to disk.
 */
static void write_bit(Huff* h, uint8_t b)
{
    if (h->buff_pos > 7)
    {
        h->buff_pos = 0;
        write_io_buffer(h);
    }

    if (b)
        SET_BIT(h->io_buffer, 7 - h->buff_pos);
    else
        CLEAR_BIT(h->io_buffer, 7 - h->buff_pos);

    ++h->buff_pos;
}

/*! Write byte to IO buffer
 *
 * starts from MSB
 */
static void write_byte(Huff* h, uint8_t v)
{
    for (int i = 7; i >= 0; --i)
        write_bit(h, TEST_BIT(v, i));
}
/*! Serialize Huffman tree - compact version
 *
 * Uses single bit to indicate if this is node or leaf
 */
static void serialize2(Huff* h, Node* n)
{
    if (n == NULL) return;

    if (n->left)  // it's node
    {
    //    printf(" 1");
        write_bit(h, HAS_CHILD);
        serialize2(h, n->left);
        serialize2(h, n->right);
    }
    else         // it's leaf
    {
    //    printf(" 0 "BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(n->value));
        write_bit(h, NO_CHILD);
        write_byte(h, n->value);
    }
}

/*! Create and write output file header
 *
 */
static void write_header(Huff* h)
{
    Header header = {
        .identifier = "HUFx",
        .message_size = htonl(h->message_size),
    };

    header.identifier[3] = h->mode;

    size_t result = fwrite(&header, sizeof(header), 1, h->output_file);
    if (result != 1)  // not all objects written
        SET_ERROR("Can't write header to file.");
}

/*! Deserialize tree
 *
 * Read elements from file and build tree structure
 * Recursive funcition
 */
static Node* deserialize_tree(Huff* h)
{
    Element e;

    if (fread(&e, sizeof(e), 1, h->input_file) == 0)
    {
        SET_ERROR("Can't read nodes from file.");
        return NULL;
    }

    Node* n = new_node(h, NULL, NULL);
    if (!n)
        return NULL;   // error handling

    n->value = e.value;

    if (h->head == NULL)
        h->head = n;

    if (e.child_status == HAS_CHILD)
    {
        n->left = deserialize_tree(h);
        n->right = deserialize_tree(h);

        if (n->left == NULL || n->right == NULL)  // error handling
            return NULL;
    }
    return n;
}

/*! Read bit from IO buffer
 *
 * Reads MSB, if buffer ends, loads next block from file
 */
int read_bit(Huff* h)
{
    --h->buff_pos;
    if (h->buff_pos < 0)
    {
        if (fread(&h->io_buffer, sizeof(h->io_buffer), 1, h->input_file) == 0)
        {
            SET_ERROR("Can't read nodes from file.");
            return -1;
        }
        h->buff_pos = 7;
    }
    
    return TEST_BIT(h->io_buffer, h->buff_pos);
}

/*! Read byte from io_buffer
 *
 * reads bit by bit, from MSB
 */
uint8_t read_byte(Huff* h)
{
    uint8_t b = 0;
    for (int i = 7; i >= 0; --i)
        if (read_bit(h))
            SET_BIT(b, i);

    return b;
}

/*! Deserialize tree version 2
 *
 * Read elements from file and build tree structure
 * Recursive funcition
 */
static Node* deserialize2(Huff* h)
{
    int b = read_bit(h);

    Node* n = new_node(h, NULL, NULL);

    if (!n)
        return NULL;   // error handling

    if (h->head == NULL)
        h->head = n;

    if (b == HAS_CHILD)  // it's node
    {
        n->left = deserialize2(h);
        n->right = deserialize2(h);
    }
    else    // it's leaf
        n->value = read_byte(h);

    return n;
}

/*! Read file header
 *
 * Reads header with static information
 */
static void read_header(Huff* h)
{
    int result = 0;
    Header header = { 0 };

    result = fread(&header, sizeof(header), 1, h->input_file);
    if (!result)
    {
        SET_ERROR("Can't read header from file.");
        return;
    }

    if (strncmp("HUF", header.identifier, 3) != 0)
    {
        SET_ERROR("Wrong file format.");
        return;
    }

    h->mode = header.identifier[3];  // mode 1, 2 ...
    h->message_size = ntohl(header.message_size);
}

/*! Print graph
 *
 * Exports Huffman tree in DOT format
 * can be used as input to Graphviz
 * https://dreampuf.github.io/GraphvizOnline
 */
void print_tree(Huff* h, Node* n)
{
    if (n == NULL) return;

    if (n == h->head)
    {
        // file header
        printf("digraph G {\n");
        printf("Node [label=\"\" shape=point]\n");
    }

    if (n->left)  // it's node
    {
        printf("Node_%zu -> ", n->id);
        print_tree(h, n->left);
        printf("Node_%zu -> ", n->id);
        print_tree(h, n->right);
    }
    else         // it's leaf
        if (n->value > 32 && n->value < 127 && n->value != '"') // printable
            printf("Node_%zu\nNode_%zu [label=\"%c $%02X\\n%zu\" shape=box]\n", n->id, n->id, n->value, n->value, n->weight);
        else
            printf("Node_%zu\nNode_%zu [label=\"%02X\\n%zu\" shape=box]\n", n->id, n->id, n->value, n->weight);

    if (n == h->head)
    {
        // file footer
        printf("Node_%zu [shape=doublecircle label=Root]\n", n->id);
        printf("}\n");
    }
}

/*! Initialize main structure
 *
 * Should be called as first fucntion
 */
Huff* init(void)
{
    Huff* h = malloc(sizeof(*h));
    if (!h)
    {
        SET_ERROR("Can't allocate main structure.");
        return NULL;
    }

    h->head = NULL;
    h->first = NULL;
    h->message_size = 0;
    h->nodes_count = 0;
    h->error = 0;
    h->quiet = 0;
    h->export_tree = 0;
    h->buff_pos = 0;
    h->mode = '1';

    return h;
}

/*! Close files and deallocate all memory
 *
 * Should be called at very end
 */
void cleanup(Huff* h)
{    
    while (h->first)
    {
        Node* next_free = h->first->next;
        free(h->first);
        h->first = next_free;
    }
    if (h->input_file) fclose(h->input_file);
    if (h->output_file) fclose(h->output_file);
    free(h);
    h = NULL;
}

/*! Open filename for binary read
 *
 * Offers basic error handling
 */
void open_input(Huff* h, const char* filename)
{
    if (!h->quiet && !h->export_tree)
        printf("Input: %s\n", filename);

    h->input_file = fopen(filename, "rb");
    if (!h->input_file)
        SET_ERROR("Can't open input file");
}

/*! Open filename for binary write
 *
 * Offers basic error handling
 */
void open_output(Huff* h, const char* filename)
{
    if (!h->quiet && !h->export_tree)
        printf("Output: %s\n", filename);

    h->output_file = fopen(filename, "wb");
    if (!h->output_file)
        SET_ERROR("Can't create output file");
}

/*! Compress function
 *
 * Compresses input file to output file using tree of nodes
 * Builds nodes, links them into tree and then write compressed data to output file
 */
void compress(Huff* h)
{
    if (!h->quiet && !h->export_tree)
        printf("Compress...\n");

    initialize_nodes(h);
    EXIT_ON_ERROR;
    if (h->export_tree)
        print_tree(h, h->head);
    write_header(h);
    EXIT_ON_ERROR;
    if (h->mode == '2')
    {
        serialize2(h, h->head);
        EXIT_ON_ERROR;
        write_io_buffer(h);   // there can be something remaining in buffer, flush it to disk
    }
    else
        serialize(h, h->head);
    EXIT_ON_ERROR;
    int bit = 7;
    uint8_t enc = 0;
    uint8_t byte;
    rewind(h->input_file);
    while (fread(&byte, sizeof(byte), 1, h->input_file) != 0)
    {
        Node* n = find_node(h, byte);
        if (!n)
        {
            SET_ERROR("Can't find node in Huffman tree with byte %d", byte);
            EXIT_ON_ERROR;
        }

        size_t frame = 0;  // reset temporary coding frame for single byte
        size_t frame_bit = 0;
        while (n->up != NULL)
        {
            if (n->up->left == n)
                SET_BIT(frame, frame_bit);   // bit 1 means move LEFT

            ++frame_bit;
            n = n->up;
        }

        // now I have my encoding in frame, starting from frame_bit - 1;
        // copy it in order to encoding output
    
        for (; frame_bit > 0; --frame_bit)
        {
            if (TEST_BIT(frame, frame_bit - 1))
                SET_BIT(enc, bit);

            --bit;

            if (bit < 0)
            {
                fputc(enc, h->output_file);
                enc = 0;
                bit = 7;
            }
        }
    }

    if (bit != 0)  // put to output last byte of encoding
        fputc(enc, h->output_file);
}

/*! Decompress function
 *
 * Decompresses input file to output file using tree of nodes
 * Reads header, builds tree and then write uncompressed data to output file
 */
void decompress(Huff* h)
{
    if (!h->quiet)
    {
        printf("Decompress\n");
    }
    read_header(h);
    EXIT_ON_ERROR;
    if (h->mode == '2')
        deserialize2(h);
    else
        deserialize_tree(h);
    EXIT_ON_ERROR;
    
    long current_pos = ftell(h->input_file);
    fseek(h->input_file, 0L, SEEK_END);
    long encoded_size = ftell(h->input_file) - current_pos;
    fseek(h->input_file, current_pos, SEEK_SET);

    char *decode = malloc(encoded_size * sizeof(*decode));
    fread(decode, sizeof(*decode), encoded_size, h->input_file);

    long pos = 0;
    int bit = 7;

    for (size_t i = 0; i < h->message_size; ++i)
    {
        Node* n = h->head;
        while (n->left) // if it's not leaf, digg further
        {
            if (TEST_BIT(decode[pos], bit))
                n = n->left;
            else
                n = n->right;

            --bit;
            if (bit < 0)
            {
                bit = 7;
                ++pos;
                if (pos > encoded_size)
                {
                    fprintf(stderr, "Encoded data is too short");
                    exit(1);
                }
            }
        }
        fputc(n->value, h->output_file);  // decoded value from tree leaf
    }
    free(decode);
}

