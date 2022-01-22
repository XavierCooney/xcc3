#define LIST_STRUCT_APPEND_FUNC(ItemType, struct_name, list_length, list_allocated, list_data, new_item) \
    if(struct_name->list_length >= struct_name->list_allocated) { \
        size_t new_allocated = 2 * (struct_name->list_length + 1); \
        void *new_allocation = xcc_malloc(sizeof(ItemType) * new_allocated); \
        if(struct_name->list_data) { \
            memcpy(new_allocation, struct_name->list_data, sizeof(ItemType) * struct_name->list_length); \
            xcc_free(struct_name->list_data); \
        } \
        struct_name->list_data = new_allocation; \
        struct_name->list_allocated = new_allocated; \
    } \
    new_item = struct_name->list_data + struct_name->list_length; \
    ++struct_name->list_length;
