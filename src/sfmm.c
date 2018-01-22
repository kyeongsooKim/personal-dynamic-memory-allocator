/**
 * All functions you make for the assignment must be implemented in this file.
 * Do not submit your assignment with a main function in this file.
 * If you submit with a main function in this file, you will get a zero.
 */
#include "sfmm.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#define ALIGNMENT_SIZE 16
#define MAX_PAGE_NUMBER 4
#define SPLINTER_SIZE 32

/**
 * You should store the heads of your free lists in these variables.
 * Doing so will make it accessible via the extern statement in sfmm.h
 * which will allow you to pass the address to sf_snapshot in a different file.
 */
free_list seg_free_list[4] = {
    {NULL, LIST_1_MIN, LIST_1_MAX},
    {NULL, LIST_2_MIN, LIST_2_MAX},
    {NULL, LIST_3_MIN, LIST_3_MAX},
    {NULL, LIST_4_MIN, LIST_4_MAX}
};

int sf_errno = 0;
int sbrk_life = MAX_PAGE_NUMBER;
/* Util function for sf_malloc, sf_realloc and sf_free */

sf_free_header * best_fit(size_t block);
void util_valid_check(sf_free_header * cur, sf_header * cur_header, sf_footer * cur_footer);
void LIFO(sf_free_header * cur);
int util_find_list_index(int sz);


void *sf_malloc(size_t size) {

    bool isPadded = false;

    size_t block = size + SF_HEADER_SIZE/8 + SF_FOOTER_SIZE/8;
    if (block%ALIGNMENT_SIZE == 0)
        isPadded = false;
    else
    {
        isPadded = true;
        block = (block/ALIGNMENT_SIZE)*ALIGNMENT_SIZE + ALIGNMENT_SIZE;
    }

    //block size validation
    if (size == 0)
    {
        #ifdef DEBUG
        fprintf(stderr, "%s\n", "ERROR : reqeust size is 0");
        #endif
        sf_errno = EINVAL;
        return NULL;
    }
    else if (size > 4*PAGE_SZ)
    {
        #ifdef DEBUG
        fprintf(stderr, "%s\n", "ERROR : reqeust size is more than 4*PAGE_SIZE");
        #endif
        sf_errno = EINVAL;
        return NULL;
    }

    if (sbrk_life == 4) //first allocation
    {
        sf_sbrk();
        sbrk_life--;
        seg_free_list[3].head = (sf_free_header *)get_heap_start();
        (seg_free_list[3].head)->next = NULL;
        (seg_free_list[3].head)->prev = NULL;
        ((seg_free_list[3].head)->header).block_size = (PAGE_SZ>>4);

        sf_footer * footer = (sf_footer *) (&((seg_free_list[3].head)->header)
                                            + PAGE_SZ/(SF_HEADER_SIZE/8) - 1);
        footer->block_size = (PAGE_SZ>>4);

        #ifdef DEBUG
        printf("first allocation, call sf_sbrk()");
        printf(" (heap : from %p to ", (sf_free_header *)get_heap_start());
        printf("%p)\n", (sf_free_header *)get_heap_end());
        sf_snapshot();
        #endif
    }


    #ifdef DEBUG
    printf("\nrequested size : %ld, block : %ld", size, block);
    printf("\t(heap : from %p to ", (sf_free_header *)get_heap_start());
    printf("%p)\n", (sf_free_header *)get_heap_end());
    #endif

    //just in case LIST_1_MIN is modified, so minimum block is not guaranteed
    if( block < LIST_1_MIN) return NULL;


    //use best_fit algorithm
    sf_free_header * best_fit_header = best_fit(block);

    //make retAdr pointer in case of calling sf_malloc() recursively.
    sf_header * retAdr = NULL;
    /*No fit found, expand heap by calling sf_sbrk()*/
    if( best_fit_header == NULL && sbrk_life > 0)
    {
        sf_footer * prev_adj_footer = (sf_footer *)get_heap_end();
        prev_adj_footer--;
        sf_sbrk();
        sbrk_life--;

        if(prev_adj_footer->allocated == 0x0)
        {
             //coalescing with lower address
            sf_footer * footer = (sf_footer*)get_heap_end();
            footer--;

            sf_header * prev_adj_header = (sf_header *)(prev_adj_footer
                        - ((prev_adj_footer->block_size)<<4)/(SF_FOOTER_SIZE/8) + 1);
            sf_free_header * new_free_header = (sf_free_header *)prev_adj_header;
            sf_free_header * cur = (sf_free_header * )prev_adj_header;

            for (int i = 0; i < 4 ;i++)
            {
                if ((((cur->header).block_size)<<4) <= seg_free_list[i].max )
                {
                    if(seg_free_list[i].head == cur)
                        seg_free_list[i].head = cur->next;
                    if(seg_free_list[i].head != NULL)
                        (seg_free_list[i].head)->prev = NULL;
                    break;
                }
            }

            if(cur->next !=NULL)
                cur->next->prev = cur->prev;
            if(cur->prev !=NULL)
                cur->prev->next = cur->next;

            (new_free_header->header).block_size = (PAGE_SZ>>4) + (prev_adj_footer->block_size);
            footer->block_size =  (new_free_header->header).block_size;
            (prev_adj_footer->block_size) = 0x0;

            //update linked free list
            new_free_header->prev = NULL;
            new_free_header->next = seg_free_list[3].head;
            if(seg_free_list[3].head != NULL)
                seg_free_list[3].head->prev = new_free_header;
            (seg_free_list[3].head) = new_free_header;
        }
        //when previous adjacent footer is allocated block,
        //no need to do coalescing with lower address
        else
        {
            prev_adj_footer++;
            sf_free_header * new_free_header = (sf_free_header *) prev_adj_footer;
            new_free_header->prev = NULL;
            new_free_header->next = seg_free_list[3].head;
            if(seg_free_list[3].head != NULL)
                seg_free_list[3].head->prev = new_free_header;
            (seg_free_list[3].head) = new_free_header;

            ((seg_free_list[3].head)->header).block_size = (PAGE_SZ>>4);

            sf_footer * footer = (sf_footer*)get_heap_end();
            footer--;

            footer->block_size = (PAGE_SZ>>4);

        }


        #ifdef DEBUG
        printf("No fit found, expanded heap \t\t(heap : from %p to %p)\n", (sf_free_header *)get_heap_start(),(sf_free_header *)get_heap_end());
        sf_snapshot();
        #endif

        //try again
        retAdr = sf_malloc(size);
    }
    //request cannot be satisfied
    else if (best_fit_header == NULL && sbrk_life == 0)
    {
        #ifdef DEBUG
        fprintf(stderr, "%s\n", "ERROR : already called sf_sbrk() 4 times. no more expansion.");
        #endif
        sf_errno = ENOMEM;
        return NULL;
    }
    //request is satisfied
    else
    {
        #ifdef DEBUG
        printf("\n\n******    allocation succeeded! header : %p, ", best_fit_header);
        #endif

        //set properties for header of the allocated block
        if(isPadded == true)
            (best_fit_header->header).padded = 0x1;
        else
            (best_fit_header->header).padded = 0x0;

        sf_footer * footer = (sf_footer *)(&(best_fit_header->header) +
                            (((best_fit_header->header).block_size)<<4)/(SF_HEADER_SIZE/8) - 1
                            );
        #ifdef DEBUG
        printf("footer : %p   ******\n\n",footer);
        #endif

        //set properties for footer of the allocated block
        footer->allocated = (best_fit_header->header).allocated;
        footer->two_zeroes = (best_fit_header->header).two_zeroes;
        footer->block_size = (best_fit_header->header).block_size;
        footer->padded = (best_fit_header->header).padded;
        footer->requested_size = size;


        //return the valid region of memory of the requested size.
        sf_header * retAdr2 = &(best_fit_header->header);
        retAdr2++;
        return (void *)(retAdr2);
    }

    return retAdr;

}

void *sf_realloc(void *ptr, size_t size) {


    if((char * )ptr > (char * )get_heap_end() || (char * )ptr < (char * )get_heap_start())
    {
        #ifdef DEBUG
        fprintf(stderr,"invalid ptr\n");
        #endif

        abort();
    }

    #ifdef DEBUG
        printf("$$$$$$$$$$$$$$$$$\t\t sf_realloc()is called\t\t$$$$$$$$$$$$$$$\n\t\t\t\trequest block info\n");
        sf_varprint(ptr);
    #endif

    if(ptr == NULL)
    {
        abort();
    }

    sf_header * tmp = (sf_header *)ptr;
    tmp--;
    sf_free_header * cur = (sf_free_header * )tmp;
    sf_header * cur_header = &(cur->header);
    sf_footer * cur_footer = (sf_footer *)(cur_header + ((cur_header->block_size)<<4)/(SF_FOOTER_SIZE/8) - 1);

    util_valid_check(cur, cur_header, cur_footer);

    if(size == 0)
    {
        sf_free(ptr);
        return NULL;
    }

    bool isPadded = false;
    size_t block = size + SF_HEADER_SIZE/8 + SF_FOOTER_SIZE/8;
    if (block%ALIGNMENT_SIZE == 0)
        isPadded = false;
    else
    {
        isPadded = true;
        block = (block/ALIGNMENT_SIZE)*ALIGNMENT_SIZE + ALIGNMENT_SIZE;
    }






    //requested_size is larger
    if (cur_footer->requested_size < size)
    {
        if((cur_footer->block_size)<<4 == block) //when block size is same but requested size is different
        {
            #ifdef DEBUG
            printf("\nRequested size is larger, but block size is still same, only renew some property in header and footer\n\n");
            #endif

            cur_footer->requested_size = size;

            if (isPadded)
            {
                cur_header->padded = 0x1;
                cur_footer->padded = 0x1;
            }
            else
            {
                cur_header->padded = 0x0;
                cur_footer->padded = 0x0;
            }

            #ifdef DEBUG
             printf("\t\t\trenewed block info!!\n\n");
             sf_blockprint(cur);
            printf("\n$$$$$$$$$$$$$$$$$\t\t sf_relloc()is done\t\t$$$$$$$$$$$$$$$\n\n\n");
            #endif
            return ptr;
        }
        else
        {
            #ifdef DEBUG
             printf("\nRealloc to larger block!\n");
            #endif

            void * isAlloacted = NULL;
            isAlloacted = sf_malloc(size);

            if(isAlloacted)
            {
                size_t payload_size = ((cur_header->block_size)<<4) - SF_HEADER_SIZE/8 - SF_FOOTER_SIZE/8;
                memcpy(isAlloacted, ptr, payload_size);
                sf_free(ptr);

                #ifdef DEBUG
                printf("\t\t\tnew allocated block info\n\n");
                sf_blockprint(cur);
                printf("\n$$$$$$$$$$$$$$$$$\t\t sf_relloc()is done\t\t$$$$$$$$$$$$$$$\n\n\n");
                #endif

            }
            else{
                #ifdef DEBUG
                printf("\t\t\tFail to realloc\n\n");
                #endif
            }


            #ifdef DEBUG
            printf("\n$$$$$$$$$$$$$$$$$\t\t sf_relloc()is done\t\t$$$$$$$$$$$$$$$\n\n\n");
            #endif
            return isAlloacted;


        }
    }
    //requested size is smaller
    else if (cur_footer->requested_size > size)
    {

        if(((cur_footer->block_size)<<4) == block)
        {
            #ifdef DEBUG
             printf("\nRequested size is smaller but block size is still same, so only renew some property in header and footer\n\n");
             #endif

            cur_footer->requested_size = size;

            if (isPadded)
            {
                cur_header->padded = 0x1;
                cur_footer->padded = 0x1;
            }
            else
            {
                cur_header->padded = 0x0;
                cur_footer->padded = 0x0;
            }


            #ifdef DEBUG
             printf("\t\t\trenewed block info\n\n");
             sf_blockprint(cur_header);
            printf("\n$$$$$$$$$$$$$$$$$\t\t sf_relloc()is done\t\t$$$$$$$$$$$$$$$\n\n\n");
            #endif
            return ptr;
        }
        //Do not split the block if splitting the returned block results in a splinter.
        else if(((cur_footer->block_size)<<4) - block < SPLINTER_SIZE)
        {
            #ifdef DEBUG
             printf("\nRealloc to smaller block, splitting would make splinter, only renew some property in header and footer\n\n");
             #endif
            cur_footer->requested_size = size;

            //since we consider slinter, we should add padded bit.
            cur_header->padded = 0x1;
            cur_footer->padded = 0x1;


            #ifdef DEBUG
             printf("\t\t\trenewed block info\n\n");
             sf_blockprint(cur);
            printf("\n$$$$$$$$$$$$$$$$$\t\t sf_relloc()is done\t\t$$$$$$$$$$$$$$$\n\n\n");
            #endif
            return ptr;
        }
        else //splitting
        {
             #ifdef DEBUG
             printf("\nRealloc to smaller block with splitting\n\t\t\t");
             #endif
            sf_free_header * new = cur;
            sf_header * new_header = &(new->header);
            sf_footer * new_footer = (sf_footer *)(new_header + block/(SF_FOOTER_SIZE/8) - 1);

            sf_header * adj_header = (sf_header *)(new_footer + 1);
            sf_footer * adj_footer = (sf_footer *)cur_footer;

            size_t tmp_block_size = ((cur_header->block_size)<<4)- block;


            adj_header->block_size = (tmp_block_size>>4);
            adj_footer->block_size = adj_header->block_size;
            adj_header->padded = 0x0;
            adj_footer->padded = 0x0;
            adj_header->allocated = adj_footer->allocated;
            adj_footer->requested_size = (tmp_block_size - SF_HEADER_SIZE/8 - SF_FOOTER_SIZE/8);

             #ifdef DEBUG
             printf("\t\t\tnew block to be freed\n\n");
             sf_blockprint(adj_header);
             #endif


            new_header->block_size = (block>>4);
            new_footer->block_size = (block>>4);
            new_footer->requested_size = size;
            new_footer->allocated = 0x1;
            if(isPadded)
            {
                new_header->padded = 0x1;
                new_footer->padded = 0x1;
            }
            else
            {
                new_header->padded = 0x0;
                new_footer->padded = 0x0;
            }


            #ifdef DEBUG
             printf("\t\t\tnew block to be allocated\n\n");
             sf_blockprint(new);
            #endif

            sf_free(adj_header + 1);

            #ifdef DEBUG
            printf("$$$$$$$$$$$$$$$$$\t\t sf_relloc()is done\t\t$$$$$$$$$$$$$$$\n\n\n");
            #endif
            return (new_header + 1);

        }
    }
    else //when requested size in realloc is same as when alloc is called
    {
        //do nothing
        return (void * )ptr;
    }


	return NULL;
}

void sf_free(void *ptr) {


    if((char * )ptr > (char * )get_heap_end() || (char * )ptr < (char * )get_heap_start())
    {
        #ifdef DEBUG
        fprintf(stderr,"invalid ptr\n");
        #endif

        abort();
    }

    #ifdef DEBUG
        printf("$$$$$$$$$$$$$$$$$\t\t sf_free()is called\t\t$$$$$$$$$$$$$$$\n\t\t\t\tcurrent free lists\n\n");
        sf_snapshot();
    #endif

    if(ptr == NULL)
    {
        abort();
    }

    sf_header * tmp = (sf_header *)ptr;
    tmp--;
    sf_free_header * cur = (sf_free_header *) tmp;
    sf_header * cur_header = &(cur->header);
    sf_footer * cur_footer = (sf_footer *)(cur_header + ((cur_header->block_size)<<4)/(SF_FOOTER_SIZE/8) - 1);



    printf("%p", ptr);

    util_valid_check(cur, cur_header, cur_footer);


    //immediate coalescing with higher memory
    if(cur_footer + 1 <= (sf_footer *)get_heap_end() && (cur_footer + 1)->allocated == 0x0)
    {
        #ifdef DEBUG
        printf("\n\nimmediate coalescing is done\n\n");
        #endif
        sf_header * adj_header = (sf_header *) (cur_footer + 1);
        sf_free_header * adj = (sf_free_header *)adj_header;
        sf_footer * adj_footer = (sf_footer *)(adj_header + ((adj_header->block_size)<<4)/(SF_FOOTER_SIZE/8) - 1);



        int k = util_find_list_index((adj_header->block_size)<<4);

        cur_header->block_size += adj_footer->block_size;
        adj_footer->block_size = cur_header->block_size;
        cur_footer->block_size = 0;
        adj_header->block_size = 0;

        if(adj->next != NULL)
            adj->next->prev = adj->prev;
        if(adj->prev != NULL)
            adj->prev->next = adj->next;
        if (seg_free_list[k].head == adj)
        {
            seg_free_list[k].head = adj->next;
        }
        adj->next = NULL;
        adj->prev = NULL;
    }

    cur_header->allocated = 0x0;
    cur_header->padded = 0x0;
    cur_footer->allocated = 0x0;
    cur_footer->padded = 0x0;
    cur_footer->requested_size = 0;
    LIFO(cur);

    #ifdef DEBUG
        printf("\t\t\trenewed free lists info\n\n");
        sf_snapshot();
        printf("$$$$$$$$$$$$$$$$$\t\t sf_free()is done\t\t$$$$$$$$$$$$$$$\n\n\n");

    #endif

	return;
}





/************************* Util Function for sf_malloc, sf_realloc and sf_free **********************/


// choose best fit freed block
sf_free_header * best_fit(size_t block) {


    sf_free_header * cur;
    for (int i = 0; i < 4 ;i++)
    {
        cur = seg_free_list[i].head;
        if (block <= seg_free_list[i].max)
        {
            while(cur != NULL && (((cur->header).block_size)<<4) < (uint64_t)block)
                cur = cur->next;
            if (cur != NULL)
            {
                //when splitting a block creates splinter, do not split it.
                if ((((cur->header).block_size)<<4) - block < 32)
                {

                    #ifdef DEBUG
                    printf("\nno splitting. header moved from %p ", seg_free_list[i].head);
                    #endif


                    //when we don't split it, we consider it as a padding.
                    (cur->header).padded = 0x1;

                    //update the free_list.
                    if(cur->next != NULL)
                        cur->next->prev = cur->prev;
                    if(cur->prev != NULL)
                        cur->prev->next = cur->next;

                    if(seg_free_list[i].head == cur)
                        seg_free_list[i].head = cur->next;

                    //renew the property of the block to be allocated
                    (cur->header).padded = 0x1;
                    cur->next = NULL;
                    cur->prev = NULL;
                    #ifdef DEBUG
                    printf("to %p\n", cur); sf_snapshot();
                    #endif
                    (cur->header).allocated = 0x1;
                    (cur->header).two_zeroes = 0x0;
                    return cur;
                }
                else
                {
                    uint64_t tmp_blockSize = ((cur->header).block_size)<<4;

                    #ifdef DEBUG
                    printf("\nsplitting is done. header moved from %p ", cur);
                    #endif

                    //spilt a block and move the head in free list
                    sf_free_header * retAdr = cur;
                    sf_header * tmp = (sf_header *) cur;
                    tmp += (block/(SF_HEADER_SIZE/8));
                    cur = (sf_free_header *)tmp;

                    //renew the new block size for new header and footer in free list
                    (cur->header).block_size = (tmp_blockSize - block)>>4;
                    sf_footer * footer = (sf_footer *) (&(cur->header)
                    + (tmp_blockSize - block)/(SF_HEADER_SIZE/8) - 1);
                    footer->block_size = (cur->header).block_size;



                    //move the new header to the correct list if needed
                    if( (((cur->header).block_size)<<4) < seg_free_list[i].min)
                    {
                        LIFO(cur);

                        if(cur->next !=NULL)
                                cur->next->prev = cur->prev;
                        if(cur->prev !=NULL)
                                cur->prev->next = cur->next;

                        if(retAdr == seg_free_list[i].head)
                            seg_free_list[i].head = cur->next;
                    }
                    else
                        seg_free_list[i].head = cur;
                    #ifdef DEBUG
                    printf("to %p\n", cur); sf_snapshot();
                    #endif
                    //renew the property for allocated block
                    (retAdr->header).block_size = (block>>4);
                    (retAdr->header).allocated = 0x1;
                    (retAdr->header).two_zeroes = 0x0;
                    return retAdr;
                }


            }
        }
    }

    return NULL;
}

void LIFO(sf_free_header * cur)
{
    sf_free_header * prev_header;
    for (int i = 0; i < 4 ;i++)
    {
        prev_header = seg_free_list[i].head;
        if ((((cur->header).block_size)<<4) <= seg_free_list[i].max )
        {
            seg_free_list[i].head = cur;
            (seg_free_list[i].head)->next = prev_header;
            (seg_free_list[i].head)->prev = NULL;
            if (prev_header != NULL)
                prev_header->prev = (seg_free_list[i].head);
            break;
        }
    }
}


void util_valid_check(sf_free_header *cur, sf_header *cur_header , sf_footer * cur_footer)
{
    if(cur_header->allocated == 0x0 || cur_footer->allocated == 0x0)
    {
        #ifdef DEBUG
        fprintf(stderr,"allocated bit is not 1\n");
        #endif
        abort();
    }
    if(cur_header < (sf_header * )get_heap_start())
    {
        #ifdef DEBUG
        fprintf(stderr,"header comes before heap_start\n");
        #endif

        abort();
    }
    if(cur_footer + 1 > (sf_footer *)get_heap_end())
    {
        #ifdef DEBUG
        fprintf(stderr,"end bit of footer comes later than heap_end\n");
        #endif

        abort();
    }
    if(cur_footer->padded != cur_header->padded || cur_header->allocated != cur_footer->allocated)
    {
        #ifdef DEBUG
        fprintf(stderr,"padded bits or allocated bits in header and footer do not match\n");
        #endif
        abort();
    }
    if((cur_footer->requested_size + ALIGNMENT_SIZE != cur_footer->block_size && cur_footer->padded == 0x0)
        &&(cur_footer->requested_size + ALIGNMENT_SIZE == cur_footer->block_size && cur_footer->padded == 0x1)
        )
    {
        #ifdef DEBUG
        fprintf(stderr,"requested_size and block_size and padded size do not make sense together\n");
        #endif
        abort();
    }
}

int util_find_list_index(int sz) {
    if (sz >= LIST_1_MIN && sz <= LIST_1_MAX) return 0;
    else if (sz >= LIST_2_MIN && sz <= LIST_2_MAX) return 1;
    else if (sz >= LIST_3_MIN && sz <= LIST_3_MAX) return 2;
    else return 3;
}