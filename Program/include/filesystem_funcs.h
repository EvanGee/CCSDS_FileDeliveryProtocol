/*------------------------------------------------------------------------------
This file is protected under copyright. If you want to use it,
please include this text, that is my only stipulation.  

Author: Evan Giese
------------------------------------------------------------------------------*/
#ifndef FILESYSTEM_FUNCS_H
#define FILESYSTEM_FUNCS_H
#include "types.h"

#define TEMP_FILESIZE 1000

struct params {
    int error;
    int fd;
};


int get_file_size(char *source_file_name);

//allocates space for a file pointer
File *create_file(char *source_file_name, int clear_file_contents);
void ssp_free_file(void *file);
int add_first_offset(File *file, uint32_t file_size);

int does_file_exist(char *source_file_name);
int get_offset(File *file, void *buff, uint32_t buf_size, int offset);
int write_offset(File *file, void *buff, uint32_t size, uint32_t offset);
uint32_t calc_check_sum(char *data, uint32_t length);
uint32_t check_sum_file(File *file, uint16_t stack_buffer);
int receive_offset(File *file, uint32_t offset_start, uint32_t offset_end);
File *create_temp_file(char *file_name, uint32_t size);
int change_tempfile_to_actual(char *temp, char *destination_file_name, uint32_t file_size, File *file);
int read_json(char *file_name, int (*callback)(char *key, char *value, void *params), void *params);

int write_lv(int fd, LV lv);
int read_lv(int fd, LV *lv);

int read_id(int fd, uint64_t *id);
int write_id(int fd, uint64_t id);

int save_req_to_file(Request *req);
int save_file_to_file(int fd, File *file);
int get_file_from_file(int fd, File *file);
int get_req_from_file(uint32_t dest_cfdp_id, uint64_t transaction_seq_num, uint32_t my_cfdp_id, Request *req);
int delete_saved_request(Request *req);
int read_request_from_file(int fd, Request *req);


//new json stuff:
int write_request_json (Request *req, char *file_name);
int get_request_from_json (Request *req, char *file_name);

#endif 
