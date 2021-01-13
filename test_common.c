#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "bench.c"
#include "bloom.h"
#include "tst.h"

#define ANALYSIS_MODE
#define BLOOM_ENABLED
#define RANDOM_ENABLED

#ifdef BLOOM_ENABLED
#define TableSize 5000000 /* size of bloom filter */
#define HashNumber 2      /* number of hash functions */
#endif

#ifdef RANDOM_ENABLED
// #define RANDOMIZE_LAST_CHAR
static int threshold = 20;
#endif

/** constants insert, delete, max word(s) & stack nodes */
enum { INS, DEL, WRDMAX = 256, STKMAX = 512, LMAX = 1024 };

int REF = INS;

#define BENCH_TEST_FILE "bench_ref.txt"

long poolsize = 2000000 * WRDMAX;

/* simple trim '\n' from end of buffer filled by fgets */
static void rmcrlf(char *s)
{
    size_t len = strlen(s);
    if (len && s[len - 1] == '\n')
        s[--len] = 0;
}

#define IN_FILE "cities.txt"

int main(int argc, char **argv)
{
#ifdef RANDOM_ENABLED
    srand(time(NULL));
    const char letters[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
#endif
    char word[WRDMAX] = "";
    char *sgl[LMAX] = {NULL};
    tst_node *root = NULL, *res = NULL;
    int idx = 0, sidx = 0;
    double t1, t2;
    int CPYmask = -1;
    if (argc < 2) {
        printf("too less argument\n");
        return 1;
    }

    if (!strcmp(argv[1], "CPY") || (argc > 2 && !strcmp(argv[2], "CPY"))) {
        CPYmask = 0;
        REF = DEL;
        printf("CPY mechanism\n");
    } else
        printf("REF mechanism\n");

    char *Top = word;
    char *pool = NULL;

    if (CPYmask) {  // Only allacte pool in REF mechanism
        pool = malloc(poolsize);
        if (!pool) {
            fprintf(stderr, "Failed to allocate memory pool.\n");
            return 1;
        }
        Top = pool;
    }

    FILE *fp = fopen(IN_FILE, "r");

    if (!fp) { /* prompt, open, validate file for reading */
        fprintf(stderr, "error: file open failed '%s'.\n", argv[1]);
        return 1;
    }

    t1 = tvgetf();
#ifdef BLOOM_ENABLED
    bloom_t bloom = bloom_create(TableSize);
#endif

    char buf[WORDMAX];
    while (fgets(buf, WORDMAX, fp)) {
        int offset = 0;
        for (int i = 0, j = 0; buf[i + offset]; i++) {
            Top[i] =
                (buf[i + j] == ',' || buf[i + j] == '\n') ? '\0' : buf[i + j];
            j += (buf[i + j] == ',');
        }
        while (*Top) {
            if (!tst_ins(&root, Top, REF)) { /* fail to insert */
                fprintf(stderr, "error: memory exhausted, tst_insert.\n");
                fclose(fp);
                return 1;
            }
#ifdef BLOOM_ENABLED
            bloom_add(bloom, Top);
#endif
            idx++;
            int len = strlen(Top);
            offset += len + 1;
            Top += len + 1;
        }
        Top -= offset & ~CPYmask;
        memset(Top, '\0', WORDMAX);
    }
    t2 = tvgetf();
    fclose(fp);
    printf("ternary_tree, loaded %d words in %.6f sec\n", idx, t2 - t1);

    if (argc == 3 && strcmp(argv[1], "--bench") == 0) {
        int stat = bench_test(root, BENCH_TEST_FILE, LMAX);
        tst_free(root);
        free(pool);
        return stat;
    }

    FILE *output;
    output = fopen("ref.txt", "a");
    if (output != NULL) {
        fprintf(output, "%.6f\n", t2 - t1);
        fclose(output);
    } else
        printf("open file error\n");

#ifdef ANALYSIS_MODE
    /* Sets test file */
    char test_filename[256];
    strncpy(test_filename, argv[2], 256);
    FILE *test_file = fopen(test_filename, "r");
    if (test_file == NULL) {
        fprintf(stderr, "Error: Could not open file %s\n", test_filename);
        exit(EXIT_FAILURE);
    }

    int not_found_count = 0;
    double total_time = 0.0;
    while (fgets(buf, WORDMAX, test_file)) {
        memset(word, '\0', WORDMAX);
        for (int i = 0, j = 0; buf[i]; i++) {
            word[i] =
                (buf[i + j] == ',' || buf[i + j] == '\n') ? '\0' : buf[i + j];
            j += (buf[i + j] == ',');
        }

        /* Find words in TST */
        char *ptr = word;
        while (*ptr) {
#ifdef RANDOM_ENABLED
#ifdef RANDOMIZE_LAST_CHAR
            if (rand() % 100 >= threshold) {
                /* Randomizes the last character of the string */
                int last_idx = strlen(ptr) - 1;
                char tmp;
                do {
                    tmp = letters[rand() % (sizeof(letters) - 1)];
                } while (tmp == ptr[last_idx]);
                ptr[last_idx] = tmp;
            }
#else
            size_t len = strlen(ptr);
            /* Randomizes the whole string */
            for (int i = 0; i < len; i++) {
                ptr[i] = letters[rand() % (sizeof(letters) - 1)];
            }
#endif
#endif
            res = NULL;
            t1 = tvgetf();
#ifdef BLOOM_ENABLED
            if (bloom_test(bloom, ptr)) {
                res = tst_search(root, ptr);
            }
#else
            res = tst_search(root, ptr);
#endif
            t2 = tvgetf();
            total_time += (t2 - t1);

            if (!res)
                not_found_count++;
            ptr += strlen(ptr) + 1;
        }
    }
    printf("%d words not found, %5.2f%% of all words\n", not_found_count,
           ((float) not_found_count / (float) idx) * 100.0f);
    fclose(test_file);

    const char output_filename[] = "results.txt";
    FILE *output_file = fopen(output_filename, "a");
    if (output_file == NULL) {
        fprintf(stderr, "Error: Could not open file %s\n", output_filename);
        exit(EXIT_FAILURE);
    }
    fprintf(output_file, "%.6f\n", total_time);
    fclose(output_file);
#else
    for (;;) {
        printf(
            "\nCommands:\n"
            " a  add word to the tree\n"
            " f  find word in tree\n"
            " s  search words matching prefix\n"
            " d  delete word from the tree\n"
            " q  quit, freeing all data\n\n"
            "choice: ");

        if (argc > 2 && strcmp(argv[1], "--bench") == 0)  // a for auto
            strcpy(word, argv[3]);
        else
            fgets(word, sizeof word, stdin);

        switch (*word) {
        case 'a':
            printf("enter word to add: ");
            if (argc > 2 && strcmp(argv[1], "--bench") == 0)
                strcpy(Top, argv[4]);
            else if (!fgets(Top, sizeof word, stdin)) {
                fprintf(stderr, "error: insufficient input.\n");
                break;
            }
            rmcrlf(Top);

            t1 = tvgetf();
#ifdef BLOOM_ENABLED
            if (bloom_test(bloom, Top)) /* if detected by filter, skip */
                res = NULL;
            else { /* update via tree traversal and bloom filter */
                bloom_add(bloom, Top);
                res = tst_ins(&root, Top, REF);
            }
#else
            res = tst_ins(&root, Top, REF);
#endif
            t2 = tvgetf();
            if (res) {
                idx++;
                Top += (strlen(Top) + 1) & CPYmask;
                printf("  %s - inserted in %.10f sec. (%d words in tree)\n",
                       (char *) res, t2 - t1, idx);
            }

            if (argc > 2 && strcmp(argv[1], "--bench") == 0)  // a for auto
                goto quit;
            break;
        case 'f':
            printf("find word in tree: ");
            if (!fgets(word, sizeof word, stdin)) {
                fprintf(stderr, "error: insufficient input.\n");
                break;
            }
            rmcrlf(word);

#ifdef BLOOM_ENABLED
            t1 = tvgetf();
            if (bloom_test(bloom, word)) {
                t2 = tvgetf();
                printf("  Bloomfilter found %s in %.6f sec.\n", word, t2 - t1);
                printf(
                    "  Probability of false positives:%lf\n",
                    pow(1 - exp(-(double) HashNumber /
                                (double) ((double) TableSize / (double) idx)),
                        HashNumber));
                t1 = tvgetf();
                res = tst_search(root, word);
                t2 = tvgetf();
                if (res)
                    printf("  ----------\n  Tree found %s in %.6f sec.\n",
                           (char *) res, t2 - t1);
                else
                    printf("  ----------\n  %s not found by tree.\n", word);
            } else
                printf("  %s not found by bloom filter.\n", word);
#else
            t1 = tvgetf();
            res = tst_search(root, word);
            t2 = tvgetf();
            if (res)
                printf("  ----------\n  Tree found %s in %.6f sec.\n",
                       (char *) res, t2 - t1);
            else
                printf("  ----------\n  %s not found by tree.\n", word);
#endif
            break;
        case 's':
            printf("find words matching prefix (at least 1 char): ");

            if (argc > 2 && strcmp(argv[1], "--bench") == 0)
                strcpy(word, argv[4]);
            else if (!fgets(word, sizeof word, stdin)) {
                fprintf(stderr, "error: insufficient input.\n");
                break;
            }
            rmcrlf(word);
            t1 = tvgetf();
            res = tst_search_prefix(root, word, sgl, &sidx, LMAX);
            t2 = tvgetf();
            if (res) {
                printf("  %s - searched prefix in %.6f sec\n\n", word, t2 - t1);
                for (int i = 0; i < sidx; i++)
                    printf("suggest[%d] : %s\n", i, sgl[i]);
            } else
                printf("  %s - not found\n", word);

            if (argc > 2 && strcmp(argv[1], "--bench") == 0)  // a for auto
                goto quit;
            break;
        case 'd':
            printf("enter word to del: ");
            if (!fgets(word, sizeof word, stdin)) {
                fprintf(stderr, "error: insufficient input.\n");
                break;
            }
            rmcrlf(word);
            printf("  deleting %s\n", word);
            t1 = tvgetf();
            /* FIXME: remove reference to each string */
            res = tst_del(&root, word, REF);
            t2 = tvgetf();
            if (res)
                printf("  delete failed.\n");
            else {
                printf("  deleted %s in %.6f sec\n", word, t2 - t1);
                idx--;
            }
            break;
        case 'q':
            goto quit;
        default:
            fprintf(stderr, "error: invalid selection.\n");
            break;
        }
    }
#endif

quit:
    free(pool);
    /* for REF mechanism */
    if (CPYmask)
        tst_free(root);
    else
        tst_free_all(root);

#ifdef BLOOM_ENABLED
    bloom_free(bloom);
#endif
    return 0;
}
