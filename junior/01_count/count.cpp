#include <stdio.h>

#define OUT 0
#define IN  1

#define INIT OUT

bool spliter(char c) {
    if (c == ' ' || c == '\n' || c == '\t' ||
        c == '\'' || c == '\"' || c == '+' ||
        c == ',' || c == '?' || c == ';' || c == '.') {
        return true; //在单词外部
    }
    return false; //在单词内部
}

int count_words(const char *filename) {
    int status = INIT;
    FILE *fp = fopen(filename, "r");
    int count = 0;

    char c;
    while ((c = fgetc(fp)) != EOF) {
        if (!spliter(c) && status == OUT) {
            status = IN;
            count++;
        } else if (spliter(c)) {
            status = OUT;
        }
    }
    return count;
}

int main(int argc, char *argvs[]) {
    printf("%d\n", count_words(argvs[1]));
    return 0;
}
