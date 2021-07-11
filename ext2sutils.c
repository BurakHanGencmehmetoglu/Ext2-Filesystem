#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <limits.h>
#include <stdbool.h>
#include "ext2fs.h"


unsigned int number_of_deallocated_blocks = 0;
unsigned int block_number = 0;
unsigned int first_data_block_number = 0;
unsigned int i_node_size = 0;
unsigned int i_node_per_group = 0;
unsigned int block_per_group = 0;
unsigned int block_size = 0;
unsigned int group_count = 0;
unsigned int allocated_data_block_for_entry = -1;
struct ext2_block_group_descriptor* group_descriptor_table;
unsigned char* block_bitmap;
unsigned char* inode_bitmap;
int fd;
char destination_file_name[PATH_MAX];
bool decrement_block_number = false;
bool increment_inode_number = false;
struct timeval time;



bool isNumber(char number[]);
void fetch_inode_from_table(int inode_number,struct ext2_inode* i_node);
void write_inode(int inode_number,struct ext2_inode* i_node);
uint32_t get_source_inode_from_path(char* path,int source_or_destination); 



bool free_inode(int inode_number);
void increment_reference_counters(uint32_t direct_blocks[EXT2_NUM_DIRECT_BLOCKS]);
void increment_reference_counter(int block_number);
void insert_entry_to_the_directory(int directory_inode,int reserved_inode); 
bool allocate_new_block(int block_number); 
int padding_length_calculator(int name_length);
void decrement_inode_from_group(int inode_number);


int get_file_inode_number_from_dir(int dir_inode,char* file_name);
void delete_dir_entry_from_inode(struct ext2_inode* i_node,int data_block_number,int location_of_entry);
void delete_inode(int inode_number);
void delete_inode_from_bitmap(int inode_number); 
void decrement_reference_counters(uint32_t direct_blocks[EXT2_NUM_DIRECT_BLOCKS]);
void deallocate_block_number(int block_number);
void increment_inode_number_from_group(int inode_number);





// Below functions are general use functions.
bool isNumber(char number[]) {
    int i = 0;
    if (number[0] == '-')
        i = 1;
    for (; number[i] != 0; i++)
    {
        if (!isdigit(number[i]))
            return false;
    }
    return true;
}



void fetch_inode_from_table(int inode_number,struct ext2_inode* i_node)  {
    int ds_table_index = 0;
    if (inode_number % i_node_per_group == 0) 
        ds_table_index = (inode_number / i_node_per_group) - 1;
    else 
        ds_table_index = inode_number / i_node_per_group;
    if (ds_table_index < 0) 
        printf("DS table index is less than zero. Look at fetch inode function.\n");

    int relative_offset = inode_number % i_node_per_group == 0 ? i_node_per_group - 1 : (inode_number % i_node_per_group) - 1; 
    int seek_value = group_descriptor_table[ds_table_index].inode_table * block_size + relative_offset * i_node_size;
    lseek(fd,seek_value, SEEK_SET);                                                                   
    read(fd,i_node,sizeof(struct ext2_inode));    
    return;
}



void write_inode(int inode_number,struct ext2_inode* i_node) {
    int ds_table_index = 0;
    if (inode_number % i_node_per_group == 0) 
        ds_table_index = (inode_number / i_node_per_group) - 1;
    else 
        ds_table_index = inode_number / i_node_per_group;
    if (ds_table_index < 0) 
        printf("DS table index is less than zero. Look at fetch inode function.\n");
    int relative_offset = inode_number % i_node_per_group == 0 ? i_node_per_group - 1 : (inode_number % i_node_per_group) - 1; 
    int seek_value = group_descriptor_table[ds_table_index].inode_table * block_size + relative_offset * i_node_size;
    lseek(fd,seek_value, SEEK_SET);                                                                   
    write(fd,i_node,sizeof(struct ext2_inode));    
    return;
}



uint32_t get_source_inode_from_path(char* path,int source_or_destination) {
    char** file_names;
    char* token;
    char temp[PATH_MAX];
    int number_of_files = 0;

    strcpy(temp,path);
 
    token = strtok(path, "/");
    while( token != NULL ) {
        number_of_files++;
        token = strtok(NULL, "/");
    }

    file_names = malloc(sizeof(char *)*number_of_files);
    for (int i=0;i<number_of_files;i++)
        file_names[i] = malloc(sizeof(char)*PATH_MAX);


    int i = 0; 
    token = strtok(temp, "/");
    while( token != NULL ) {
        strcpy(file_names[i],token);
        i++;
        token = strtok(NULL, "/");
    }

    struct ext2_inode i_node;
    struct ext2_dir_entry* directory_entry;
    unsigned char* data_block;
    data_block = malloc(block_size);


    // Find source inode .
    if (source_or_destination == 0) {
        int j=0;
        int readed_bytes = 0;
        char file_name[EXT2_MAX_NAME_LENGTH+1];
        fetch_inode_from_table(EXT2_ROOT_INODE,&i_node); // Read root inodes.
        label:
            for (int i=0;i<12;i++) {
                if (i_node.direct_blocks[i] != 0) {
                    lseek(fd,i_node.direct_blocks[i] * block_size,SEEK_SET);
                    read(fd,data_block,block_size);
                    readed_bytes = 0;
                    directory_entry = (struct ext2_dir_entry *) data_block;
                    while (readed_bytes < block_size) {
                        if (directory_entry->inode != 0) {
                            memcpy(file_name, directory_entry->name, directory_entry->name_length);
                            file_name[directory_entry->name_length] = 0;     
                            if (!strcmp(file_name,file_names[j])) {
                                if (j == number_of_files-1) {
                                    for (int i=0;i<number_of_files;i++) 
                                        free(file_names[i]);                                    
                                    free(data_block);
                                    free(file_names);
                                    return directory_entry->inode;
                                } 
                                else {
                                    j++;
                                    fetch_inode_from_table(directory_entry->inode,&i_node);
                                    goto label;
                                }                   
                            }
                        }
                        readed_bytes +=  directory_entry->length;
                        directory_entry = (void*) directory_entry + directory_entry->length; 
                    }
                }
            }
        free(data_block);
        free(file_names);
        printf("We could not find source i_node\n");
        return 0;    
    }

    // Find destination inode.
    else {
        strcpy(destination_file_name,file_names[number_of_files-1]);
        if (number_of_files == 1)
            return 2;
        int j=0;
        int readed_bytes = 0;
        char file_name[EXT2_MAX_NAME_LENGTH+1];
        fetch_inode_from_table(EXT2_ROOT_INODE,&i_node); // Read root inodes.
        label2:
            for (int i=0;i<12;i++) {
                if (i_node.direct_blocks[i] != 0) {
                    lseek(fd,i_node.direct_blocks[i] * block_size,SEEK_SET);
                    read(fd,data_block,block_size);
                    readed_bytes = 0;
                    directory_entry = (struct ext2_dir_entry *) data_block;
                    while (readed_bytes < block_size) {
                        if (directory_entry->inode != 0) {
                            memcpy(file_name, directory_entry->name, directory_entry->name_length);
                            file_name[directory_entry->name_length] = 0;     
                            if (!strcmp(file_name,file_names[j])) {
                                if (j == number_of_files-2) {
                                    for (int i=0;i<number_of_files;i++) 
                                        free(file_names[i]);
                                    free(file_names);
                                    free(data_block);
                                    return directory_entry->inode;
                                } 
                                else {
                                    j++;
                                    fetch_inode_from_table(directory_entry->inode,&i_node);
                                    goto label2;
                                }                   
                            }
                        }
                        readed_bytes +=  directory_entry->length;
                        directory_entry = (void*) directory_entry + directory_entry->length; 
                    }
                }
            }
        free(data_block);
        free(file_names);
        printf("We could not find destination i_node\n");
        return 0;              
    }
}









// Functions for dup are below.
bool free_inode(int inode_number) {
    int ds_table_index = 0;
    if (inode_number % i_node_per_group == 0) 
        ds_table_index = (inode_number / i_node_per_group) - 1;
    else 
        ds_table_index = inode_number / i_node_per_group;
    if (ds_table_index < 0) 
        printf("DS table index is less than zero. Look at is_node_free function.\n");    

    int seek_value = group_descriptor_table[ds_table_index].inode_bitmap * block_size;
    lseek(fd,seek_value, SEEK_SET);                                                                   
    read(fd,inode_bitmap,block_size);  


    int relative_inode_number = inode_number % i_node_per_group == 0 ? i_node_per_group : inode_number % i_node_per_group ;
    int readed_bits = 0;
    for (int i=0;;i++) {
            unsigned char byte = inode_bitmap[i];
            for(int j = 0; j < 8; j++) {
                readed_bits++;
                if (readed_bits == relative_inode_number) {
                    if (((byte >> j) & 0x01) == 1) 
                        return false;
                    else {
                        byte |= 0x01 << j;
                        inode_bitmap[i] = byte;
                        seek_value = group_descriptor_table[ds_table_index].inode_bitmap * block_size;
                        lseek(fd,seek_value, SEEK_SET);                                                                   
                        write(fd,inode_bitmap,block_size);                          
                        return true;
                    }  
                }
            }
    }
}






void increment_reference_counters(uint32_t direct_blocks[EXT2_NUM_DIRECT_BLOCKS]) {
    refctr_t reference_counter;
    for (int i=0;i<12;i++) {
        if (direct_blocks[i] != 0) {
            int block_number = direct_blocks[i];
            int ds_table_index = 0;
            if (block_number % block_per_group == 0) 
                ds_table_index = block_size == 1024 ? (block_number / block_per_group) - 1 : block_number / block_per_group; 
            else 
                ds_table_index = block_number / block_per_group;
            if (ds_table_index < 0) 
                printf("DS table index is less than zero in increment reference counters function.\n");

            int relative_offset = 0;
            if (block_size == 1024) 
                relative_offset = block_number % block_per_group == 0 ? block_per_group - 1 : (block_number % block_per_group) - 1; 
            else 
                relative_offset = block_number % block_per_group == 0 ? 0 : block_number % block_per_group;
            
            int seek_value = group_descriptor_table[ds_table_index].block_refmap * block_size + relative_offset * sizeof(reference_counter);
            lseek(fd,seek_value, SEEK_SET);
            read(fd,&reference_counter,sizeof(reference_counter));
            reference_counter++;
            lseek(fd,seek_value, SEEK_SET);
            write(fd,&reference_counter,sizeof(reference_counter));
        }
    }
    return;
}






void increment_reference_counter(int block_number) {
    refctr_t reference_counter;
    int ds_table_index = 0;
    if (block_number % block_per_group == 0) 
        ds_table_index = block_size == 1024 ? (block_number / block_per_group) - 1 : block_number / block_per_group; 
    else 
        ds_table_index = block_number / block_per_group;
    if (ds_table_index < 0) 
        printf("DS table index is less than zero in increment reference counters function.\n");

    int relative_offset = 0;
    if (block_size == 1024) 
        relative_offset = block_number % block_per_group == 0 ? block_per_group - 1 : (block_number % block_per_group) - 1; 
    else 
        relative_offset = block_number % block_per_group == 0 ? 0 : block_number % block_per_group;
    
    int seek_value = group_descriptor_table[ds_table_index].block_refmap * block_size + relative_offset * sizeof(reference_counter);
    lseek(fd,seek_value, SEEK_SET);
    read(fd,&reference_counter,sizeof(reference_counter));
    reference_counter++;
    lseek(fd,seek_value, SEEK_SET);
    write(fd,&reference_counter,sizeof(reference_counter));
    return;

}






void insert_entry_to_the_directory(int directory_inode,int reserved_inode) {
    struct ext2_inode i_node;
    struct ext2_dir_entry* directory_entry;
    struct ext2_dir_entry* new_entry;
    unsigned char* data_block;
    


    new_entry = malloc(8 + sizeof(char) * strlen(destination_file_name));
    new_entry->name_length = strlen(destination_file_name);
    new_entry->inode = reserved_inode;
    new_entry->file_type = 1;
    new_entry->length = 8 + new_entry->name_length;

    strncpy(new_entry->name,destination_file_name,strlen(destination_file_name));
    
    

    data_block = malloc(block_size);    
    int readed_bytes = 0;

    fetch_inode_from_table(directory_inode,&i_node);

    for (int i=0;i<12;i++) {
        if (i_node.direct_blocks[i] != 0) {
            lseek(fd,i_node.direct_blocks[i] * block_size,SEEK_SET);
            read(fd,data_block,block_size);
            readed_bytes = 0;
            directory_entry = (struct ext2_dir_entry *) data_block;
            while (readed_bytes < block_size) { 
                // We are adding to the first entry or entirely new block. Either case, we will take old dir entry length.
                if (directory_entry->inode == 0 && directory_entry->length >= new_entry->length) {
                    new_entry->length = directory_entry->length;
                    lseek(fd,i_node.direct_blocks[i] * block_size + readed_bytes,SEEK_SET);
                    write(fd,new_entry,new_entry->length);
                    free(data_block);
                    return;
                }

                // We are adding to the next of the entry if there is enough space.
                if (padding_length_calculator(directory_entry->name_length) < directory_entry->length) {
                    if (directory_entry->length - padding_length_calculator(directory_entry->name_length) >= padding_length_calculator(new_entry->name_length)) {
                        new_entry->length = directory_entry->length - padding_length_calculator(directory_entry->name_length);
                        lseek(fd,i_node.direct_blocks[i] * block_size + readed_bytes + padding_length_calculator(directory_entry->name_length),SEEK_SET);
                        write(fd,new_entry,new_entry->length);                        
                        directory_entry->length = padding_length_calculator(directory_entry->name_length);
                        lseek(fd,i_node.direct_blocks[i] * block_size + readed_bytes,SEEK_SET);
                        write(fd,directory_entry,directory_entry->length);
                        free(data_block);
                        return;
                    }
                }                                                                                                                                                                                                     
                readed_bytes = readed_bytes + directory_entry->length;
                directory_entry = (void*) directory_entry + directory_entry->length; 
            }
        }
    }
    
    
    for (int i=first_data_block_number;i<block_number;i++) {
        if (allocate_new_block(i))
            break;
    }

    increment_reference_counter(allocated_data_block_for_entry);
    
    decrement_block_number = true;
            

    // Decrement block counter in group.
    int ds_table_index = 0;
    if (allocated_data_block_for_entry % block_per_group == 0) 
        ds_table_index = block_size == 1024 ? (allocated_data_block_for_entry / block_per_group) - 1 : allocated_data_block_for_entry / block_per_group; 
    else 
        ds_table_index = allocated_data_block_for_entry / block_per_group;
    if (ds_table_index < 0) 
        printf("DS table index is less than zero in insert entry to the directory.\n");


    group_descriptor_table[ds_table_index].free_block_count--;

    if (block_size == 2048 || block_size == 1024) {
        lseek(fd, 1024 + 1024 + sizeof(struct ext2_block_group_descriptor) * ds_table_index , SEEK_SET);
        write(fd, &(group_descriptor_table[ds_table_index]),sizeof(struct ext2_block_group_descriptor));           
    }

    else {
        lseek(fd, block_size + sizeof(struct ext2_block_group_descriptor) * ds_table_index  , SEEK_SET);
        write(fd, &(group_descriptor_table[ds_table_index]),sizeof(struct ext2_block_group_descriptor));   
    }




    for (int i=0;i<12;i++) {
        if (i_node.direct_blocks[i] == 0) {
            i_node.direct_blocks[i] = allocated_data_block_for_entry;
            i_node.block_count_512 = i_node.block_count_512 + (block_size/512);
            i_node.size = i_node.block_count_512 * 512;
            write_inode(directory_inode,&i_node);
            new_entry->length = block_size;
            lseek(fd,i_node.direct_blocks[i] * block_size,SEEK_SET); // write new entry
            write(fd,new_entry,new_entry->length);        
            free(data_block);
            return;            
        }
    }

}






bool allocate_new_block(int block_number) {
    int ds_table_index = 0;
    if (block_number % block_per_group == 0) 
        ds_table_index = block_size == 1024 ? (block_number / block_per_group) - 1 : block_number / block_per_group; 
    else 
        ds_table_index = block_number / block_per_group;
    if (ds_table_index < 0) 
        printf("DS table index is less than zero in allocate new block function.\n");


    int relative_bit_number= 0;
    if (block_size == 1024) 
        relative_bit_number = block_number % block_per_group == 0 ? block_per_group : block_number % block_per_group; 
    else 
        relative_bit_number = block_number % block_per_group == 0 ? 1 : (block_number % block_per_group) + 1;


    int seek_value = group_descriptor_table[ds_table_index].block_bitmap * block_size;
    lseek(fd,seek_value, SEEK_SET);                                                                   
    read(fd,block_bitmap,block_size);  
    
    int readed_bits = 0;
    for (int i=0;;i++) {
            unsigned char byte = block_bitmap[i];
            for(int j = 0; j < 8; j++) {
                readed_bits++;
                if (readed_bits == relative_bit_number) {
                    if (((byte >> j) & 0x01) == 1) 
                        return false;
                    else {
                        byte |= 0x01 << j;
                        block_bitmap[i] = byte;
                        seek_value = group_descriptor_table[ds_table_index].block_bitmap * block_size;
                        lseek(fd,seek_value, SEEK_SET);                                                                   
                        write(fd,block_bitmap,block_size);  
                        allocated_data_block_for_entry = block_number;                        
                        return true;
                    }  
                }
            }
    }
}






int padding_length_calculator(int name_length) {
    int record_length = 8 + name_length;
    if (record_length % 4 == 0)
        return record_length;
    if (record_length % 4 == 1)
        return record_length + 3;    
    if (record_length % 4 == 2)
        return record_length + 2;
    if (record_length % 4 == 3)
        return record_length + 1;
}






void decrement_inode_from_group(int inode_number) {
    int ds_table_index = 0;
    if (inode_number % i_node_per_group == 0) 
        ds_table_index = (inode_number / i_node_per_group) - 1;
    else 
        ds_table_index = inode_number / i_node_per_group;
    if (ds_table_index < 0) 
        printf("DS table index is less than zero. Look at fetch inode function.\n");
    

    group_descriptor_table[ds_table_index].free_inode_count--;
    
    if (block_size == 2048 || block_size == 1024) {
        lseek(fd, 1024 + 1024 + sizeof(struct ext2_block_group_descriptor) * ds_table_index , SEEK_SET);
        write(fd, &(group_descriptor_table[ds_table_index]),sizeof(struct ext2_block_group_descriptor));           
    }

    else {
        lseek(fd, block_size + sizeof(struct ext2_block_group_descriptor) * ds_table_index  , SEEK_SET);
        write(fd, &(group_descriptor_table[ds_table_index]),sizeof(struct ext2_block_group_descriptor));   
    }


    return;
}








// Functions for remove are below.
int get_file_inode_number_from_dir(int dir_inode,char* file_name) {
    struct ext2_inode i_node;
    struct ext2_dir_entry* directory_entry;
    
    unsigned char* data_block;
    data_block = malloc(block_size);
    
    int j=0;
    int readed_bytes = 0;
    int file_inode_number = 0;

    char dir_file[EXT2_MAX_NAME_LENGTH+1];
    fetch_inode_from_table(dir_inode,&i_node); 
    
    for (int i=0;i<12;i++) {
        if (i_node.direct_blocks[i] != 0) {
            lseek(fd,i_node.direct_blocks[i] * block_size,SEEK_SET);
            read(fd,data_block,block_size);
            readed_bytes = 0;
            directory_entry = (struct ext2_dir_entry *) data_block;
            while (readed_bytes < block_size) {
                if (directory_entry->inode != 0) {
                    memcpy(dir_file, directory_entry->name, directory_entry->name_length);
                    dir_file[directory_entry->name_length] = 0;     
                    if (!strcmp(dir_file,file_name)) {  
                        file_inode_number = directory_entry->inode;   
                        if (readed_bytes == 0) {
                            directory_entry->inode = 0;
                            lseek(fd,i_node.direct_blocks[i] * block_size,SEEK_SET);
                            write(fd,directory_entry,directory_entry->length);                   
                        }
                        else 
                            delete_dir_entry_from_inode(&i_node,i,readed_bytes);
                        free(data_block);
                        return file_inode_number; 
                    }
                }
                readed_bytes +=  directory_entry->length;
                directory_entry = (void*) directory_entry + directory_entry->length; 
            }
        }
    }

    printf("We could not find file i_node number. Look at here man.\n");
    return 1;
}






void delete_dir_entry_from_inode(struct ext2_inode* i_node,int data_block_number,int location_of_entry) {
    struct ext2_dir_entry* directory_entry;
    struct ext2_dir_entry* deleted_entry;

    unsigned char* data_block;
    data_block = malloc(block_size);
    

    int readed_bytes = 0;


    lseek(fd,(i_node->direct_blocks)[data_block_number] * block_size,SEEK_SET);
    read(fd,data_block,block_size);
    
    readed_bytes = 0;
    directory_entry = (struct ext2_dir_entry *) data_block;
    
    while (readed_bytes < block_size) {
        if (readed_bytes + directory_entry->length == location_of_entry) {
            deleted_entry = (void*) directory_entry + directory_entry->length;        
            directory_entry->length = directory_entry->length + deleted_entry->length;
            lseek(fd,(i_node->direct_blocks)[data_block_number] * block_size + readed_bytes,SEEK_SET);
            write(fd,directory_entry,directory_entry->length);
            break;
        }
        readed_bytes +=  directory_entry->length;
        directory_entry = (void*) directory_entry + directory_entry->length; 
    }
    
    
    free(data_block);
    return;
}





void delete_inode(int inode_number) {
    struct ext2_inode i_node;
    fetch_inode_from_table(inode_number,&i_node);
    i_node.link_count--;
    if (i_node.link_count > 0) {
        write_inode(inode_number,&i_node);
        return;
    }

    else {
        gettimeofday(&time,NULL);
        increment_inode_number = true;
        delete_inode_from_bitmap(inode_number);
        increment_inode_number_from_group(inode_number);
        i_node.deletion_time = time.tv_sec; 
        i_node.link_count = 0;
        decrement_reference_counters(i_node.direct_blocks);
        i_node.block_count_512 = 0;
        i_node.size = 0;
        for (int i = 0;i<EXT2_NUM_DIRECT_BLOCKS;i++) 
            i_node.direct_blocks[i] = 0;



        write_inode(inode_number,&i_node);

        // inode bitmapten sil. DONE !!
        // inode bloklarını decrement reference count yaz oraya gönder. DONE !!
        // data block numaralarını sıfır yap. DONE !!
        // bu inode tekrar yazmayı unutma. Herhangi bir yerde değiştirdiğini tekrar yaz file ' a. hadi kolay gelsin. DONE !!
        // orada sıfıra ulaşırsa block bitmaplerini de sil. DONE !!
        // grup ve globalde inode ve block counterlarını arttır. DONE !!
        //
    }



    return;
}





void delete_inode_from_bitmap(int inode_number) {
    int ds_table_index = 0;
    if (inode_number % i_node_per_group == 0) 
        ds_table_index = (inode_number / i_node_per_group) - 1;
    else 
        ds_table_index = inode_number / i_node_per_group;
    if (ds_table_index < 0) 
        printf("DS table index is less than zero. Look at is_node_free function.\n");    

    int seek_value = group_descriptor_table[ds_table_index].inode_bitmap * block_size;
    lseek(fd,seek_value, SEEK_SET);                                                                   
    read(fd,inode_bitmap,block_size);  


    int relative_inode_number = inode_number % i_node_per_group == 0 ? i_node_per_group : inode_number % i_node_per_group ;
    int readed_bits = 0;
    for (int i=0;;i++) {
        unsigned char byte = inode_bitmap[i];
        for(int j = 0; j < 8; j++) {
            readed_bits++;
            if (readed_bits == relative_inode_number) {
                byte &= ~(0x01 << j);
                inode_bitmap[i] = byte;
                seek_value = group_descriptor_table[ds_table_index].inode_bitmap * block_size;
                lseek(fd,seek_value, SEEK_SET);                                                                   
                write(fd,inode_bitmap,block_size);    
                return;                   
            }
        }
    }
    printf("We could not deallocate inode bitmap in delete_inode_from_bitmap.\n");
    return;
} 





void increment_inode_number_from_group(int inode_number) {
    int ds_table_index = 0;
    if (inode_number % i_node_per_group == 0) 
        ds_table_index = (inode_number / i_node_per_group) - 1;
    else 
        ds_table_index = inode_number / i_node_per_group;
    if (ds_table_index < 0) 
        printf("DS table index is less than zero. Look at fetch inode function.\n");
    

    group_descriptor_table[ds_table_index].free_inode_count++;
    
    if (block_size == 2048 || block_size == 1024) {
        lseek(fd, 1024 + 1024 + sizeof(struct ext2_block_group_descriptor) * ds_table_index , SEEK_SET);
        write(fd, &(group_descriptor_table[ds_table_index]),sizeof(struct ext2_block_group_descriptor));           
    }

    else {
        lseek(fd, block_size + sizeof(struct ext2_block_group_descriptor) * ds_table_index  , SEEK_SET);
        write(fd, &(group_descriptor_table[ds_table_index]),sizeof(struct ext2_block_group_descriptor));   
    }

    return;
}






void decrement_reference_counters(uint32_t direct_blocks[EXT2_NUM_DIRECT_BLOCKS]) {
    refctr_t reference_counter;
    for (int i=0;i<12;i++) {
        if (direct_blocks[i] != 0) {
            int block_number = direct_blocks[i];
            int ds_table_index = 0;
            if (block_number % block_per_group == 0) 
                ds_table_index = block_size == 1024 ? (block_number / block_per_group) - 1 : block_number / block_per_group; 
            else 
                ds_table_index = block_number / block_per_group;
            if (ds_table_index < 0) 
                printf("DS table index is less than zero in decrement reference counters function.\n");

            int relative_offset = 0;
            if (block_size == 1024) 
                relative_offset = block_number % block_per_group == 0 ? block_per_group - 1 : (block_number % block_per_group) - 1; 
            else 
                relative_offset = block_number % block_per_group == 0 ? 0 : block_number % block_per_group;
            
            int seek_value = group_descriptor_table[ds_table_index].block_refmap * block_size + relative_offset * sizeof(reference_counter);
            lseek(fd,seek_value, SEEK_SET);
            read(fd,&reference_counter,sizeof(reference_counter));
            
            reference_counter--;
            if (reference_counter <= 0) {
                number_of_deallocated_blocks++;
                printf("%d ",block_number);
                increment_block_counter_in_group(block_number);
                deallocate_block_number(block_number);
            }
            lseek(fd,seek_value, SEEK_SET);
            write(fd,&reference_counter,sizeof(reference_counter));
        }
    }
    return;
}






void increment_block_counter_in_group(int block_number) {
    int ds_table_index = 0;
    if (block_number % block_per_group == 0) 
        ds_table_index = block_size == 1024 ? (block_number / block_per_group) - 1 : block_number / block_per_group; 
    else 
        ds_table_index = block_number / block_per_group;
    if (ds_table_index < 0) 
        printf("DS table index is less than zero in insert entry to the directory.\n");


    group_descriptor_table[ds_table_index].free_block_count++;

    if (block_size == 2048 || block_size == 1024) {
        lseek(fd, 1024 + 1024 + sizeof(struct ext2_block_group_descriptor) * ds_table_index , SEEK_SET);
        write(fd, &(group_descriptor_table[ds_table_index]),sizeof(struct ext2_block_group_descriptor));           
    }

    else {
        lseek(fd, block_size + sizeof(struct ext2_block_group_descriptor) * ds_table_index  , SEEK_SET);
        write(fd, &(group_descriptor_table[ds_table_index]),sizeof(struct ext2_block_group_descriptor));   
    }


    return;
}






void deallocate_block_number(int block_number) {
    int ds_table_index = 0;
    if (block_number % block_per_group == 0) 
        ds_table_index = block_size == 1024 ? (block_number / block_per_group) - 1 : block_number / block_per_group; 
    else 
        ds_table_index = block_number / block_per_group;
    if (ds_table_index < 0) 
        printf("DS table index is less than zero in allocate new block function.\n");


    int relative_bit_number= 0;
    if (block_size == 1024) 
        relative_bit_number = block_number % block_per_group == 0 ? block_per_group : block_number % block_per_group; 
    else 
        relative_bit_number = block_number % block_per_group == 0 ? 1 : (block_number % block_per_group) + 1;


    int seek_value = group_descriptor_table[ds_table_index].block_bitmap * block_size;
    lseek(fd,seek_value, SEEK_SET);                                                                   
    read(fd,block_bitmap,block_size);  
    
    int readed_bits = 0;
    for (int i=0;;i++) {
            unsigned char byte = block_bitmap[i];
            for(int j = 0; j < 8; j++) {
                readed_bits++;
                if (readed_bits == relative_bit_number) {
                    byte &= ~(0x01 << j); // Clear relative bit number in block bitmap.
                    block_bitmap[i] = byte;
                    seek_value = group_descriptor_table[ds_table_index].block_bitmap * block_size;
                    lseek(fd,seek_value, SEEK_SET);                                                                   
                    write(fd,block_bitmap,block_size);      
                    return;                   
                }
            }
    }
    printf("We could not deallocate block bitmap in deallocate_block_number.\n");
    return;
}






int main(int argc, char *argv[])
{
    struct ext2_super_block super_block;
    struct ext2_inode i_node;
    struct ext2_dir_entry* directory_entry;


    if (true) {
        fd = open(argv[2],O_RDWR); 
        lseek(fd, EXT2_SUPER_BLOCK_POSITION, SEEK_SET); 
        read(fd, &super_block, sizeof(super_block));

        block_size = EXT2_UNLOG(super_block.s_log_block_size);
        i_node_per_group = super_block.s_inodes_per_group;
        i_node_size = super_block.s_inode_size;
        block_per_group = super_block.s_blocks_per_group;
        first_data_block_number = super_block.s_first_data_block;
        block_number = super_block.s_block_count;

        block_bitmap = malloc(block_size);
        inode_bitmap = malloc(block_size);

        group_count = 1 + (super_block.s_block_count-1) / super_block.s_blocks_per_group;
        group_descriptor_table = malloc(sizeof(struct ext2_block_group_descriptor)*group_count);

        if (block_size == 2048 || block_size == 1024) {
            unsigned int group_descriptor_table_offset = 0;
            for (int i=0;i < group_count;i++) {
                lseek(fd, 1024 + 1024 + group_descriptor_table_offset , SEEK_SET);
                read(fd, &(group_descriptor_table[i]),sizeof(struct ext2_block_group_descriptor));
                group_descriptor_table_offset += sizeof(struct ext2_block_group_descriptor);
            }
        }

        else {
            unsigned int group_descriptor_table_offset = 0;
            for (int i=0;i < group_count;i++) {
                lseek(fd, block_size + group_descriptor_table_offset , SEEK_SET);
                read(fd, &(group_descriptor_table[i]),sizeof(struct ext2_block_group_descriptor));
                group_descriptor_table_offset += sizeof(struct ext2_block_group_descriptor);
            }        
        }
    }


    if (!strcmp(argv[1],"dup")) {
        unsigned int source_inode_number = 0;
        unsigned int destination_inode_number;
        if (argv[4][0] != '/') {
            char* token;
            token = strtok(argv[4], "/");
            destination_inode_number = atoi(token);
            while( token != NULL ) {
                token = strtok(NULL, "/");
                if (token != NULL)
                    strcpy(destination_file_name,token);
            }
        }
        else 
            destination_inode_number = get_source_inode_from_path(argv[4],1); // 1 means extract destination i_node 
        
        if (isNumber(argv[3])) 
            source_inode_number = atoi(argv[3]);
        else 
            source_inode_number = get_source_inode_from_path(argv[3],0); // 0 means extract source i_node


        int reserved_inode_number = 0;
        for (int i=super_block.s_first_inode;i<super_block.s_inode_count;i++) {
            if (free_inode(i)) {
                reserved_inode_number = i;
                break;
            }
        }

        fetch_inode_from_table(source_inode_number,&i_node);
        gettimeofday(&time,NULL);
        i_node.access_time = time.tv_sec; i_node.creation_time = time.tv_sec; i_node.modification_time = time.tv_sec; i_node.link_count = 1; i_node.deletion_time = 0;
        write_inode(reserved_inode_number,&i_node);

        increment_reference_counters(i_node.direct_blocks);

        insert_entry_to_the_directory(destination_inode_number,reserved_inode_number);

        super_block.s_free_inode_count--;
        if (decrement_block_number) 
            super_block.s_free_block_count--;
        lseek(fd, EXT2_SUPER_BLOCK_POSITION, SEEK_SET); 
        write(fd, &super_block, sizeof(super_block));

        decrement_inode_from_group(reserved_inode_number);

        printf("%d\n%d\n",reserved_inode_number,allocated_data_block_for_entry);
    }


    else if (!strcmp(argv[1],"rm")) {
        unsigned int directory_inode_number;
        unsigned int file_inode_number;
        if (argv[3][0] != '/') {
            char* token;
            token = strtok(argv[3], "/");
            directory_inode_number = atoi(token);
            while( token != NULL ) {
                token = strtok(NULL, "/");
                if (token != NULL)
                    strcpy(destination_file_name,token);
            }
        }
        else 
            directory_inode_number = get_source_inode_from_path(argv[3],1); 


        file_inode_number = get_file_inode_number_from_dir(directory_inode_number,destination_file_name);
        
        printf("%d\n",file_inode_number);

        
        delete_inode(file_inode_number);

        
        if (increment_inode_number) 
            super_block.s_free_inode_count++;      

        super_block.s_free_block_count = super_block.s_free_block_count + number_of_deallocated_blocks;
        lseek(fd, EXT2_SUPER_BLOCK_POSITION, SEEK_SET); 
        write(fd, &super_block, sizeof(super_block)); 
        

        if (number_of_deallocated_blocks > 0)
            printf("\n");
        else 
            printf("%d\n",-1);

    }




















    free(block_bitmap);
    free(inode_bitmap);
    free(group_descriptor_table);
    close(fd);
} 


































































