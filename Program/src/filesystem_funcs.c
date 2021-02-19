/*------------------------------------------------------------------------------
This file is protected under copyright. If you want to use it,
please include this text, that is my only stipulation.  

Author: Evan Giese
------------------------------------------------------------------------------*/
#include "stdint.h"
#include "port.h"
#include "filesystem_funcs.h"
#include "jsmn.h"
#include "requests.h"


uint32_t get_file_size(char *source_file_name) {

    int fd = ssp_open(source_file_name, SSP_O_RDWR);
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
        fd = ssp_open(source_file_name, SSP_O_RDWR | SSP_O_CREAT | SSP_O_TRUNC);
    }else {
        fd = ssp_open(source_file_name, SSP_O_RDWR | SSP_O_CREAT);
    }   
    if (fd == -1){
        ssp_error("couldn't create file\n");
        fd = ssp_open(source_file_name, SSP_O_RDWR);
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

    int fd = ssp_open(source_file_name, SSP_O_RDWR);
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

    if (ssp_lseek(file->fd, offset, SSP_SEEK_SET) == -1){
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

    ssp_lseek(file->fd, (int) offset, SSP_SEEK_SET);
    int bytes = ssp_write(file->fd, buff, (size_t) size);

    if (bytes == -1){
        ssp_error("Could not write\n");
    }
    if (bytes < size && bytes >= 0){
        ssp_error("did not write all the bytes, this could be because the disk is full, or the file that was sent is empty!\n");
    }
    return bytes;
}

void ssp_free_file(void *file) {

    File *f = (File *) file;
    f->missing_offsets->free(f->missing_offsets, ssp_free);
    ssp_free(f);
}



uint32_t calc_check_sum(char *data, uint32_t length) {
    uint8_t remaining_bytes = length % 4;
    uint32_t check_sum = 0;
    uint32_t end = length - 4;
    unsigned int i = 0;
    for (i = 0; i < end; i+= 4){
        check_sum += *((uint32_t *) &data[i]);
    }
    
    if (remaining_bytes){
        uint8_t last_chunk[4];
        memset(last_chunk, 0, 4);

        end = length - remaining_bytes;
        i = 0;
        for (i = 0; i < remaining_bytes; i++) {
            last_chunk[i] = data[end + i];
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
    int i = 0;
    for (i = 0; i < file->total_size; i++) {
        
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

    int number_of_tokens = 255;
    jsmn_parser p;
    jsmn_init(&p);

    jsmntok_t tok[255];

    uint32_t total_size = get_file_size(file_name);

    if (total_size == 0){
        ssp_error("couldn't get file size\n");
        return -1;
    }

    char *buff = ssp_alloc(total_size, sizeof(char));
    if (buff == NULL) 
        return -1;

    int fd = ssp_open(file_name, SSP_O_RDWR);
    if (fd < 0) {
        ssp_free(buff);
        ssp_error("couldn't open file\n");
        return -1;
    }
    
    int r = ssp_read(fd, buff, total_size);
    if (r < 0) {
        ssp_free(buff);
        ssp_error("read failed\n");
        return -1;
    }

    r = jsmn_parse(&p, buff, total_size, tok, number_of_tokens);
    if (r < 0) {
        ssp_free(buff);
        ssp_error("Failed to parse JSON\n");
        return -1;
    }
    int i = 1;
    for (i = 1; i < r; i++) {

        int key_size = tok[i].end - tok[i].start;
        int value_size = tok[i+1].end - tok[i+1].start;

        //char key[key_size + 1];
        char *key = ssp_alloc(key_size + 1, sizeof(char));
        if (key == NULL) {
            ssp_free(buff);
            return -1;
        }
            
        key[key_size] = '\0';
        strncpy(key, &buff[tok[i].start], key_size);

        char *value = ssp_alloc(value_size + 1, sizeof(char));
        if (value == NULL) {
            ssp_free(buff);
            ssp_free(value);
            return -1;
        }

        value[value_size] = '\0';
        strncpy(value, &buff[tok[i+1].start], value_size);

        callback(key, value, params);
        ssp_free(key);
        ssp_free(value);
        i++;
    }

    ssp_free(buff);

    return 0;
}



static void save_file_callback(Node *node, void *element, void *param) {

    char *error_message = "failed to write offset\n";

    struct params *p = (struct params *) param;
    if (p->error < 0){
        return;
    }

    Offset *offset = (Offset *)element;
    
    int err = ssp_write(p->fd, offset, sizeof(Offset));
    if (err < 0) {
        ssp_error(error_message);
        p->error = err;
        return;
    }
}

/*
    first write file,
    then length of offsets,
    then offsets
*/
int save_file_to_file(int fd, File *file) {

    char *error_message = "failed to write file\n";

    int err = ssp_write(fd, file, sizeof(File));
    if (err < 0) {
        ssp_error(error_message);
        return -1;
    }

    err = ssp_write(fd, &file->missing_offsets->count, sizeof(uint32_t));
    if (err < 0) {
        ssp_error(error_message);
        return -1;
    }

    struct params param = {
        0,
        fd
    };

    file->missing_offsets->iterate(file->missing_offsets, save_file_callback, &param);      
    return param.error;  
}
//[file][length][offsets]
int get_file_from_file(int fd, File *file) {

    uint32_t length = 0;
    char *error_msg = "read failed meta data\n";
    int r = ssp_read(fd, (char *)file, sizeof(File));
    if (r < 0) {
        ssp_error(error_msg);
        return -1;
    }

    r = ssp_read(fd, (char *)&length, sizeof(uint32_t));
    if (r < 0) {
        ssp_error(error_msg);
        return -1;
    }
    Offset offset;
    memset(&offset, 0, sizeof(Offset));

    List *missing_offsets = linked_list();
    if (missing_offsets == NULL) {
        return -1;
    }
    int i = 0;
    for (i = 0; i < length; i++) {

        r = ssp_read(fd, (char*)&offset, sizeof(Offset));
        
        if (r < 0) {
            ssp_error(error_msg);
            return -1;
        }

        Offset *new_offset = ssp_alloc(1, sizeof(Offset));
        if (new_offset == NULL) {
            missing_offsets->free(missing_offsets, ssp_free);
            return -1;
        }

        ssp_memcpy(new_offset, &offset, sizeof(Offset));
        missing_offsets->push(missing_offsets, new_offset, -1);
    }

    file->missing_offsets = missing_offsets;
    return 0;
}

int write_lv(int fd, LV lv){

    char *error_message = "failed to write lv\n";
    int err = ssp_write(fd, (char*)&lv.length, sizeof(uint8_t));
    if (err < 0) {
        ssp_error(error_message);
        return -1;
    }
    err = ssp_write(fd, (char*)lv.value, lv.length);
    if (err < 0) {
        ssp_error(error_message);
        return -1;
    }
    return 1;
}
int read_lv(int fd, LV *lv){

    char *error_message = "failed to read lv\n";
    int err = ssp_read(fd, (char*)&lv->length, sizeof(uint8_t));
    if (err < 0) {
        ssp_error(error_message);
        return -1;
    }
    lv->value = ssp_alloc(lv->length, sizeof(uint8_t));
    if(lv->value == NULL) {
        return -1;
    }
    err = ssp_read(fd, (char*)lv->value, lv->length);
    if (err < 0) {
        ssp_error(error_message);
        return -1;
    }
    return 1;
}

//length
//dest_file
//length
//src_file

static void write_put_proxy_message(int fd, int *error, Message_put_proxy *proxy_message) {

    char *error_message = "failed to write put proxy message\n";
    
    int err = write_lv(fd, proxy_message->destination_file_name);
    if (err < 0) {
        *error = err;
        ssp_error(error_message);
        return;
    }
    
    err = write_lv(fd, proxy_message->source_file_name);
    if (err < 0) {
        *error = err;
        ssp_error(error_message);
        return;
    }

    err = write_lv(fd, proxy_message->destination_id);
    if (err < 0) {
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
    
    //write type
    int error = ssp_write(fd, &message->header.message_type, sizeof(uint8_t));
    if (error < 0) {
        p->error = error;
        ssp_error("failed to append to end of file\n");
        return;
    }
    Message_put_proxy *proxy_message;

    switch (message->header.message_type)
    {
        case PROXY_PUT_REQUEST:
            proxy_message = (Message_put_proxy *)message->value;
            //ssp_printf("writing put proxy message\n");
            write_put_proxy_message(fd, &p->error, proxy_message);
            break;
    
        default:
            break;
    }
}


static int write_file_present_bool(int fd, File *file) {
    int error = 0;
    bool file_is_present = true;
    bool file_is_not_present = false;

    if (file != NULL) {
        error = ssp_write(fd, &file_is_present, sizeof(bool));
        if (error == -1) 
            return -1;
    } else {
        error = ssp_write(fd, &file_is_not_present, sizeof(bool));
        if (error == -1) 
            return -1;
    }
    return error;
}

static int get_file_name(char *filename, uint32_t dest_cfdp_id, uint32_t cfdp_id, uint64_t trans) {

    char dir_name[MAX_PATH];
    ssp_snprintf(dir_name, MAX_PATH, "%s%u%s", "incomplete_requests/CFID:", dest_cfdp_id, "_requests");

    int error = ssp_mkdir(dir_name);
    if (error < 0)
        return -1;

    ssp_snprintf(filename, MAX_PATH, "%s%u%s%u%s%u%s%llu%s", "incomplete_requests/CFID:", dest_cfdp_id, "_requests/dest_id:", dest_cfdp_id,":cfdp_id:", cfdp_id, ":trans:", trans, ".request");

    return 1;
}

int delete_saved_request(Request *req) {
    char file_name[MAX_PATH];
    get_file_name(file_name, req->dest_cfdp_id, req->my_cfdp_id, req->transaction_sequence_number);
    ssp_printf("deleting %s\n", file_name);
    int error = ssp_remove(file_name);
    return error;
}

//work in progress
//[REQ][IS_FILE_PRESENT][FILE][MESSAGE_LENGTH][MESSAGES]
int save_req_to_file(Request *req) {

    char file_name[255];
    get_file_name(file_name, req->dest_cfdp_id, req->my_cfdp_id, req->transaction_sequence_number);
   
    int fd = ssp_open(file_name, SSP_O_RDWR | SSP_O_CREAT);
    if (fd < 0) {
        ssp_error("couldn't open file\n");
        return -1;
    }
    
    //writing request struct
    int error = ssp_write(fd, req, sizeof(Request));
    if (error == -1) {
        ssp_printf("couldnt write request struct\n");
        return -1;
    }

    struct params param = {
        0,
        fd
    };

    //writing is file present
    error = write_file_present_bool(fd, req->file);
    if (error < 0){
        ssp_printf("couldnt write bool\n");
        return -1;
    }
        
    //writing file
    if (req->file != NULL) {
        error = save_file_to_file(fd, req->file);
        if (error < 0) {
            ssp_printf("couldnt write file\n");
            return -1;
        }
    }

    //writing message count
    error = ssp_write(fd, &req->messages_to_user->count, sizeof(uint8_t));
    if (error == -1) {
        ssp_printf("couldnt write message count\n");
        return -1;
    }

    if (!req->messages_to_user->count)
        return 0;

    //writing messages
    req->messages_to_user->iterate(req->messages_to_user, write_message_callback, &param);
    if (param.error < 0) {
        ssp_printf("couldnt write messsages\n");
        return -1;
    }

    error = ssp_close(fd);
    if (error < 0) {
        ssp_error("couldn't close file descriptor\n");
        return -1;
    }
    return 0;
}

static Message *read_in_proxy_message(int fd) {


    char *error_message = "failed to read put proxy message\n";

    LV destination_file;
    LV src_file_name;
    LV dest_id;

    int err = read_lv(fd, &destination_file);
    if (err < 0) {
        ssp_error(error_message);
        return NULL;
    }
    err = read_lv(fd, &src_file_name);
    if (err < 0) {
        ssp_error(error_message);
        return NULL;
    }
    err = read_lv(fd, &dest_id);
    if (err < 0) {
        ssp_error(error_message);
        return NULL;
    }

    Message *message = create_message(PROXY_PUT_REQUEST);
    if (message == NULL)
        return NULL;

    Message_put_proxy *proxy_message = ssp_alloc(1, sizeof(Message_put_proxy));
    if (proxy_message == NULL) {
        ssp_free(message);
        return NULL;
    }
    proxy_message->destination_file_name = destination_file;
    proxy_message->source_file_name = src_file_name;
    proxy_message->destination_id = dest_id;
    
    message->value = proxy_message;
    return message;
}

static int get_messages_from_file(int fd, List *messages){

    uint8_t number_of_messages;
    int error = ssp_read(fd, (char *) &number_of_messages, sizeof(uint8_t));
    if (error == -1){
        return -1;
    }

    if (number_of_messages > 0) {
        int i = 0;
        for (i = 0; i < number_of_messages; i++) {
                
            uint8_t message_type = 0;
            error = ssp_read(fd, (char *) &message_type, sizeof(uint8_t));
            if (error == -1)
                return -1;
            
            Message *message;
            switch (message_type)
            {
                case PROXY_PUT_REQUEST:
                    message = read_in_proxy_message(fd);
                    break;
                case CONTINUE_PARTIAL:
                    ssp_printf("this type of message is not implmeneted yet\n");
                    continue;
                default:
                    ssp_printf("failed to read in message, no known message type\n");
                    continue;
            }
            message->header.message_type = message_type;
            messages->push(messages, message, -1);
        }
    }
    return 0;
}

int read_request_from_file(int fd, Request *req){

    //will overwrite messages pointer, so we need to save it
    List *messages = req->messages_to_user;
    int timeout_before_cancel = req->timeout_before_cancel;
    int timeout_before_journal = req->timeout_before_journal;
    void *addr = req->res.addr;

    //read in request struct
    int error = ssp_read(fd, (char *)req, sizeof(Request));
    if (error == -1)
        return -1;
    
    //check to see if file is present
    bool is_file_present = false;
    error = ssp_read(fd, (char *)&is_file_present, sizeof(bool));
    if (error == -1)
        return -1;
    
    File *file = NULL;
    if (is_file_present) {
        file = ssp_alloc(1, sizeof(File));
        if (file == NULL) {
            return -1;
        }
        error = get_file_from_file(fd, file);
        if (error < 0) {
            ssp_free(file);
            return -1;
        }
        req->file = file;
    }

    req->messages_to_user = messages;
    req->timeout_before_cancel = timeout_before_cancel;
    req->timeout_before_journal = timeout_before_journal;
    req->res.addr = addr;

    error = get_messages_from_file(fd, req->messages_to_user);
    if (error == -1) {
        ssp_free(file);
        return -1;
    }

    return error;
}

//[REQ][IS_FILE_PRESENT][FILE][MESSAGE_LENGTH][MESSAGES]
int get_req_from_file(uint32_t dest_cfdp_id, uint64_t transaction_seq_num, uint32_t my_cfdp_id, Request *req) {
    
    char file_name[255];
    
    get_file_name(file_name, dest_cfdp_id, my_cfdp_id, transaction_seq_num);    
    ssp_printf("opening %s\n", file_name);

    int fd = ssp_open(file_name, SSP_O_RDWR);
    if (fd < 0) {
        ssp_error("couldn't open file\n");
        return -1;
    }
    int error = read_request_from_file(fd, req);
    if (error < 0) {
        return -1;
    }
    error = ssp_close(fd);
    if (error < 0) {
        ssp_error("couldn't close file descriptor \n");
    }
    
    return 0;
}
