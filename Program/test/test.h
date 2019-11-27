
#ifndef PACKET_TEST_H
#define PACKET_TEST_H
#include <stdio.h>

void DECLARE_NEW_TEST(char *description);
int ASSERT_EQUALS_STR(char* description, char *val1,  char* val2, size_t size);
int ASSERT_NOT_EQUALS_INT(char* description, int val1, int val2);
int ASSERT_EQUALS_INT(char* description, int val1, int val2);
int ASSERT_NOT_EQUALS_STR(char* description, char *val1,  char* val2, size_t size);
int ASSERT_NULL(char* description, void *val1);
int ASSERT_NOT_NULL(char* description, void *val1);


#endif