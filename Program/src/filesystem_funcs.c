#include <stdint.h>
#include "port.h"
#include "filesystem_funcs.h"
#include <fcntl.h>
#include <stddef.h>
#include <unistd.h>
#include "utils.h"
#include <string.h>
#include "list.h"
#include "jsmn.h"
#include "requests.h"

uint32_t get_file_size(char *source_file_name) {

    int fd = ssp_open(source_file_name, 0);
    if (fd == -1){
        ssp_error("could not open file\n");
        return 0;
    }

    int bytes = ssp_lseek(fd, 0, 2);
    if (bytes == -1){
        ssp_error("could not seek file for file size\n");
        return 0;
    }

    ssp_lseek(fd, 0, 0);

    if (ssp_close(fd) == -1){
        ssp_error("could not close file\n");
        return 0;
    }

    return bytes;
}

File *create_file(char *source_file_name, int clear_file_contents) {

    int fd = 0;
    if (clear_file_contents){
        fd = ssp_open(source_file_name, O_RDWR | O_CREAT | O_TRUNC);
    }else {
        fd = ssp_open(source_file_name, O_RDWR | O_CREAT);
    }   
    if (fd == -1){
        ssp_error("couldn't create file\n");
        fd = ssp_open(source_file_name, O_RDWR);
        if (fd == -1) {
            ssp_error("count not open file\n");
            return NULL;
        }
    }


    uint32_t total_size = get_file_size(source_file_name);
    if (total_size == -1){
        ssp_error("couldn't get file size\n");
        return NULL;
    }

    File *file = ssp_alloc(1, sizeof(File));
    
    file->fd = fd;
    file->eof_checksum = 0;
    file->next_offset_to_send = 0;
    file->total_size = total_size;
    file->partial_checksum = 0;
    file->missing_offsets = linked_list();

    return file;

}


int does_file_exist(char *source_file_name) {

    int fd = ssp_open(source_file_name, O_RDWR);
    if (fd == -1){
        return 0;
    }
    if (ssp_close(fd) == -1){
        return -1;
    }

    return 1;
}

//modifys the seek location, returns bytes read
int get_offset(File *file, void *buff, uint32_t buf_size, int offset) {

    if (offset >= file->total_size){
        ssp_error("offset greater than file size\n");
        return -1;
    }

    if (ssp_lseek(file->fd, offset, SEEK_SET) == -1){
        ssp_error("could'nt set offset\n");
    }

    int bytes = ssp_read(file->fd, buff, buf_size);
    if (bytes == -1){
        ssp_error("Could not read anything from file\n");
    }

    return bytes;

}

int write_offset(File *file, void *buff, uint32_t size, uint32_t offset) {

    if (file == NULL) {
        ssp_error("Could not write, File struct is not created\n");
        return -1;
    }

    ssp_lseek(file->fd, (int) offset, SEEK_SET);
    int bytes = ssp_write(file->fd, buff, (size_t) size);

    if (bytes == -1){
        ssp_error("Could not write\n");
    }
    if (bytes < size && bytes >= 0){
        ssp_error("did not write all the bytes, this could be because the disk is full, or the file that was sent is empty!\n");
    }
    return bytes;
}

void free_file(void *file) {

    File *f = (File *) file;
    f->missing_offsets->free(f->missing_offsets, ssp_free);
    ssp_free(f);
}



uint32_t calc_check_sum(char *data, uint32_t length) {
    uint8_t remaining_bytes = length % 4;
    uint32_t check_sum = 0;
    uint32_t end = length - 4;
    for (unsigned int i = 0; i < end; i+= 4){
        check_sum += *((uint32_t *) &data[i]);
    }
    
    if (remaining_bytes){
        uint8_t last_chunk[4];
        memset(last_chunk, 0, 4);

        end = length - remaining_bytes;

        for (uint8_t i = 0; i < remaining_bytes; i++) {
            last_chunk[i] = data[end + i];;
        } 
          
        check_sum += *((uint32_t*) &last_chunk);        
    }

    return check_sum;
}


//stack buffer is the size of the packet length
uint32_t check_sum_file(File *file, uint16_t stack_buffer) {

    char buff[stack_buffer];
    uint32_t checksum = 0;
    uint32_t bytes_read = 0;
    for (int i = 0; i < file->total_size; i++) {
        
        bytes_read = get_offset(file, buff, stack_buffer, (int) stack_buffer);
        if (bytes_read > 0)
            checksum += calc_check_sum(buff, bytes_read);
    }

    return checksum;
}

static int find_nak(void *element, void* args) {

    Offset *offset_in_list = (Offset *) element;
    Offset *offset_to_insert = (Offset *) args;

    if (offset_to_insert->start >= offset_in_list->start && offset_to_insert->start <= offset_in_list->end
    && offset_to_insert->end <= offset_in_list->end && offset_to_insert->end >= offset_in_list->start) { 
        return 1;
    }

    return 0;
}

//ack is 1, nak is 0
int receive_offset(File *file, uint8_t ack, uint32_t offset_start, uint32_t offset_end) {
    
    List * nak_list = file->missing_offsets;

    Offset offset_to_insert;
    offset_to_insert.start = offset_start;
    offset_to_insert.end = offset_end;
    
    Node *node = nak_list->findNode(nak_list, -1, find_nak, &offset_to_insert);
    if (node == NULL){

        ssp_printf("trying to receive offset start:%u end:%u\n", offset_to_insert.start, offset_to_insert.end);
        ssp_printf("no begining node for receive_offset, or offset we already received, can't add new offset\n");
        return 0; 
    }

    Offset *offset_in_list = (Offset *) node->element;
    //ssp_printf("received offset start:%u end:%u, found node: start:%u end:%u\n", offset_to_insert.start, offset_to_insert.end, offset_in_list->start, offset_in_list->end);

    //remove node if both start and end are equal
    if (offset_to_insert.start == offset_in_list->start && offset_to_insert.end == offset_in_list->end) {
        //ssp_printf("removing node\n");  
        node->next->prev = node->prev;
        node->prev->next = node->next;
        ssp_free(node->element);
        ssp_free(node);
        nak_list->count--;
        return 1;
    }

    //if new offset is in the start, change the list's node's start
    if (offset_to_insert.start == offset_in_list->start && offset_to_insert.start < offset_in_list->end) {
        offset_in_list->start = offset_to_insert.end;
        return 1;
    }

    Offset *new_offset = ssp_alloc(1, sizeof(Offset));

    new_offset->start = offset_in_list->start;
    offset_in_list->start = offset_end;
    new_offset->end = offset_start;

    Node *cur = node;
    Node *new = createNode(new_offset, new_offset->start);

    new->next = cur;
    new->prev = cur->prev;
    new->prev->next = new;
    cur->prev = new;
    nak_list->count++;
        
    //remove end node
    if (offset_in_list->start == offset_in_list->end){
        ssp_printf("removing last node\n");
        ssp_free(nak_list->pop(nak_list));
    }
    return 1;

}

File *create_temp_file(char *file_name, uint32_t size) {
    File *file = create_file(file_name, 1);
    file->is_temp = 1;
    file->total_size = size;

    ssp_printf("mode acknowledged, building offset map\n");
    Offset *offset = ssp_alloc(1, sizeof(Offset));
    offset->end = size;
    offset->start = 0;
    file->missing_offsets->insert(file->missing_offsets, offset, size);
    return file;
}

/*
static int print_nak(void *element, void* args) {

    Offset *offset_in_list = (Offset *) element;
    ssp_printf("start: %u, end: %u\n", offset_in_list->start, offset_in_list->end);
    return 0;
}
*/

int change_tempfile_to_actual(char *temp, char *destination_file_name, uint32_t file_size, File *file) {

    ssp_printf("renaming %s to: %s", temp, destination_file_name);
    ssp_rename(temp, destination_file_name);
    
    //file->missing_offsets->print(file->missing_offsets, print_nak, NULL);
    Offset* offset = (Offset*)file->missing_offsets->pop(file->missing_offsets);
    if (offset == NULL) {
        ssp_printf("no last node to pop\n");
        return -1;
    }


    offset->end = file_size;
    file->missing_offsets->push(file->missing_offsets, offset, file_size);
    file->is_temp = 0;
    file->total_size = file_size;
    return 0;
}

int read_json(char *file_name, void (*callback)(char *key, char *value, void *params), void *params) {

    jsmn_parser p;
    int number_of_tokens = 255;
    jsmntok_t tok[number_of_tokens];

    jsmn_init(&p);

    uint32_t total_size = get_file_size(file_name);
    if (total_size == 0){
        ssp_error("couldn't get file size\n");
        return -1;
    }

    char buff[total_size];

    int fd = ssp_open(file_name, O_RDWR);
    if (fd < 0) {
        ssp_error("couldn't open file\n");
        return -1;
    }
    
    int r = ssp_read(fd, buff, sizeof(buff));
    if (r < 0) {
        ssp_error("read failed\n");
        return -1;
    }

    r = jsmn_parse(&p, buff, total_size, tok, number_of_tokens);
    if (r < 0) {
        ssp_error("Failed to parse JSON\n");
        return -1;
    }
    
    for (int i = 1; i < r; i++) {

        int key_size = tok[i].end - tok[i].start;
        int value_size = tok[i+1].end - tok[i+1].start;

        char key[key_size + 1];
        key[key_size] = '\0';
        strncpy(key, &buff[tok[i].start], key_size);

        char value[value_size + 1];
        value[value_size] = '\0';
        strncpy(value, &buff[tok[i+1].start], value_size);

        callback(key, value, params);
        i++;
        
    }
    return 0;
}

static struct params {
    int error;
    int fd;
};

static void write_put_proxy_message(int fd, int *error, Message_put_proxy *proxy_message) {

    char *error_message = "failed to write put proxy message\n";
    int err = ssp_write(fd, &proxy_message->destination_file_name.length, sizeof(uint8_t));
    if (err < 0) {
        *error = err;
        ssp_error(error_message);
        return;
    }
    err = ssp_write(fd, proxy_message->destination_file_name.value, proxy_message->destination_file_name.length);
    if (err < 0) {
        *error = err;
        ssp_error(error_message);
        return;
    }
    err = ssp_write(fd, &proxy_message->source_file_name.length, sizeof(uint8_t));
    if (err < 0) {
        *error = err;
        ssp_error(error_message);
        return;
    }
    err = ssp_write(fd, proxy_message->source_file_name.value, proxy_message->source_file_name.length);
    if (err < 0) {
        *error = err;
        ssp_error(error_message);
        return;
    }
    err = ssp_write(fd, &proxy_message->destination_id.length, sizeof(uint8_t));
    if (err < 0) {
        *error = err;
        ssp_error(error_message);
        return;
    }
    err = ssp_write(fd, proxy_message->destination_id.value, proxy_message->destination_id.length);
    if (error < 0) {
        *error = err;
        ssp_error(error_message);
        return;
    }

}

static void write_message_callback(Node *node, void *element, void *param) {

    struct params *p = (struct params *) param;
    int fd = p->fd;
    if (p->error < 0){
        return;
    }

    Message *message = (Message *)element;
    
    //SEEK_END 2  SEEK_CUR 1  SEEK_SET 0 
    int error = ssp_lseek(fd, 0, SEEK_END);
    if (error < 0) {
        p->error = error;
        ssp_error("failed to locate end\n");
        return;
    }
    //write type
    error = ssp_write(fd, &message->header.message_type, sizeof(uint8_t));
    if (error < 0) {
        p->error = error;
        ssp_error("failed to append to end of file\n");
        return;
    }
    //move file end
    error = ssp_lseek(fd, 0, SEEK_END);
    if (error < 0) {
        p->error = error;
        ssp_error("failed to locate end\n");
        return;
    }
    Message_put_proxy *proxy_message;

    switch (message->header.message_type)
    {
        case PROXY_PUT_REQUEST:
            proxy_message = (Message_put_proxy *)message->value;
            ssp_printf("writing put proxy message\n");
            write_put_proxy_message(fd, &p->error, proxy_message);
            break;
    
        default:
            break;
    }
}


#include <stdio.h>
//work in progress
int save_req(Request *req) {

    char file_name[255];
    snprintf(file_name, 255, "%s%u%s%llu%s", "pending_req_id:", req->dest_cfdp_id, ":num:", req->transaction_sequence_number, ".binary");

    int fd = ssp_open(file_name, O_RDWR | O_CREAT);
    if (fd < 0) {
        ssp_error("couldn't open file\n");
        return -1;
    }
    
    int req_len = sizeof(Request);
    char buff[req_len];

    memcpy(buff, req, req_len);

    int error = ssp_write(fd, buff, req_len);
    if (error == -1) 
        return -1;

    struct params param = {
        0,
        fd
    };

    if (!req->messages_to_user->count)
        return 0;

    error = ssp_write(fd, &req->messages_to_user->count, sizeof(uint8_t));
    if (error == -1) 
        return -1;

    req->messages_to_user->iterate(req->messages_to_user, write_message_callback, &param);
    if (param.error < 0)
        return -1;

    error = ssp_close(fd);
    if (error < 0) {
        ssp_error("couldn't close file descriptor\n");
    }
    return 0;
}

Message *read_in_proxy_message(int fd) {

    uint8_t destination_file_name_len = 0;
    char destination_file_name[255];
    uint8_t src_file_name_len = 0;
    char src_file_name[255];
    uint8_t dest_id_len = 0;
    uint32_t dest_id = 0;

    char *error_message = "failed to read put proxy message\n";
    int err = ssp_read(fd, &destination_file_name_len, sizeof(uint8_t));
    if (err < 0) {
        ssp_error(error_message);
        return NULL;
    }
    printf("destination_file_name_len %d\n", destination_file_name_len);
    
    err = ssp_read(fd, destination_file_name, destination_file_name_len);
    if (err < 0) {
        ssp_error(error_message);
        return NULL;
    }
    err = ssp_read(fd, &src_file_name_len, sizeof(uint8_t));
    if (err < 0) {
        ssp_error(error_message);
        return NULL;
    }
    err = ssp_read(fd, src_file_name, src_file_name_len);
    if (err < 0) {
        ssp_error(error_message);
        return NULL;
    }
    err = ssp_read(fd, &dest_id_len, sizeof(uint8_t));
    if (err < 0) {
        ssp_error(error_message);
        return NULL;
    }
    err = ssp_read(fd, (char *)&dest_id, dest_id_len);
    if (err < 0) {
        ssp_error(error_message);
        return NULL;
    }

    Message *message = create_message(PROXY_PUT_REQUEST);
    if (message == NULL)
        return NULL;

    Message_put_proxy *proxy_message = create_message_put_proxy(dest_id, dest_id_len, src_file_name, destination_file_name);
    if (proxy_message == NULL) {
        ssp_free(message);
        return NULL;
    }
    
    message->value = proxy_message;
    return message;
}

Request *get_req(uint32_t dest_cfdp_id, uint64_t transaction_seq_num) {
    
    char file_name[255];
    uint8_t number_of_messages;

    snprintf(file_name, 255, "%s%u%s%llu%s", "pending_req_id:", dest_cfdp_id, ":num:", transaction_seq_num, ".binary");

    int fd = ssp_open(file_name, O_RDWR | O_CREAT);
    if (fd < 0) {
        ssp_error("couldn't open file\n");
        return NULL;
    }

    int len = sizeof(Request);
    Request req;
    //read in request static data
    int error = ssp_read(fd, (char *)&req, len);
    if (error == -1){
        return NULL;
    }

    //read in count of messages
    error = ssp_read(fd, &number_of_messages, sizeof(uint8_t));
    if (error == -1){
        return NULL;
    }

    List *messages = NULL;
    if (number_of_messages > 0) {
        
        messages = linked_list();
        if (messages == NULL)
            return NULL;

        for (int i = 0; i < number_of_messages; i++) {
                
            uint8_t message_type = 0;
            error = ssp_read(fd, &message_type, sizeof(uint8_t));
            if (error == -1)
                return NULL;
            
            Message *message;
            switch (message_type)
            {
                case PROXY_PUT_REQUEST:
                    ssp_printf("reading put proxy message\n");
                    message = read_in_proxy_message(fd);
                    break;

                default:
                    break;
            }
            messages->push(messages, message, -1);
        }
    }

    Request *r = ssp_alloc(1, sizeof(Request));
    memcpy(r, &req, sizeof(Request));
    r->messages_to_user = messages;
    error = close(fd);
    if (error < 0) {
        ssp_error("couldn't close file descriptor \n");
    }

    return r;
    
}