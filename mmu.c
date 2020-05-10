#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "mmu.h"
#include <limits.h>

static FILE *output = NULL;
static FILE *backing_store_file = NULL;

static struct log_addr addr_table[PAGETABLE_SIZE*FRAME_SIZE];
static int TLB_pg[TLB_SIZE];
static int TLB_fr[TLB_SIZE];
static int pt[PAGETABLE_SIZE];
static int pt_lr[PAGETABLE_SIZE];
static int RAM[PAGETABLE_SIZE][FRAME_SIZE];

static int addr_emp_idx = 0;
static int RAM_emp_idx = 0;
static int TLB_emp_idx = 0;
static int pt_emp_idx = 0;
static int num_addr_entries = 0;

static int page_hits = 0;
static int page_faults = 0;
static int TLB_hits = 0;
static int TLB_misses = 0;
static int iter = 0;

int main(int argc, char *argv[])
{   
    char* line;
    static FILE *addresses_file = NULL;

    if (argc != 3) {
        fprintf(stderr, "Incorrect number of arguments\n Usage: [filename] [backing store file] [address file]\n");
        return -1;
    }
    backing_store_file = fopen(argv[1],"rb");
    if (backing_store_file == NULL) fprintf(stderr, "Backing store open failed\n");
    //initialize the arrays by writing INT_MIN to each cell
    init_arrays();
    addresses_file = fopen(argv[2], "r");
    if (addresses_file == NULL) {
        fprintf(stderr, "Unable to open addresses file\n");
        return -1;
    }
    line = malloc(sizeof(int)*10);
    while (fgets(line,FRAME_SIZE,addresses_file) != NULL) {
            int raw_addr = atoi(line);
            add_addr_table(raw_addr);
            num_addr_entries++;
     }
    
    output = fopen(OUTPUT_FILE, "w");

    if (output == NULL) {
        fprintf(stderr, "Unable to open output file. Attempting to continue\n");
    }
    //fprintf(output, "Logical Address,Physical Address,Value\n");
    int frame = -1;
    //num_addr_entries
    for (int i = 0; i < num_addr_entries; i++) {
        iter++;
        struct log_addr* temp = &addr_table[i];
        frame = get_frame(temp);
        //printf("Iter: %d\n", iter);
        //printf("Frame: %d\n", frame);
        //printf("Offset: %d\n", (temp->offset)); 
        //printf("Data: %d\n", get_byte(frame, (temp->offset)));
        //printf("RAM search output: %d\n", get_byte(frame, (temp->offset)));
        //printf("%d\n", get_byte(frame, (temp->offset)));
        //print_pt();
        fprintf(output,"%d,%d,%d\n", (temp->raw), (frame*FRAME_SIZE) + (temp->offset), get_byte(frame, (temp->offset)));
    }
    printf("Parameters:\nPagetable size: %d\n If you would like to change the pagetable size, please view the readme.txt file\n", (int) PAGETABLE_SIZE);
    printf("Statistics:\nPage Faults = %d\nTLB Hits = %d\n",page_faults, TLB_hits);
    printf("Page Fault Percentage = %0.2f%%\n", (((double) page_faults) / (double) num_addr_entries)*100.00);
    printf("TLB Hit Percentage = %0.2f%%\n", (((double) TLB_hits) / (double) num_addr_entries)*100.00);
    printf("Number of addresses processed: %d\n", num_addr_entries);
    fclose(backing_store_file);
    fclose(addresses_file);
    return 0;
}

int get_frame(struct log_addr* log_addr) {
    //Check TLB
    int frame;
    
    frame = search_TLB(log_addr);
    if (frame != INT_MIN) {
        TLB_hits++;
        //printf("Found in TLB\n");
        //Update the counter entry in the page table
        update_pt_lr(log_addr);
        // printf("Found in TLB. Frame: %d\n", frame);
        // printf("TLB: Data from RAM: %d\n", get_byte(frame, (log_addr->offset)));
        // printf("Corresponding pt frame: %d\n", debug_search_pt(log_addr));
        // printf("PT: Data from RAM: %d\n", get_byte(search_pt(log_addr), (log_addr->offset)));
        // printf("Raw backstore out: %d\n", get_debug_byte_backstore(log_addr));
        return frame;
    } else {
        TLB_misses++;
    }

    //Then check page table.
    //If not in the page table, that means that it is a page fault and must be loaded from the backing store 
    frame = search_pt(log_addr);
    if (frame != INT_MIN) {  
        page_hits++;
        // printf("Found in PT. Frame: %d\n", frame);
        // printf("PT: Data from RAM: %d\n", get_byte(frame, (log_addr->offset)));
        // printf("Raw backstore out: %d\n", get_debug_byte_backstore(log_addr));
        return frame;
    } else {
        page_faults++;
    }

    //Get the data from backing store
    //printf("Getting from BS\n");
    frame = load_page_backing_store(log_addr);
    return frame;
}
int debug_search_pt(struct log_addr* log_addr) {
    //Search pagetable without doing any updates
     for (int i = 0; i < PAGETABLE_SIZE; i++) {
        if ((log_addr->page_num) == (pt[i])) {
            return i;
        }
    }
    return INT_MIN;
}
int get_debug_byte_backstore(struct log_addr* log_addr) {
    signed char buf[FRAME_SIZE];
    fseek(backing_store_file, ((log_addr -> page_num)*FRAME_SIZE), SEEK_SET);
    fread(buf, sizeof(signed char), FRAME_SIZE, backing_store_file);
    return buf[(log_addr->offset)];
}
int get_byte(int frame, int offset) {
    //Retrieves a byte from RAM based on the frame and offset
    //printf("%d\n",RAM[frame][offset]);
    return RAM[frame][offset];
}
int search_TLB(struct log_addr* log_addr) {  
    //update count determines whether or not to update the countery in the page table
    //Searches the TLB for the logical address. returns INT_MIN if not found, and the data from RAM if found
    for (int i = 0; i < TLB_SIZE; i++) {
        if ((log_addr->page_num) == (TLB_pg[i])) {
            //If print flag is set, print out the output
            //If a page number match is found, then return the frame
            return TLB_fr[i];
        }
    }
    //If not found in loop, then it does not exist in TLB.
    //printf("Did not find in TLB\n");
    return INT_MIN;
}
int update_pt_lr(struct log_addr* log_addr) {
    for (int i = 0; i < PAGETABLE_SIZE; i++) {
        if ((log_addr->page_num) == (pt[i])) {
            pt_lr[i] = iter;
            return i;
        }
    }
}
int search_pt(struct log_addr* log_addr) {
    //Searches the page table for the logical address. returns INT_MIN if not found, and the data from RAM if found
    for (int i = 0; i < PAGETABLE_SIZE; i++) {
        if ((log_addr->page_num) == (pt[i])) {
            //printf("Found in PT\nPage: %d, Frame: %d\n", (log_addr->page_num), i);
            //Add to TLB
            //printf("Calling add TLB with add page %d, frame: %d\n", (log_addr->page_num), i);
            add_TLB(log_addr, i);
            //Update the count of when the entry was accessed
            //printf("Updating LR from search_pt\n");
            update_pt_lr(log_addr);
            //If a page number match is found, then return the frame, which is i.
            return i;
        }
    }
    //If not found in loop, then it does not exist in pt
    //printf("Did not find in PT\n");
    return INT_MIN;
}

int add_TLB(struct log_addr* log_addr, int frame_num) {
    //printf("Adding TLB: page: %d, frame %d\n", (log_addr->page_num), frame_num);
    //Check to see if item already exists in TLB
    if (search_TLB(log_addr) != INT_MIN) {
        return INT_MIN;
    }    
    if (TLB_emp_idx < TLB_SIZE) {
        //If empty, add to the end of the TLB
        TLB_pg[TLB_emp_idx] = (log_addr->page_num);
        TLB_fr[TLB_emp_idx] = frame_num;
        TLB_emp_idx++;
    } else {
        //Remove entry in TLB based on FIFO. The last index will be freed.
        remove_TLB_FIFO();
        TLB_pg[TLB_SIZE - 1] = (log_addr->page_num);
        TLB_fr[TLB_SIZE - 1] = frame_num;
    }
    return frame_num;
}
int remove_TLB_FIFO() {
    //The first element is the first element. Therefore, shift all elements up by one, starting from the second index.
    for (int i = 1; i < TLB_SIZE; i++) {
        TLB_pg[i - 1] = TLB_pg[i];
        TLB_fr[i - 1] = TLB_fr[i];
    }
    //Last index becomes INT_MIN to show that there is no value there.
    TLB_pg[TLB_SIZE - 1] = INT_MIN;
    TLB_fr[TLB_SIZE - 1] = INT_MIN;
    return TLB_SIZE - 1;
}
int load_page_backing_store (struct log_addr* log_addr) {
    //Loads a page into page table and RAM from the backing store. Returns the byte value of offset.
    signed char buf[FRAME_SIZE];
    fseek(backing_store_file, ((log_addr -> page_num)*FRAME_SIZE), SEEK_SET);
    fread(buf, sizeof(signed char), FRAME_SIZE, backing_store_file);
    //printf("Raw data from Backstore: %d\n", buf[(log_addr->offset)]);
    //Return the frame
    return add_pt(log_addr, buf);
}

int add_pt(struct log_addr* log_addr, signed char data[]) {
    int frame;
    //Determine if page table is full
    //Check to see if it already exists in the page table. if it does, then return that frame
    frame = debug_search_pt(log_addr);
    if (frame != INT_MIN) {
        return frame;
    }
    if (pt_emp_idx < PAGETABLE_SIZE) {
        //If not full, insert the page into the empty index
        frame = insert_page(pt_emp_idx, (log_addr->page_num), data);
        //Add to TLB
        //printf("RAM output: %d\n", RAM[frame][log_addr->offset]);
        add_TLB(log_addr, pt_emp_idx);
        pt_emp_idx++;
        //Return the frame 
        return frame;
    } else {
        //remove least recently used 
        int idx = remove_pt_LRU(); 
        frame = insert_page(idx, (log_addr->page_num), data);
        add_TLB(log_addr, frame);
        //Return the frame
        return idx;
    }
    
}
int remove_pt_LRU() {
    //Removes the least recently used page, and returns it's index so that a new page can be stored
    int curr_min = INT_MAX;
    int curr_min_idx = -1;
    static int remove_count;
    
    for (int i = 0; i < PAGETABLE_SIZE; i++) {
        if (pt_lr[i] <= curr_min) {
            curr_min = pt_lr[i];
            curr_min_idx = i;
        }
    }
    if (curr_min_idx == -1) {
        fprintf(stderr, "Pagetable earliest access search failed\n");
    }
    remove_count++;
    //printf("LRU Used: %d\n", remove_count);
    //printf("Removing entry at: %d. Last accessed: %d\n", curr_min_idx, pt_lr[curr_min_idx]);
    //Remove the entry with the least count (So it last was accessed at the earliest time -- least recently used) 
    pt[curr_min_idx] = 0;
    pt_lr[curr_min_idx] = 0;
    return curr_min_idx;
}
int insert_page(int index, int page_num, signed char data[]) {
    //Low level function to insert a page into the page table and the frame RAM
    if ((index < 0) || index >= PAGETABLE_SIZE) {
        fprintf(stderr, "Insert failed - page table index out of bounds\n");
        return -1;
    }
    //printf("Inserting page at: %d\n", index);

    pt[index] = page_num;
    //Store the access time
    pt_lr[index] = iter;
    //Load the frame into RAM, bit by bit
    // printf("Inserting data:\n");
    // for (int i = 0; i < FRAME_SIZE; i++) {
    //     printf("%d",data[i]);
    // }
    // printf("\n");
    for (int i = 0; i < FRAME_SIZE; i++) {
        RAM[index][i] = data[i];
    }
    //Return frame
    return index;
}   
int print_addr_table() {
    for (int i = 0; i < addr_emp_idx; i++) {
        print_addr(&addr_table[i]);
    }
    return 0;
}

int print_addr(struct log_addr* a) {
    //printf("Logical address: %d, Page number: %d, Offset %d\n", a->phys_addr, a->page_num, a->offset);
    //printf("Page number: %d, Offset: %d\n", a->page_num, a->offset);
    return 0;
}

int add_addr_table(int raw_addr) {
    //Adds an entry to the page table
    struct log_addr *temp;
    temp =  malloc(sizeof(struct log_addr));
    //Physical address is the top 16 bits. 
    //temp -> phys_addr = (raw_addr >> 16);
    //printf("Physical address: %d\n", (temp->phys_addr));
    //Page number is bits 8 - 15. Shift right 8 and mask.
    temp -> page_num = ((raw_addr &(0xFFFF)) >> 8);
    //Offset is bottom 8 bits. 
    temp -> offset = (raw_addr & (0xFF));
    temp -> raw = raw_addr;
    //Add the entry to the page table
    //printf("Adding to index: %d, address:\n", addr_emp_idx);
    addr_table[addr_emp_idx] = *temp;
    addr_emp_idx++;
    return addr_emp_idx - 1;
} 
int print_TLB() {
    printf("TLB Contents:\n");
    for(int i = 0; i < TLB_SIZE; i++) {
        printf("Index: %d: Page Number %d , frame number %d\n", i, TLB_pg[i], TLB_fr[i]);
    }
}

int print_pt() {
    //printf("Page Table contents:\n"); 
    for(int i = 0; i < PAGETABLE_SIZE; i++) {
        //printf("I: %d: PN %d , LR: %d\n", i, pt[i], pt_lr[i]); DEBUG
        printf("idx: %d fr: %d \n", i, pt[i]);
    }
}
int init_arrays() {
    //Initialize arrays to INT_MIN
    for(int i = 0; i < TLB_SIZE; i++) {
        TLB_pg[i] = INT_MIN;
        TLB_fr[i] = INT_MIN;
    }
    for(int i = 0; i < PAGETABLE_SIZE; i++) {
        pt[i] = INT_MIN;
        pt_lr[i] = INT_MIN;
        RAM[i][0] = INT_MIN;
    }
}  
