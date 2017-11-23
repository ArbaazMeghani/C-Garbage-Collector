#include "garbagecollector.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

struct memory_region{
  size_t * start;
  size_t * end;
};

struct memory_region global_mem;
struct memory_region heap_mem;
struct memory_region stack_mem;

void walk_region_and_mark(void* start, void* end);



//how many ptrs into the heap we have
#define INDEX_SIZE 1000
void* heapindex[INDEX_SIZE];


//grabbing the address and size of the global memory region from proc 
void init_global_range(){
  char file[100];
  char * line=NULL;
  size_t n=0;
  size_t read_bytes=0;
  size_t start, end;

  sprintf(file, "/proc/%d/maps", getpid());
  FILE * mapfile  = fopen(file, "r");
  if (mapfile==NULL){
    perror("opening maps file failed\n");
    exit(-1);
  }

  int counter=0;
  while ((read_bytes = getline(&line, &n, mapfile)) != -1) {
    if (strstr(line, "hw4")!=NULL){
      ++counter;
      if (counter==3){
        sscanf(line, "%lx-%lx", &start, &end);
        global_mem.start=(size_t*)start;
        // with a regular address space, our globals spill over into the heap
        global_mem.end=malloc(256);
        free(global_mem.end);
      }
    }
    else if (read_bytes > 0 && counter==3) {
      if(strstr(line,"heap")==NULL) {
        // with a randomized address space, our globals spill over into an unnamed segment directly following the globals
        sscanf(line, "%lx-%lx", &start, &end);
        printf("found an extra segment, ending at %zx\n",end);						
        global_mem.end=(size_t*)end;
      }
      break;
    }
  }
  fclose(mapfile);
}

//marking related operations

int is_marked(size_t* chunk) {
  return ((*chunk) & 0x2) > 0;
}

void mark(size_t* chunk) {
  (*chunk)|=0x2;
}

void clear_mark(size_t* chunk) {
  (*chunk)&=(~0x2);
}
// chunk related operations

#define chunk_size(c)  ((*((size_t*)c))& ~(size_t)3 ) 
void* next_chunk(void* c) { 
  if(chunk_size(c) == 0) {
    printf("Panic, chunk is of zero size.\n");
  }
  if((c+chunk_size(c)) < sbrk(0))
    return ((void*)c+chunk_size(c));
  else 
    return 0;
}
int in_use(void *c) { 
  return (next_chunk(c) && ((*(size_t*)next_chunk(c)) & 1));
}


// index related operations

#define IND_INTERVAL ((sbrk(0) - (void*)(heap_mem.start - 1)) / INDEX_SIZE)
void build_heap_index() {
  // TODO
}

// the actual collection code
void sweep() {
  //get the first chunk
  size_t * chnk_strt = heap_mem.start - 1;
  
  //while the chunk is valid and we have not passed end of heap
  while( chnk_strt && chnk_strt < (size_t *)sbrk(0)) {

    //get the start of mem which is chunk + 1
    size_t * mem_strt = chnk_strt + 1;
    //get the start of enxt chunk
    size_t * nxt_mem = ((size_t *)next_chunk(chnk_strt));
    
    //if the chunk is marked then it is not garbage so
    //don't free just unmark it
    if(is_marked(chnk_strt)) {
      clear_mark(chnk_strt);
    }
    //otherwise the chunk isn't marked so check if allocated before freeing
    else if(in_use(chnk_strt)) {
      free(mem_strt);
    }

    //go to next chunk
    chnk_strt = nxt_mem;
  }
}

//determine if what "looks" like a pointer actually points to a block in the heap
size_t * is_pointer(size_t * ptr) {
  //make sure ptr not null
  if(!ptr)
    return NULL;
  
  //make sure ptr within heap range
  if(ptr >= heap_mem.end || ptr < heap_mem.start)
    return NULL;

  //get the first chunk
  size_t * cur_chnk = heap_mem.start - 1;

  //while the chunk is valid and less than end of the heap
  while(cur_chnk && cur_chnk < (size_t *)sbrk(0)) {
    //get the start of memory
    size_t * mem_strt = cur_chnk + 1;
    //get tje start of next chunk
    size_t * mem_end = ((size_t *)next_chunk(cur_chnk));

    //if the ptr falls in range of current chunk return it
    if(ptr >= mem_strt && ptr < mem_end)
	return cur_chnk;

    //otherwise go to the next chunk
    cur_chnk = mem_end;
  }

  //if we reach this point, then the pointer points to a header so return NULL
  return NULL;	
}

/*
*  Function: Recursive mark function to mark pointers in heap
*  Parameters: pointer to a chunk in heap
*  Return: None; marks chunks
*/
void _mark(size_t * ptr) {

  //make sure valid ptr
  if(!ptr)
    return;

  //if ptr marked, it's already been checked
  if(is_marked(ptr))
    return;

  //mark the ptr
  mark(ptr);


  size_t i;

  //while less than the size of the chunk, check if any of them are pointers
  //to another chunk and mark if they are
  for(i = 1; i < ((size_t *)next_chunk(ptr)) - (ptr + 1); i++) {
    //dereference possible pointer on heap.
    //if it is a pointer it'll be some value, otherwise it'll be 0
    size_t * ptr_chnk = is_pointer((size_t *)*(ptr + i));

    //recursively call mark
    _mark(ptr_chnk);
  }
    
}

void walk_region_and_mark(void* start, void* end) {

  //start and end variables
  size_t * strt_cpy;
  size_t * end_cpy;

  //set start and end such that end has higher address than start
  if(start < end) {
    strt_cpy = (size_t *)start;
    end_cpy = (size_t *)end;
  }
  else {
    strt_cpy = (size_t *)end;
    end_cpy = (size_t *)start;
  }

  //while not at end
  while(strt_cpy < end_cpy) {

    //check if the value at address is a pointer
    size_t * ptr_chnk = is_pointer((size_t *)*strt_cpy);
    //call mark on it
    _mark(ptr_chnk);

    //get next address
    strt_cpy += 1;
  }  
}

// standard initialization 

void init_gc() {
  size_t stack_var;
  init_global_range();
  heap_mem.start=malloc(512);
  //since the heap grows down, the end is found first
  stack_mem.end=(size_t *)&stack_var;
}

void gc() {
  size_t stack_var;
  heap_mem.end=sbrk(0);
  //grows down, so start is a lower address
  stack_mem.start=(size_t *)&stack_var;

  // build the index that makes determining valid ptrs easier
  // implementing this smart function for collecting garbage can get bonus;
  // if you can't figure it out, just comment out this function.
  // walk_region_and_mark and sweep are enough for this project.
  //build_heap_index();

  //walk memory regions
  walk_region_and_mark(global_mem.start,global_mem.end);
  walk_region_and_mark(stack_mem.start,stack_mem.end);

  sweep();
}
