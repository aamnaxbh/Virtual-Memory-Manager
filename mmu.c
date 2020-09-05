#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <alloca.h>
#define MASK_FOR_ADDRESS 0xFFFF //this is page num + offset (16) - will need to bitshift later 
#define MASK_FOR_OFFSET 0xFF //this is just offset (8) 
#define ADDRESS_SIZE 7 //max is 5, 6 just in case - 6 didnt work - 7 did 
#define PAGE_TABLE_SIZE 256 //given 
#define TLB_SIZE 16 //given 
#define FRAME_SIZE 256 //given
#define FRAME_COUNT 256 //given 
#define BYTES 256 //given for storage 


//files 
FILE *input_file; 
FILE *store_file; 


int logical_address; 
char address[ADDRESS_SIZE]; 
int TLB_hits = 0; 
int page_faults = 0;
int translated_addresses_count = 0;
int first_free_page = 0;
int first_free_frame = 0;
int TLB_entries = 0; 

int TLB_pages[TLB_SIZE];            //page numbers in TLB
int TLB_frames[TLB_SIZE];           //frame numbers in TLB
int pages_table[PAGE_TABLE_SIZE];   //page numbers in table
int frames_table[PAGE_TABLE_SIZE];  //frame numbers in table 
signed char store_reads[BYTES];     //info coming from backing store
int physical_memory[FRAME_COUNT][FRAME_SIZE]; 
int phys_add_arr[2000]; 
int log_add_arr[2000]; 
int value_arr[2000];


void get_info(int logical_addy);
void frames(int page_number, int offset); 
void make_csv_file();
void insert_into_TLB(int page_number, int frame_number);
void backing_store(int page_number); 


//get info of the address such as offset and page number 
void get_info(int logical_addy) {
    int i, frame; 
    int offset = logical_addy & MASK_FOR_OFFSET; 
    int page_num = ((logical_addy & MASK_FOR_ADDRESS) >> 8); 
   
    // check for the diff way to do this if itll work 
    //setting a diff vairbale for page num and shit yknow what im talkin about
    frames(page_num, offset);     
}

void frames(int page_number, int offset) {
    int i, frame; 
    
    for(i = 0; i < TLB_SIZE; i++) {
        if (TLB_pages[i] == page_number) {
            frame = TLB_frames[i]; 
            TLB_hits++;
        }
        else {
            frame = -1;     //frame not found 
        }
    }

    //if not found, look in page tables and retrieve it
    if (frame == -1) { 
        for (i = 0; i < first_free_page; i++) {
            if (pages_table[i] == page_number) {
                frame = frames_table[i]; 
            }
        }
        //if still not found, go to backing store 
        if (frame == -1) {
            backing_store(page_number); 
            frame = first_free_page - 1; 
            page_faults++; 

        }
    }

    insert_into_TLB(page_number, frame); 
    int value = physical_memory[frame][offset];
    int physical_address = (frame << 8) | offset; 
    //make_csv_file(logical_address, physical_address, value); 

    //printf("frame number: %d\n", frame);
    //printf("offset: %d\n", offset); 
    // output the virtual address, physical address and value of the signed char to the console
    //printf("Virtual address: %d Physical address: %d\n", log, (frame << 8) | offset);
    //printf("value: %d\n", value); 
    phys_add_arr[translated_addresses_count] = physical_address;
    log_add_arr[translated_addresses_count] = logical_address; 
    value_arr[translated_addresses_count] = value; 
}

void make_csv_file() {
    FILE *data; 
    //char[20] *filename = "output"; 
    int i;
    //filename = strcat(filename, ".csv");

    data = fopen("output.csv", "w+"); 
    fprintf(data, "Logical Address, Physical Address, Signed Byte Value"); 

    for (i = 1; i <= translated_addresses_count; i++) {
        fprintf(data, "\n%d, %d, %d", log_add_arr[i], phys_add_arr[i], value_arr[i]); 
    }
    fclose(data); 

}

void insert_into_TLB(int page_number, int frame_number) {
    int i;
    int duplicate = 0; 

    for (i = 0; i < TLB_entries; i++) {
        if (TLB_pages[i] == page_number) {
            duplicate = i;                      //if its not in tlb, then i != TLB_entries 
            break;                              //if page # is already in tlb, stop iterating - keep track  of index
        }
    }

    if (duplicate != 0) {   //then its not in the tlb 
        if (TLB_entries < TLB_SIZE) {   //add onto end via fifo, given number of entries is within size
            TLB_frames[TLB_entries] = frame_number; 
            TLB_pages[TLB_entries] = page_number; 
        }
        else {  //if tlb is full
            for (i = 0; i < TLB_SIZE - 1; i++) {    //shift everything to left, leave last index empty and insert new thing there 
                TLB_frames[i] = TLB_frames[i + 1];
                TLB_pages[i] = TLB_pages[i + 1]; 
            }
            TLB_frames[TLB_SIZE - 1] = frame_number; 
            TLB_pages[TLB_SIZE - 1] = page_number;
        }
    }
    else {  //page number already exists in the tlb so it must be removed before added again 
        for (i = duplicate; i < TLB_entries - 1; i++) {
            TLB_frames[i] = TLB_frames[i + 1];
            TLB_pages[i] = TLB_pages[i + 1]; 
        }
         if (TLB_entries < TLB_SIZE) {   //add onto end via fifo, given number of entries is within size
            TLB_frames[TLB_entries] = frame_number; 
            TLB_pages[TLB_entries] = page_number; 
         }
         else {
             TLB_frames[TLB_entries - 1] = frame_number; 
             TLB_pages[TLB_entries - 1] = page_number;
         }
    }
    if (TLB_entries < TLB_SIZE) {
        TLB_entries++; 
    }
}


void backing_store(int page_number) {
    int i;
    if (fseek(store_file, page_number * BYTES, SEEK_SET) != 0) {
        fprintf(stdout, "Backing store file cannot be seeked.\n"); 
    }

    if (fread(store_reads, sizeof(signed char), BYTES, store_file) == 0) {
        fprintf(stdout, "Backing store file cannot be read.\n"); 
    }

    for (i = 0; i < BYTES; i++) {
        physical_memory[first_free_frame][i] = store_reads[i]; 
    }

    frames_table[first_free_page] = first_free_frame;
    pages_table[first_free_page] = page_number; 
    first_free_page++;
    first_free_frame++; 

}

int main(int argc, char *argv[]) {
    //int translated_addresses_count = 0;
    if (argc != 2) {
        fprintf(stdout, "Error in running this file, wrong number of arguments entered.\n"); 
    }
     store_file = fopen("BACKING_STORE.bin", "rb"); 
    input_file = fopen(argv[1], "r"); 
    //store_file = fopen("BACKING_STORE.bin", "rb"); 

    if (input_file == NULL) {
        fprintf(stdout, "File is empty or cannot be opened.\n"); 
    }

    if (store_file == NULL) {
        fprintf(stdout, "File is empty or cannot be opened.\n"); 
    }

    while ((fgets(address, ADDRESS_SIZE, input_file)) != NULL) {
        logical_address = atoi(address); 
        translated_addresses_count++; 
        get_info(logical_address); 
        //translated_addresses_count++; 
       // fputs(address, stdout);  //-----> prints the file contents correctly! :)
        
    }
    make_csv_file();
    double fault_rate = page_faults / (double) translated_addresses_count;
    double TLB_rate = TLB_hits / (double) translated_addresses_count; 

    printf("The Page fault rate is %.3f.\n", fault_rate); 
    printf("The TLB hit rate is %.3f.\n", TLB_rate); 

    // printf("count = %d\n", translated_addresses_count); 
    // printf("faults = %d\n", page_faults); 
    // printf("hits: %d\n", TLB_hits); 

    fclose(input_file); 
    fclose(store_file); 
    return 0; 
}