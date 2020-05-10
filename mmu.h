#define PAGETABLE_SIZE  (128)
#define TLB_SIZE    (16)
#define FRAME_SIZE  (256)
#define OUTPUT_FILE "output.csv"

struct log_addr {
    int raw; 
    int page_num;
    int offset;
};

//Function declarations
int print_addr(struct log_addr*);
int load_page_backing_store(struct log_addr*);
//signed char load_page_backing_store(struct log_addr*);
int add_addr_table(int);
int print_addr_table();
int get_frame(struct log_addr*);
int search_TLB(struct log_addr*);
int search_pt(struct log_addr*);
int add_pt(struct log_addr*, signed char[]);
int insert_page(int, int, signed char[]);
int init_arrays();
int add_TLB(struct log_addr*, int);
int remove_TLB_FIFO();
int print_TLB();
int remove_pt_LRU();
int print_pt();
int get_byte(int,int);
signed char get_data(struct log_addr*);
int debug_search_pt(struct log_addr*);
int get_debug_byte_backstore(struct log_addr*);
int update_pt_lr(struct log_addr*);
