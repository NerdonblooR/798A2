#define INITIAL_SIZE 32

#define htonll(x) ((1==htonl(1)) ? (x) : ((uint64_t)htonl((x) & 0xFFFFFFFF) << 32) | htonl((x) >> 32))
#define ntohll(x) ((1==ntohl(1)) ? (x) : ((uint64_t)ntohl((x) & 0xFFFFFFFF) << 32) | ntohl((x) >> 32))


struct Buffer {
    size_t next;
    size_t size;
    char *data;
};

struct Buffer *new_buffer() {
    struct Buffer *b = malloc(sizeof(Buffer));
    b->data = malloc(INITIAL_SIZE);
    b->size = INITIAL_SIZE;
    b->next = 0;
    return b;
}

void reserve_space(Buffer *b, size_t bytes) {
    if ((b->next + bytes) > b->size) {
        /* double size to enforce O(lg N) reallocs */
        b->data = realloc(b->data, b->size * 2);
        b->size *= 2;
    }
}

void serialize_size_t(size_t x, Buffer *b) {
    /* assume int == long; how can this be done better? */
    x = htonll(x);
    reserve_space(b, sizeof(x));
    memcpy(b->data + b->next, &x, sizeof(x));
    b->next += sizeof(x);
}


void serialize_str(char *x, Buffer *b) {
    reserve_space(b, strlen(x) + 1);
    strcpy(b->data + b->next, x);
    b->next += strlen(x) + 1;
}

//deserialize str from next
void deserialize_size_t(size_t *x, char *b, size_t offset) {
    size_t tmp = 0;
    memcpy(&tmp, b + offset, sizeof(tmp));
    *x = ntohll(tmp);
}


//deserialie str from next
void deserialize_str(char *x, char *b, size_t offset, size_t len) {
    memcpy(x, b + offset, len);
}



