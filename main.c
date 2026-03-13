#include<stdio.h>
#include<memory.h>
#include<unistd.h>
#include<sys/mman.h>
#include <stdint.h>

static size_t system_page_size=0;


void mm_init(){

    system_page_size=getpagesize();
}

void * mm_get_new_vm_from_kernel(int units){
    char *a=mmap(0,units*system_page_size,PROT_READ|PROT_WRITE|PROT_EXEC,MAP_ANON | MAP_PRIVATE,0,0);
    if(a==MAP_FAILED){
        return NULL;
    }
    memset(a,0,units*system_page_size);
    return a;
}

void  mm_get_return_vm_to_kernel(void *a,int units){
    if(munmap(a,units*system_page_size)){
        printf("Error with returning\n");
    }
}

typedef struct vm_family_{
    char name[32];
    uint32_t size;
    vm_page_t *page;
}vm_family_t;

typedef struct vm_families_{
    struct vm_families_ *next;
    vm_family_t pages[0];
}vm_families_t;

static vm_families_t* first;

#define NUM_FAMILIES \
    (system_page_size - sizeof(vm_families_t))/sizeof(vm_family_t)

#define ITERATE_PAGE_FAMILY_BEGIN(ptr,curr) \
    for(curr=(vm_family_t*)&ptr->pages[0];curr->size && count<NUM_FAMILIES;curr++,count++){ 

#define ITERATE_PAGE_FAMILY_END }

typedef struct vm_meta_block_{
    int32_t free;
    struct vm_meta_block_ *prev,*next;
    int32_t size;
    int32_t offset;
}vm_meta_block_t;

#define OFFSET_OFF(container,field) \
    ((size_t)&(((container*)0)->field))

#define GET_VIRTUAL_PAGE(meta_ptr) \
    (char *)(meta_ptr-meta_ptr->offset)

#define GET_NEXT_META_BLOCK(meta_ptr) \
    meta_ptr->next

#define GET_PREV_META_BLOCK(meta_ptr) \
    meta_ptr->prev

#define GET_NEXT_META_BLOCK_WITH_SIZE(meta_ptr) \
    (vm_meta_block_t*)((char *)(meta_ptr+1)+meta_ptr->size)

void mm_create_new_page(char * name,uint32_t size){
    vm_family_t *family=NULL;
    vm_families_t *families=NULL;
    if(size>system_page_size){
        printf("Structure size is bigger than system page size\n");
        return;
    }
    if(!first){
        first=mm_get_new_vm_from_kernel(1);
        first->next=NULL;
        strncpy(first->pages->name,name,strlen(name));
        first->pages[0].size=size;
        return;
    }
    uint32_t count=0;
    ITERATE_PAGE_FAMILY_BEGIN(first,family)

        if(strncmp(family->name,name,20)!=0){
            count++;
            continue;
        }
    ITERATE_PAGE_FAMILY_END
    if(count==NUM_FAMILIES){
        vm_families_t* new_family=mm_get_new_vm_from_kernel(1);
        new_family->next=first;
        first=new_family;
    }
    else{
        strncpy(family->name,name,20);
        family->size=size;
    }
}
void merge(vm_meta_block_t *b1,vm_meta_block_t *b2){
    if(!b1->free || !b2->free){
        return;
    }
    b1->size+=sizeof(vm_meta_block_t)+b2->size;
    b1->next=b1->next;
    if(b2->next!=NULL){
        b2->next->prev=b1;
    }
}

typedef struct vm_page_{
    struct vm_page_ *next;
    struct vm_page_ *prev;
    struct vm_family_t *pg_family;
    vm_meta_block_t block_data;
    char memory[0];
}vm_page_t;

int virtual_page_is_empty(vm_page_t *page){
    return page->block_data.next==NULL && page->block_data.prev==NULL && page->block_data.free;
}


static void make_vm_empty(vm_page_t *page){
    page->block_data.next=NULL;
    page->block_data.prev=NULL;
    page->block_data.free=1;
}



#define ITERATE_VM_PAGE_BEGIN(family,curr) \
    for(curr=(vm_page_t*)family->page;curr!=NULL;curr=curr->next){

#define ITERATE_VM_PAGE_END }

#define ITERATE_BLOCK_ITERATE_BEGIN(page,curr) \ 
    for(curr=(vm_page_t)&page->block_data;curr!=NULL;curr->next){

#define ITERATE_BLOCK_ITERATE_END }

static int maxnumber_off_free_bytes(int units){
    return units*system_page_size-OFFSET_OFF(vm_page_t,memory);
}

vm_page_t *allocate_new_page(vm_family_t *family){
    vm_page_t* new_page=mm_get_new_vm_from_kernel(1);
    make_vm_empty(new_page);
    new_page->block_data.size=maxnumber_off_free_bytes(1);
    new_page->block_data.offset=OFFSET_OFF(vm_page_t,block_data);
    new_page->next=NULL;
    new_page->prev=NULL;
    new_page->pg_family=family;
    if(family->page==NULL){
        family->page=new_page;
        return new_page;
    }
    family->page->prev=new_page;
    new_page->next=family->page;
    family->page=new_page;
    return new_page;
}

void delete_page(vm_page_t *page){
    vm_family_t *family=page->pg_family;
    if(family->page==page){
        family->page=family->page->next;
        page->prev=NULL;
        page->next=NULL;
        mm_get_return_vm_to_kernel((void *)page,1);
        return;
    }
    page->prev->next=page->next;
    if(page->next!=NULL){
        page->next->prev=page->prev;
    }
    page->next=NULL;
    page->prev=NULL;
    mm_get_return_vm_to_kernel((void *)page,1);
}

typedef struct a{
    int a,b;
}a;

int main(int argc,char *argv[]){
    mm_init();
    printf("VM page size = %d\n",system_page_size);
    void *a=mm_get_new_vm_from_kernel(4);
    void *b=mm_get_new_vm_from_kernel(3);
    printf("Addr1 %p Addr2 %p %d ",a,b,a-b);
    mm_create_new_page("a",sizeof(a));
    return 0;
}