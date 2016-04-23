	struct member {
		struct member * next;
		char * data;
		int length;

	};

	struct hashset_st {
		int table_size;
		int items_count;
		struct member ** table; /*pole pointeru*/
		
	};
    
    typedef struct hashset_st *hashset_t;

    /* create hashset instance */
    hashset_t hashset_create(int table_size);

    /* destroy hashset instance */
    void hashset_destroy(hashset_t set);

    int hashset_num_items(hashset_t set);

    /* add item into the hashset.
     *
     * @note 0 and 1 is special values, meaning nil and deleted items. the
     *       function will return -1 indicating error.
     *
     * returns zero if the item already in the set and non-zero otherwise
     */
    int hashset_add(hashset_t set, void *item,int size);

 

