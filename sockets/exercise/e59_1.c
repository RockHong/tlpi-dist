#define BUFSIZE 100

struct buf_manager {
    char buf[BUFSIZE];
    int pos;
    size_t size;
}bm;

ssize_t readLineBuf(int fd, void *buffer, size_t n)
{
    bm.size = 0;
    bm.pos = 0;

    size_t numRead = 0;
    size_t todo = n;
    char *buf = buffer;

    size_t i = 0;

    while (todo > 0)
    {
        for(i = 0; i < bm.size; i++)
        {
           *buf++ = bm.buf[bm.pos++];
            numRead++; 
