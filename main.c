#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <regex.h>
#include "huff.h"

enum Action {NONE, COMPRESS, DECOMPRESS};
void help(char* program)
{
    fprintf(stderr, "Huffman copressor/decompressor\n\n");
    fprintf(stderr, "Usage: %s -c -2 filename\n", program);
    fprintf(stderr, "       %s -x filename.huf -q\n\n", program);
    fprintf(stderr, "       -c compress\n");
    fprintf(stderr, "       -x decompress\n");
    fprintf(stderr, "       -1 standard compression (default)\n");
    fprintf(stderr, "       -2 higher compression ratio, slower\n");
    fprintf(stderr, "       -q quiet\n");
    fprintf(stderr, "       -p print Huffman tree in Graphviz format\n");
}

int main(int argc, char** argv)
{
    // function pointer to (de)compress function
    void (* huff)(Huff* h) = NULL;
    enum Action action = NONE;

    int exit_status = EXIT_SUCCESS;
    Huff* h = init();
    if (!h)
    {
        fprintf(stderr, "Can't allocate memory for main structure.");
        return EXIT_FAILURE;
    }

    // prepare regexp
    regex_t regex;
    if (regcomp(&regex, "\\.huf$",0))
    {
        fprintf(stderr, "Can't complie regex\n.");
        return EXIT_FAILURE;
    }

    int option;
    char* in_filename = NULL;
    huff = NULL;
    while ((option = getopt(argc, argv, "c:x:qh12p")) != -1)
    {
        switch (option)
        {
            case 'c':
                if (!action)    // prevents selection both compress and decompress at once
                {
                    huff = &compress;
                    action = COMPRESS;
                    in_filename = optarg;
                }
                break;

            case 'x':
                if (!action)
                {
                    huff = &decompress;
                    action = DECOMPRESS;
                    in_filename = optarg;
                }
                break;

            case '1':
            case '2':
                h->mode = option;
                break;

            case 'q':
                h->quiet = 1;
                break;

            case 'p':
                h->export_tree = 1;
                break;

            default:
                help(argv[0]);
                goto Error;
        }
    }

    if (action == NONE)
    {
        help(argv[0]);
        goto Error;
    }

    open_input(h, in_filename);
    GOTO_ERROR;

    // prepare output filename
    char out_filename[255] = {0};
    if (action == COMPRESS)
    {
        //add .huf to compressed filename
        snprintf(out_filename, 255, "%s.huf", in_filename);
    }
    else // action == DECOMPRESS
    {
        size_t len = strlen(in_filename);  
        // check if filename ends with .huf suffix
        if (len < 5 ||    // filename is too short (edgecase: filename = ".huf")
            regexec(&regex, in_filename, 0, NULL, 0) == REG_NOMATCH)
            // unknown suffix, add .dehuf to decompressed filename
            snprintf(out_filename, 255, "%s.dehuf", in_filename);
        else
        {
            // truncate .huf from input
            in_filename[len - 4] = '\0'; // truncate last 4 chatacters!
            snprintf(out_filename, 255, "%s", in_filename);
        }
    }

    open_output(h, out_filename);
    GOTO_ERROR;

    (* huff)(h);        // all set, ready to go

Error:
    if (h->error)
        fprintf(stderr, "Error: %s\n", h->error_msg);

    cleanup(h);
    return exit_status;
}

