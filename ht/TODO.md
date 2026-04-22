
# TODO

## Hash table

- Make sll_create and ht_create allocate and return a pointer
- Make sll_destroy and ht_destroy free the pointer
- Set MAX_CAPACITY macro
- Track current capacity and double it when ht.len >= 75% of current capacity
- Make a copy of ht and rehash everything modulo the new capacity
- Capacity must never exceed MAX_CAPACITY
