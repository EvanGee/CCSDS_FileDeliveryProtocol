
#include "test.h"
#include <stdio.h>
int test_num = 0;



void DECLARE_NEW_TEST(char *description) {
    printf("\x1b[33m");
    printf("-----------------------------------%s-----------------------------------\n", description);
}


void ASSERT_EQUALS_INT(char* description, int val1, int val2) {
    
    test_num++;
    if (val1 == val2){
        printf("\033[0;32m");
        printf("%s", description);
        printf(" pass # %d\n", test_num);
    }
    else {
        printf("\033[0;31m");
        printf("%s", description);
        printf(" fail # %d\n", test_num);
    }
    printf("\033[0m"); 
}


void ASSERT_NOT_EQUALS_INT(char* description, int val1, int val2) {
    
    test_num++;
    if (val1 == val2){
        printf("\033[0;31m");
        printf("%s", description);
        printf(" fail # %d %s %s\n", test_num, __LINE__, __FILE__);
    }
    else {
        printf("\033[0;32m");
        printf("%s", description);
        printf(" pass # %d\n", test_num);
    }
    printf("\033[0m"); 
}


void ASSERT_EQUALS_STR(char* description, char *val1,  char* val2, size_t size) {
    
    test_num++;
    if (!memcmp(val1, val2, size)) {
        printf("\033[0;32m");
        printf("%s", description);
        printf(" pass # %d\n", test_num);

    } else {
        printf("\033[0;31m");
        printf("%s", description);
        printf(" fail # %d\n", test_num);

    }
    printf("\033[0m"); 
}

void ASSERT_NOT_EQUALS_STR(char* description, char *val1,  char* val2, size_t size) {
    
    test_num++;
    if (!memcmp(val1, val2, size)) {

        printf("\033[0;31m");
        printf("%s", description);
        printf(" fail # %d\n", test_num);

    } else {
        printf("\033[0;32m");
        printf("%s", description);
        printf(" pass # %d\n", test_num);
    }
    printf("\033[0m"); 
}
