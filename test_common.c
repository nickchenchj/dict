#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "bench.c"
#include "bloom.h"
#include "tst.h"
#include "xorfilter.h"

#define ANALYZE_MODE
// #define BLOOM_ENABLED
#define XOR_ENABLED

#define MAX_WORDS 2000000

#ifdef BLOOM_ENABLED
#define TableSize 5000000 /* size of bloom filter */
#define HashNumber 2      /* number of hash functions */
#endif

/** constants insert, delete, max word(s) & stack nodes */
enum { INS, DEL, WRDMAX = 256, STKMAX = 512, LMAX = 1024 };

int REF = INS;


#define BENCH_TEST_FILE "bench_ref.txt"

long poolsize = MAX_WORDS * WRDMAX;

/* simple trim '\n' from end of buffer filled by fgets */
static void rmcrlf(char *s)
{
    size_t len = strlen(s);
    if (len && s[len - 1] == '\n')
        s[--len] = 0;
}

static uint64_t jenkins(const void *_str)
{
    const char *key = _str;
    uint64_t hash = 0;
    while (*key) {
        hash += *key;
        hash += (hash << 10);
        hash ^= (hash >> 6);
        key++;
    }
    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);
    return hash;
}

#define IN_FILE "_cities.txt"

int main(int argc, char **argv)
{
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
        // printf("CPY mechanism\n");
    } else {
        // printf("REF mechanism\n");
    }

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

    // t1 = tvgetf();
    double elapsed_time = 0.0;
#ifdef BLOOM_ENABLED
    // t1 = tvgetf();
    bloom_t bloom = bloom_create(TableSize);
    // t2 = tvgetf();
    // elapsed_time += t2 - t1;
#elif defined(XOR_ENABLED)
    xor8_t filter;
    char **word_set = malloc(MAX_WORDS * sizeof(char *));
    if (!word_set) {
        fprintf(stderr, "error: memory allocation failed\n");
        exit(EXIT_FAILURE);
    }
#endif

    char buf[WORDMAX];
    // FILE *out_file = fopen("_cities.txt", "w");
    // if (!out_file) {
    //     fprintf(stderr, "error: unable to open _cities.txt\n");
    // }

    while (fgets(buf, WORDMAX, fp)) {
        for (int i = 0;; i++) {
            if (buf[i] == '\n') {
                Top[i] = '\0';
                break;
            }
            Top[i] = buf[i];
        }
        // printf("[%d] %s\n", idx, Top);
//         int offset = 0;
//         for (int i = 0, j = 0; buf[i + offset]; i++) {
//             Top[i] =
//                 (buf[i + j] == ',' || buf[i + j] == '\n') ? '\0' : buf[i +
//                 j];
//             j += (buf[i + j] == ',');
//         }
//         while (*Top) {
//             /* Ignores duplicated words */
//             if (!tst_search(root, Top)) {
//                 // fprintf(out_file, "%s\n", Top);
//                 char *str = tst_ins(&root, Top, REF);
//                 if (!str) { /* fail to insert */
//                     fprintf(stderr, "error: memory exhausted,
//                     tst_insert.\n"); fclose(fp); return 1;
//                 }
// char *str = tst_ins(&root, Top, REF);
// if (!str) { /* fail to insert */
//     fprintf(stderr, "error: memory exhausted, tst_insert.\n");
//     fclose(fp);
//     return 1;
// }
#ifdef BLOOM_ENABLED
        t1 = tvgetf();
        bloom_add(bloom, Top);
        t2 = tvgetf();
        elapsed_time += t2 - t1;
#elif defined(XOR_ENABLED)
        // word_set[idx] = str;
        word_set[idx] = Top;
        // printf("[%d] %s\n", idx, word_set[idx]);
#endif
        idx++;
        //             }

        int len = strlen(Top);
        // offset += len + 1;
        Top += len + 1;
        //         }
        // Top -= offset & ~CPYmask;
        // memset(Top, '\0', WORDMAX);
    }
    // fclose(out_file);
    // printf("total words: %d\n", idx);

#ifdef XOR_ENABLED
    uint64_t *key_set = malloc(idx * sizeof(uint64_t));
    if (!key_set) {
        fprintf(stderr, "error: memory allocation failed\n");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < idx; i++) {
        t1 = tvgetf();
        key_set[i] = jenkins(word_set[i]);
        // printf("after hash: %lu\n", key_set[i]);
        t2 = tvgetf();
        elapsed_time += t2 - t1;
    }
    xor8_allocate(idx, &filter);
    t1 = tvgetf();
    bool is_ok = xor8_populate(key_set, idx, &filter);
    t2 = tvgetf();
    elapsed_time += t2 - t1;
    if (!is_ok) {
        fprintf(stderr, "error: duplicated words\n");
        exit(EXIT_FAILURE);
    }

    // for (int i = 0; i < idx; i++) {
    //     if (!xor8_contain(key_set[i], &filter)) {
    //         fprintf(stderr, "error: %s is not added to xor filter\n",
    //                 word_set[i]);
    //         exit(EXIT_FAILURE);
    //     }
    // }
    free(word_set);
    free(key_set);
#endif

    // t2 = tvgetf();
    fclose(fp);
    // printf("ternary_tree, loaded %d words in %.6f sec\n", idx, t2 - t1);

#ifdef ANALYZE_MODE
    fp = fopen(IN_FILE, "r");
    if (!fp) {
        fprintf(stderr, "error: file open failed '%s'.\n", argv[1]);
        exit(EXIT_FAILURE);
    }
    // double elapsed_time = 0.0;
    // int word_count = 0;
    while (fgets(buf, WORDMAX, fp)) {
        for (int i = 0;; i++) {
            if (buf[i] == '\n') {
                buf[i] = '\0';
                break;
            }
        }
        // t1 = tvgetf();
#ifdef BLOOM_ENABLED
        if (bloom_test(bloom, buf)) {
            // printf("in Bloom filter\n");
        } else {
            // printf("not in Bloom filter\n");
        }
#elif defined(XOR_ENABLED)
        if (xor8_contain(jenkins(buf), &filter)) {
            // printf("in xor filter\n");
        } else {
            // printf("not in xor filter\n");
        }
#endif
        // t2 = tvgetf();
        // elapsed_time += t2 - t1;
        // word_count++;
    }
    fclose(fp);
    // printf("total words: %d\n", word_count);
    fp = fopen("results.txt", "a");
    if (!fp) {
        fprintf(stderr, "error: failed to open results.txt\n");
        exit(EXIT_FAILURE);
    }
    fprintf(fp, "%.8f\n", elapsed_time);
    fclose(fp);

#else
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
#endif
                t1 = tvgetf();
                res = tst_search(root, word);
                t2 = tvgetf();
                if (res)
                    printf("  ----------\n  Tree found %s in %.6f sec.\n",
                           (char *) res, t2 - t1);
                else
                    printf("  ----------\n  %s not found by tree.\n", word);
#ifdef BLOOM_ENABLED
            } else
                printf("  %s not found by bloom filter.\n", word);
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
#elif defined(XOR_ENABLED)
    xor8_free(&filter);
#endif
    return 0;
}
