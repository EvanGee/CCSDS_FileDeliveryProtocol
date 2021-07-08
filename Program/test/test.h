
#ifndef PACKET_TEST_H
#define PACKET_TEST_H
#include <stdio.h>


int assert_equals_str(char *file_name, int line_num, char* description, void *val1,  void* val2, size_t size);
int assert_not_equals_int(char *file_name, int line_num,  char* description, int val1, int val2);
int assert_equals_int(char *file_name, int line_num,  char* description, int val1, int val2);
int assert_not_equals_str(char *file_name, int line_num, char* description, void *val1,  void* val2, size_t size);
int assert_null(char *file_name, int line_num, char* description, void *val1);
int assert_not_null(char *file_name, int line_num, char* description, void *val1);

void DECLARE_NEW_TEST(char *description);

#define ASSERT_EQUALS_STR(description, val1, val2, size)\
    assert_equals_str(__FILE__, __LINE__, description, val1, val2, size);

#define ASSERT_NOT_EQUALS_INT(description, val1, val2)\
    assert_not_equals_int(__FILE__, __LINE__,  description, val1, val2);

#define ASSERT_EQUALS_INT(description, val1, val2)\
    assert_equals_int(__FILE__, __LINE__,  description, val1, val2);

#define ASSERT_NOT_EQUALS_STR(description, val1, val2, size)\
    assert_not_equals_str(__FILE__, __LINE__, description, val1, val2, size);

#define ASSERT_NULL(description, val1)\
    assert_null(__FILE__, __LINE__, description, val1);

#define ASSERT_NOT_NULL(description, val1)\
    assert_not_null(__FILE__, __LINE__, description, val1);

    
#endif