/*
 * StarPU
 * Copyright (C) Université Bordeaux 1, CNRS 2008-2010 (see AUTHORS file)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU Lesser General Public License in COPYING.LGPL for more details.
 */

#include <starpu.h>
#include <common/config.h>
#include <common/htable32.h>
#include <stdint.h>
#include <string.h>

void *_starpu_htbl_search_32(struct starpu_htbl32_node_s *htbl, uint32_t key)
{
	unsigned currentbit;
	unsigned keysize = 32;

	starpu_htbl32_node_t *current_htbl = htbl;

	/* 000000000001111 with HTBL_NODE_SIZE 1's */
	uint32_t mask = (1<<STARPU_HTBL32_NODE_SIZE)-1;

	for(currentbit = 0; currentbit < keysize; currentbit+=STARPU_HTBL32_NODE_SIZE)
	{
	
	//	printf("search : current bit = %d \n", currentbit);
		if (STARPU_UNLIKELY(current_htbl == NULL))
			return NULL;

		/* 0000000000001111 
		 *     | currentbit
		 * 0000111100000000 = offloaded_mask
		 *         |last_currentbit
		 * */

		unsigned last_currentbit = 
			keysize - (currentbit + STARPU_HTBL32_NODE_SIZE);
		uint32_t offloaded_mask = mask << last_currentbit;
		unsigned current_index = 
			(key & (offloaded_mask)) >> (last_currentbit);

		current_htbl = current_htbl->children[current_index];
	}

	return current_htbl;
}

/*
 * returns the previous value of the tag, or NULL else
 */

void *_starpu_htbl_insert_32(struct starpu_htbl32_node_s **htbl, uint32_t key, void *entry)
{
	unsigned currentbit;
	unsigned keysize = 32;

	starpu_htbl32_node_t **current_htbl_ptr = htbl;

	/* 000000000001111 with HTBL_NODE_SIZE 1's */
	uint32_t mask = (1<<STARPU_HTBL32_NODE_SIZE)-1;

	for(currentbit = 0; currentbit < keysize; currentbit+=STARPU_HTBL32_NODE_SIZE)
	{
		//printf("insert : current bit = %d \n", currentbit);
		if (*current_htbl_ptr == NULL) {
			/* TODO pad to change that 1 into 16 ? */
			*current_htbl_ptr = calloc(sizeof(starpu_htbl32_node_t), 1);
			assert(*current_htbl_ptr);
		}

		/* 0000000000001111 
		 *     | currentbit
		 * 0000111100000000 = offloaded_mask
		 *         |last_currentbit
		 * */

		unsigned last_currentbit = 
			keysize - (currentbit + STARPU_HTBL32_NODE_SIZE);
		uint32_t offloaded_mask = mask << last_currentbit;
		unsigned current_index = 
			(key & (offloaded_mask)) >> (last_currentbit);

		current_htbl_ptr = 
			&((*current_htbl_ptr)->children[current_index]);
	}

	/* current_htbl either contains NULL or a previous entry 
	 * we overwrite it anyway */
	void *old_entry = *current_htbl_ptr;
	*current_htbl_ptr = entry;

	return old_entry;
}
