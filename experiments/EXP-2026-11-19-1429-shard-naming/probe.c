/* The shard naming API on names whose answer is known: llama_split_prefix must copy the name with the split
 * index and total replaced, and llama_split_path must compose a name in the standard form. */
#include "llama.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    int bad = 0;
    char buf[256];

    /* "<name>-00002-of-00004.gguf" with idx 1 of 4 -> the prefix "name-", in the convention llama.cpp uses */
    memset(buf, 0, sizeof(buf));
    const int n = llama_split_prefix(buf, sizeof(buf), "model-00002-of-00004.gguf", 1, 4);
    /* the prefix is the name up to the '-' that precedes the index: "model" */
    printf("split_prefix -> n=%d prefix=[%s]\n", n, buf);
    if (n != 5 || strcmp(buf, "model") != 0) { ++bad; }

    memset(buf, 0, sizeof(buf));
    const int m = llama_split_path(buf, sizeof(buf), "model.gguf", 3, 7);
    printf("split_path   -> n=%d name=[%s]\n", m, buf);
    /* the convention is idx 0-based and the suffix appended to the whole path: idx 3 of 7 -> 00004-of-00007 */
    if (m <= 0 || strstr(buf, "-00004-of-00007.gguf") == NULL) { ++bad; }

    /* a name that is not a split must be refused rather than silently accepted */
    memset(buf, 0, sizeof(buf));
    const int r = llama_split_prefix(buf, sizeof(buf), "model.gguf", 0, 2);
    printf("split_prefix on a plain name -> n=%d\n", r);
    if (r != 0) { ++bad; }

    printf("shard naming: cases=3 failures=%d\n", bad);
    return bad ? 1 : 0;
}
