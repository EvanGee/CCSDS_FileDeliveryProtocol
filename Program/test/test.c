
#include "test.h"
#include <stdio.h>
#include <string.h>

int test_num = 0;



void DECLARE_NEW_TEST(char *description) {
    printf("\x1b[33m");
    printf("-----------------------------------%s-----------------------------------\n", description);
}


int ASSERT_EQUALS_INT(char* description, int val1, int val2) {
    
    test_num++;
    if (val1 == val2){
        printf("\033[0;32m");
        printf("%s", description);
        printf(" pass # %d\n", test_num);
    }
    else {
        printf("\033[0;31m");
        printf("%s", description);
        printf(" fail # %d %d %s\n", test_num, __LINE__, __FILE__);
        printf("\033[0m"); 
        return 1;
    }
    printf("\033[0m"); 
    return 0;
}


int ASSERT_NOT_EQUALS_INT(char* description, int val1, int val2) {
    
    test_num++;
    if (val1 == val2){
        printf("\033[0;31m");
        printf("%s", description);
        printf(" fail # %d %d %s\n", test_num, __LINE__, __FILE__);
        printf("\033[0m"); 
        return 1;
    }
    else {
        printf("\033[0;32m");
        printf("%s", description);
        printf(" pass # %d\n", test_num);
    }
    printf("\033[0m"); 
    return 0;
}


int ASSERT_EQUALS_STR(char* description, char *val1,  char* val2, size_t size) {
    
    test_num++;
    if (!memcmp(val1, val2, size)) {
        printf("\033[0;32m");
        printf("%s", description);
        printf(" pass # %d\n", test_num);

    } else {
        printf("\033[0;31m");
        printf("%s", description);
        printf(" fail # %d %d %s\n", test_num, __LINE__, __FILE__);
        printf("\033[0m"); 
        return 1;

    }
    printf("\033[0m"); 
    return 0;
}

int ASSERT_NOT_EQUALS_STR(char* description, char *val1,  char* val2, size_t size) {
    
    test_num++;
    if (!memcmp(val1, val2, size)) {

        printf("\033[0;31m");
        printf("%s", description);
        printf(" fail # %d %d %s\n", test_num, __LINE__, __FILE__);
        printf("\033[0m"); 
        return 1;

    } else {
        printf("\033[0;32m");
        printf("%s", description);
        printf(" pass # %d\n", test_num);
    }
    printf("\033[0m"); 
    return 0;
}


int ASSERT_NULL(char* description, void *val1) {
    test_num++;
    if (val1 != NULL) {
        printf("\033[0;31m");
        printf("%s", description);
        printf(" fail # %d %d %s\n", test_num, __LINE__, __FILE__);
        printf("\033[0m"); 
        return 1;
    } else {
        printf("\033[0;32m");
        printf("%s", description);
        printf(" pass # %d\n", test_num);
    }
    printf("\033[0m"); 
    return 0;

}

int ASSERT_NOT_NULL(char* description, void *val1) {

    test_num++;
    if (val1 == NULL) {
        printf("\033[0;31m");
        printf("%s", description);
        printf(" fail # %d %d %s\n", test_num, __LINE__, __FILE__);
        printf("\033[0m"); 
        return 1;

    } else {
        printf("\033[0;32m");
        printf("%s", description);
        printf(" pass # %d\n", test_num);
    }
    printf("\033[0m"); 
    return 0;
}