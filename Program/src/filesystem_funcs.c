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
#include "utils.h"

int get_file_size(char *source_file_name) {

    int fd = ssp_open(source_file_name, SSP_O_RDWR);
    if (fd == -1){
        ssp_error("could not open file\n");
        return -1;
    }

    int bytes = ssp_lseek(fd, 0, 2);
    if (bytes == -1){
        ssp_error("could not seek file for file size\n");
        return -1;
    }

    ssp_lseek(fd, 0, 0);

    if (ssp_close(fd) == -1){
        ssp_error("could not close file\n");
        return -1;
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
        ssp_printf("ERROR: couldn't open file\n");
        return -1;
    }
    if (ssp_close(fd) == -1){
        ssp_printf("ERROR: couldn't close file\n");
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
    } else if (bytes == 0) {
        ssp_error("Bytes read 0\n");    
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
    uint32_t bytes_to_add = 0;

    for (i = 0; i < end; i+= 4){
        bytes_to_add = ssp_htonl(*(uint32_t *) &data[i]);
        check_sum += bytes_to_add;
            }
    
    if (remaining_bytes){
        uint8_t last_chunk[4];
        memset(last_chunk, 0, 4);

        end = length - remaining_bytes;
        i = 0;
        for (i = 0; i < remaining_bytes; i++) {
            last_chunk[i] = data[end + i];
        } 
          
        check_sum += ssp_htonl(*((uint32_t*) &last_chunk));
               
    }

    return check_sum;
}


//stack buffer is the size of the packet length
uint32_t check_sum_file(File *file, uint16_t stack_buffer) {

    char buff[stack_buffer];
    uint32_t checksum = 0;
    uint32_t bytes_read = 0;
    int i = 0;

    for (i = 0; i < file->total_size; i+= stack_buffer) {
        
        bytes_read = get_offset(file, buff, stack_buffer, i);
        if (bytes_read > 0)
            checksum += calc_check_sum(buff, bytes_read);
    }

    return checksum;
}

static int find_nak(void *element, void* args) {

    Offset *offset_in_list = (Offset *) element;
    Offset *offset_to_insert = (Offset *) args;

    if (offset_to_insert->start >= offset_in_list->start \
    && offset_to_insert->start <= offset_in_list->end \
    && offset_to_insert->end <= offset_in_list->end \
    && offset_to_insert->end >= offset_in_list->start) { 
        return 1;
    }
    return 0;
}

//add_first_offset should be in create file
int receive_offset(File *file, uint32_t offset_start, uint32_t offset_end) {
    
    List * nak_list = file->missing_offsets;

    Offset offset_to_insert;
    offset_to_insert.start = offset_start;
    offset_to_insert.end = offset_end;

    //iterate through the list, and return the list node
    Node *node = nak_list->findNode(nak_list, -1, find_nak, &offset_to_insert);

    if (node == NULL){
        ssp_printf("offset already received, can't add new offset:%u end:%u\n", offset_start, offset_end);
        return 0; 
    }

    Offset *offset_in_list = (Offset *) node->element;

    //remove node if both start and end are equal (remove at function)
    if (offset_start == offset_in_list->start && offset_end == offset_in_list->end) {
        node->next->prev = node->prev;
        node->prev->next = node->next;
        ssp_free(node->element);
        ssp_free(node);
        nak_list->count--;
        return 1;
    
    //if new offset is in the start, but the end of the offset is smaller than list's end, change the list's node's start
    } else if (offset_start == offset_in_list->start && offset_start < offset_in_list->end) {
        offset_in_list->start = offset_end;
        return 1;

    //if offset is at the list end, but the start of the offset is larger than list's start, move previous end down
    } else if (offset_end == offset_in_list->end && offset_start > offset_in_list->start) {
        offset_in_list->end = offset_start;
        return 1;
    }
    //if the offset is inbetween the 'start' and the 'end' offsets in the list, add new offset, and make the 
    //end of the lists first offset the start of the incoming offset, and the start of the second, the end of the incoming 
    Offset *new_offset = ssp_alloc(1, sizeof(Offset));
    if (new_offset == NULL) {
        return -1;
    }

    new_offset->start = offset_end;
    new_offset->end = offset_in_list->end;
    offset_in_list->end = offset_start;

    
    Node *cur = node;
    Node *ne = createNode(new_offset, new_offset->start);
    if (ne == NULL) {
        ssp_free(new_offset);
        return 0;
    }

    ne->next = cur->next;
    ne->prev = cur;
    cur->next = ne;
    ne->next->prev = ne;

    nak_list->count++;
        
    return 1;

}

int add_first_offset(File *file, uint32_t file_size){
    Offset *offset = ssp_alloc(1, sizeof(Offset));
    if (offset == NULL)
        return -1;

    offset->end = file_size;
    offset->start = 0;
    file->missing_offsets->insert(file->missing_offsets, offset, file_size);
    return 1;
}

File *create_temp_file(char *file_name, uint32_t size) {
    File *file = create_file(file_name, 1);
    file->is_temp = 1;
    file->total_size = size;

    ssp_printf("mode acknowledged, building offset map\n");
    int error =  add_first_offset(file, size);
    if (error < 0) {
        ssp_free_file(file);
        return NULL;
    }
    return file;
}


static int print_nak(void *element, void* args) {

    Offset *offset_in_list = (Offset *) element;
    ssp_printf("start: %u, end: %u\n", offset_in_list->start, offset_in_list->end);
    return 0;
}

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

int read_json(char *file_name, int (*callback)(char *key, char *value, void *params), void *params) {

    int number_of_tokens = 255;
    jsmn_parser p;
    jsmn_init(&p);
    int error = 0;

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

        error = callback(key, value, params);

        ssp_free(key);
        ssp_free(value);
        i++;

        if (error < 0) {
            return error;
        }
        
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

int write_id(int fd, uint64_t id){

    char *error_message = "failed to write id\n";
    uint64_t network_byte_order = 0;
    network_byte_order = ssp_htonll(id);

    int err = ssp_write(fd, &network_byte_order, sizeof(uint64_t));
    if (err < 0) {
        ssp_error(error_message);
        return -1;
    }
    return 1;
}

int read_id(int fd, uint64_t *id){

    char *error_message = "failed to write id\n";
    uint64_t host_byte_order = 0;

    int err = ssp_read(fd, &host_byte_order, sizeof(uint64_t));
    if (err < 0) {
        ssp_error(error_message);
        return -1;
    }

    *id = ssp_ntohll(host_byte_order);
    return 1;
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

    err = write_id(fd, proxy_message->destination_id);
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

    ssp_snprintf(filename, MAX_PATH, "%s%u%s%u%s%u%s%lu%s", "incomplete_requests/CFID:", dest_cfdp_id, "_requests/dest_id:", dest_cfdp_id,":cfdp_id:", cfdp_id, ":trans:", trans, ".request");

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
    uint64_t dest_id;

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
    err = read_id(fd, &dest_id);
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


//NEEEEEEEEEEEEW stuff

/*
//incoming requests spin up requests
typedef struct request {
    Request_procedure procedure;

    uint64_t transaction_sequence_number;
    uint32_t dest_cfdp_id;
    uint32_t my_cfdp_id;

    File *file;
    uint32_t file_size;

    char source_file_name[MAX_PATH];
    char destination_file_name[MAX_PATH];

    //previous timeout time in seconds
    int timeout_before_cancel;
    int timeout_before_journal;
    
    uint8_t segmentation_control;
    uint8_t fault_handler_overides;
    uint8_t flow_lable;
    uint8_t transmission_mode;
    bool paused;

    //counter for resending eof packets == 3
    uint8_t resent_eof;
    //counter for resending finished packets == 3
    uint8_t resent_finished;

    //bool for sending first blast of data packets
   // uint8_t sent_first_data_round;

    Remote_entity remote_entity;
    Local_entity local_entity;

    List *messages_to_user;
    
    //sets the buffer length, for making packets
    uint32_t buff_len;

    //buffer for making packets
    char* buff;

    //header for building response packets
    Pdu_header pdu_header;

    //handler for sending responses back
    Response res;
} Request;

typedef struct file {
    int fd;
    uint8_t is_temp;
    uint32_t next_offset_to_send;
    uint32_t total_size;
    uint32_t partial_checksum;
    uint32_t eof_checksum;
    List *missing_offsets;

} File;
*/


static int parse_json_missing_offset_list(List *list, char *value) {

    int i = 0;
    char *parse_string = (char *) value;
    int len = strnlen(parse_string, 10000);

    int value_length = 15;
    char tmp_offset[value_length];
    char start[value_length];
    int value_index = 0;

    Offset *offset;
    int comma_number = 0;

    //add comma at end for algorithm
    parse_string[len-1] = ',';

    for (i = 1; i < len; i++) {

        if (parse_string[i] == ',') {
            tmp_offset[value_index] = '\0';
            value_index = 0;
            comma_number++;

            if (comma_number % 2 == 0) {
                offset = ssp_alloc(1, sizeof(Offset));
                if (offset == NULL) {
                    ssp_printf("memory allocation failed\n");
                    return -1;
                }
                offset->start = ssp_atol(start);
                offset->end = ssp_atol(tmp_offset);

                list->push(list, offset, -1);
                continue;
            }

            memcpy(start, tmp_offset, value_length);
            continue;
        } 

        tmp_offset[value_index] = parse_string[i];
        value_index++;

    }
    return 0;
}



enum {
    REQ_my_cfdp_id,
    REQ_dest_cfdp_id,
    REQ_transaction_sequence_number,
    REQ_file_size,
    REQ_source_file_name,
    REQ_destination_file_name,
    REQ_transmission_mode,
    REQ_paused,
    REQ_resent_eof,
    REQ_resent_finished,
    REQ_sent_first_data_round,
    REQ_local_entity_EOF_sent_indication,
    REQ_local_entity_EOF_recv_indication,
    REQ_local_entity_file_segment_recv_indication,
    REQ_local_entity_transaction_finished_indication,
    REQ_local_entity_suspended_indication,
    REQ_local_entity_resumed_indication,
    REQ_local_entity_Metadata_recv_indication,
    REQ_local_entity_Metadata_sent_indication,
    REQ_messages_to_user,
    REQ_file_is_temp,
    REQ_file_next_offset_to_send,
    REQ_file_total_size,
    REQ_file_partial_checksum,
    REQ_file_eof_checksum,
    REQ_file_missing_offsets,
    REQ_TOTAL
};

static char *parse_list[REQ_TOTAL] = {   
    "my_cfdp_id",
    "dest_cfdp_id",
    "transaction_sequence_number",
    "file_size",
    "source_file_name",
    "destination_file_name",
    "transmission_mode",
    "paused",
    "resent_eof",
    "resent_finished",
    "sent_first_data_round",
    "local_entity.EOF_sent_indication",
    "local_entity.EOF_recv_indication",
    "local_entity.file_segment_recv_indication",
    "local_entity.transaction_finished_indication",
    "local_entity.suspended_indication",
    "local_entity.resumed_indication",
    "local_entity.Metadata_recv_indication",
    "local_entity.Metadata_sent_indication",
    "messages_to_user",
    "file.is_temp",
    "file.next_offset_to_send",
    "file.total_size",
    "file.partial_checksum",
    "file.eof_checksum",
    "file.missing_offsets"
};


static int parse_json_request(char *key, char *value, void *params) {

    int len = 0;
    int i = 0;
    int error = 0;

    Request *req = (Request *) params;

    for (i = 0; i < REQ_TOTAL; i++) {
        len = strnlen(parse_list[i], 50);
        
        //ssp_printf("parsing %s\n", parse_list[i]);
        if (strncmp(key, parse_list[i], len) != 0)
            continue;

        switch (i)
        {
            case REQ_my_cfdp_id:
                req->my_cfdp_id = ssp_atol(value);
                break;
            case REQ_dest_cfdp_id:
                req->dest_cfdp_id = ssp_atol(value);
                break;
            case REQ_transaction_sequence_number:
                req->transaction_sequence_number = ssp_atoll(value);
                break;
            case REQ_file_size:
                req->file_size = ssp_atol(value);
                break;
            case REQ_source_file_name:
                //req->my_cfdp_id = ssp_atol(value);
                break;
            case REQ_destination_file_name:
                //req->my_cfdp_id = ssp_atol(value);
                break;
            case REQ_transmission_mode:
                req->transmission_mode = ssp_atol(value);
                break;
            case REQ_paused:
                req->paused = ssp_atol(value);
                break;
            case REQ_resent_eof:
                req->resent_eof = ssp_atol(value);
                break;
            case REQ_resent_finished:
                req->resent_finished = ssp_atol(value);
                break;
            case REQ_sent_first_data_round:
                req->sent_first_data_round = ssp_atol(value);
                break;
            case REQ_local_entity_EOF_sent_indication:
                req->local_entity.EOF_sent_indication = ssp_atol(value);
                break;
            case REQ_local_entity_EOF_recv_indication:
                req->local_entity.EOF_recv_indication = ssp_atol(value);
                break;
            case REQ_local_entity_file_segment_recv_indication:
                req->local_entity.file_segment_recv_indication = ssp_atol(value);
                break;
            case REQ_local_entity_transaction_finished_indication:
                req->local_entity.transaction_finished_indication = ssp_atol(value);
                break;
            case REQ_local_entity_suspended_indication:
                req->local_entity.suspended_indication = ssp_atol(value);
                break;
            case REQ_local_entity_resumed_indication:
                req->local_entity.resumed_indication = ssp_atol(value);
                break;
            case REQ_local_entity_Metadata_recv_indication:
                req->local_entity.Metadata_recv_indication = ssp_atol(value);
                break;
            case REQ_local_entity_Metadata_sent_indication:
                req->local_entity.Metadata_sent_indication = ssp_atol(value);
                break;
            case REQ_messages_to_user:
                //req->messages_to_user = ssp_atol(value);
                break;
            case REQ_file_is_temp:
                req->file->is_temp = ssp_atol(value);
                break;
            case REQ_file_next_offset_to_send:
                req->file->next_offset_to_send = ssp_atol(value);
                break;
            case REQ_file_total_size:
                req->file->total_size = ssp_atol(value);
                break;
            case REQ_file_partial_checksum:
                req->file->partial_checksum = ssp_atol(value);
                break;
            case REQ_file_eof_checksum:
                req->file->eof_checksum = ssp_atol(value);
                break;
            case REQ_file_missing_offsets:
                error = parse_json_missing_offset_list(req->file->missing_offsets, value);
                if (error < 0) {
                    ssp_printf("ERROR %d\n", error);
                    return error;
                }
                break;
            case REQ_TOTAL:
                break;
            default:
                break;
        }
    }
    
    return 0;
}

int get_request_from_json (Request *req, char *file_name) {

    int error = read_json(file_name, parse_json_request, req);

    if (error < 0) {
        ssp_error("json parsing failed\n");
        return -1;
    }

    return 0;
}



static struct json_write_callback {
    int error;
    int fd;
    int bytes_written;
};

static void save_file_callback_json(Node *node, void *element, void *param) {

    char *error_message = "failed to write offset\n";

    struct json_write_callback *p = (struct json_write_callback *) param;
    if (p->error < 0){
        return;
    }

    Offset *offset = (Offset *)element;
    char buff[100];

    int bytes_added = ssp_snprintf(buff, sizeof(buff), "%d,%d,", offset->start, offset->end);

    int err = ssp_write(p->fd, buff, bytes_added);

    if (err < 0) {
        ssp_error(error_message);
        p->error = err;
        return;
    }
    p->bytes_written += err;

}

static int add_file_json(int fd, File *file) {


    char buff[500];
    int size = sizeof(buff);

    int bytes_added = ssp_snprintf(buff, size, "\
    \"%s\":%d,\n\
    \"%s\":%d,\n\
    \"%s\":%d,\n\
    \"%s\":%d,\n\
    \"%s\":%d,\n\
    \"%s\":[",
    parse_list[REQ_file_is_temp], file->is_temp,
    parse_list[REQ_file_next_offset_to_send], file->next_offset_to_send,
    parse_list[REQ_file_total_size], file->total_size,
    parse_list[REQ_file_partial_checksum], file->partial_checksum,
    parse_list[REQ_file_eof_checksum], file->eof_checksum,
    parse_list[REQ_file_missing_offsets]);    

    int bytes = ssp_write(fd, buff, strnlen(buff, sizeof(buff)));
    if (bytes < 0) {
        ssp_printf("couldn't write file\n");
        return bytes;
    }

    struct json_write_callback p = {
        0,
        fd,
        bytes,
    };

    file->missing_offsets->iterate(file->missing_offsets, save_file_callback_json, &p);
    if (p.error < 0) {
        ssp_printf("couldn't write file offsets\n");
        return -1;
    }

    if (ssp_lseek(fd, -1, SSP_SEEK_END) == -1){
        ssp_error("could'nt set offset\n");
    }

    p.bytes_written += ssp_write(fd, "],\n", 3);
    if (bytes < 0) {
        ssp_printf("couldn't write file\n");
        return bytes;
    }

    return p.bytes_written;

}

static int add_end_of_json(int fd) {
 
    if (ssp_lseek(fd, -2, SSP_SEEK_END) == -1){
        ssp_error("could'nt set offset\n");
    }

    int err = ssp_write(fd, "\n\
}", 2);
    if (err < 0) {
        return -1;
    }
    return 1;
}

static int add_base_req_json(int fd, Request *req){

    char buff[1000];
    int size = sizeof(buff);

    int bytes_added = ssp_snprintf(buff, size, "{\n\
    \"%s\":%d,\n\
    \"%s\":%d,\n\
    \"%s\":%llu,\n\
    \"%s\":%d,\n\
    \"%s\":\"%s\",\n\
    \"%s\":\"%s\",\n\
    \"%s\":%d,\n\
    \"%s\":%d,\n\
    \"%s\":%d,\n\
    \"%s\":%d,\n\
    \"%s\":%d,\n\
    \"%s\":%d,\n\
    \"%s\":%d,\n\
    \"%s\":%d,\n\
    \"%s\":%d,\n\
    \"%s\":%d,\n\
    \"%s\":%d,\n\
    \"%s\":%d,\n\
    \"%s\":%d,\n\
    \"%s\":[],\n",
    parse_list[REQ_my_cfdp_id], req->my_cfdp_id,
    parse_list[REQ_dest_cfdp_id], req->dest_cfdp_id,
    parse_list[REQ_transaction_sequence_number], req->transaction_sequence_number,
    parse_list[REQ_file_size], req->file_size,

    parse_list[REQ_source_file_name], req->source_file_name,
    parse_list[REQ_destination_file_name], req->destination_file_name,
    parse_list[REQ_transmission_mode], req->transmission_mode,
    parse_list[REQ_paused], req->paused,
    parse_list[REQ_resent_eof], req->resent_eof,
    parse_list[REQ_resent_finished], req->resent_finished,
    parse_list[REQ_sent_first_data_round], req->sent_first_data_round,
    parse_list[REQ_local_entity_EOF_sent_indication], req->local_entity.EOF_sent_indication,
    parse_list[REQ_local_entity_EOF_recv_indication], req->local_entity.EOF_recv_indication,
    parse_list[REQ_local_entity_file_segment_recv_indication], req->local_entity.file_segment_recv_indication,
    parse_list[REQ_local_entity_transaction_finished_indication], req->local_entity.transaction_finished_indication,
    parse_list[REQ_local_entity_suspended_indication], req->local_entity.suspended_indication,
    parse_list[REQ_local_entity_resumed_indication], req->local_entity.resumed_indication,
    parse_list[REQ_local_entity_Metadata_recv_indication], req->local_entity.Metadata_recv_indication,
    parse_list[REQ_local_entity_Metadata_sent_indication], req->local_entity.Metadata_sent_indication,
    parse_list[REQ_messages_to_user]
    );


    int bytes = ssp_write(fd, buff, strnlen(buff, sizeof(buff)));
    if (bytes < 0) {
        ssp_printf("couldn't write file\n");
        return bytes;
    }

    return bytes;
}

int write_request_json (Request *req, char *file_name) {

    if (req == NULL) {
        return -1;
    }
    
    int fd = ssp_open(file_name, SSP_O_RDWR | SSP_O_CREAT | SSP_O_TRUNC);
    if (fd == -1) {
        ssp_error("count not open file\n");
        return fd;
    }

    int bytes_added = add_base_req_json(fd, req);

    if (req->file != NULL)
        bytes_added += add_file_json(fd, req->file);

    
    add_end_of_json(fd);

}