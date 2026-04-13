
# TODO

## Hash table

- ht.capacity never changes once set in ht_create
- Track current bucket limit and double it when ht.len >= 75% of bucket limit
- Make a copy of ht and rehash everything modulo the bucket limit
- Bucket limit must never exceed ht.capacity
