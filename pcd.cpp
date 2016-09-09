#include "pcd.h"
#include <stdlib.h>
#include <string.h>

static unsigned int
lzfDecompress(const void *const in_data, unsigned int in_len,
              void *out_data, unsigned int out_len)
{
    unsigned char const *ip = static_cast < const unsigned char *>(in_data);
    unsigned char *op = static_cast < unsigned char *>(out_data);
    unsigned char const *const in_end = ip + in_len;
    unsigned char *const out_end = op + out_len;

    do {
        unsigned int ctrl = *ip++;

        //Literal run
        if (ctrl < (1 << 5)) {
            ctrl++;

            if (op + ctrl > out_end) {
                return (0);
            }
            //Check for overflow
            if (ip + ctrl > in_end) {
                return (0);
            }
            switch (ctrl) {
            case 32:
                *op++ = *ip++;
            case 31:
                *op++ = *ip++;
            case 30:
                *op++ = *ip++;
            case 29:
                *op++ = *ip++;
            case 28:
                *op++ = *ip++;
            case 27:
                *op++ = *ip++;
            case 26:
                *op++ = *ip++;
            case 25:
                *op++ = *ip++;
            case 24:
                *op++ = *ip++;
            case 23:
                *op++ = *ip++;
            case 22:
                *op++ = *ip++;
            case 21:
                *op++ = *ip++;
            case 20:
                *op++ = *ip++;
            case 19:
                *op++ = *ip++;
            case 18:
                *op++ = *ip++;
            case 17:
                *op++ = *ip++;
            case 16:
                *op++ = *ip++;
            case 15:
                *op++ = *ip++;
            case 14:
                *op++ = *ip++;
            case 13:
                *op++ = *ip++;
            case 12:
                *op++ = *ip++;
            case 11:
                *op++ = *ip++;
            case 10:
                *op++ = *ip++;
            case 9:
                *op++ = *ip++;
            case 8:
                *op++ = *ip++;
            case 7:
                *op++ = *ip++;
            case 6:
                *op++ = *ip++;
            case 5:
                *op++ = *ip++;
            case 4:
                *op++ = *ip++;
            case 3:
                *op++ = *ip++;
            case 2:
                *op++ = *ip++;
            case 1:
                *op++ = *ip++;
            }
        }
        //Back reference
        else {
            unsigned int	len = ctrl >> 5;

            unsigned char  *ref = op - ((ctrl & 0x1f) << 8) - 1;

            //Check for overflow
            if (ip >= in_end) {
                return (0);
            }
            if (len == 7) {
                len += *ip++;
                //Check for overflow
                if (ip >= in_end) {
                    return (0);
                }
            }
            ref -= *ip++;

            if (op + len + 2 > out_end) {
                return (0);
            }
            if (ref < static_cast < unsigned char *>(out_data)) {
                return (0);
            }
            switch (len) {
            default:
            {
                len += 2;

                if (op >= ref + len) {
                    //Disjunct areas
                    memcpy(op, ref, len);
                    op += len;
                } else {
                    //Overlapping, use byte by byte copying
                    do
                        *op++ = *ref++;
                    while (--len);
                }

                break;
            }
            case 9:
                *op++ = *ref++;
            case 8:
                *op++ = *ref++;
            case 7:
                *op++ = *ref++;
            case 6:
                *op++ = *ref++;
            case 5:
                *op++ = *ref++;
            case 4:
                *op++ = *ref++;
            case 3:
                *op++ = *ref++;
            case 2:
                *op++ = *ref++;
            case 1:
                *op++ = *ref++;
            case 0:
                *op++ = *ref++;
                //two octets more
                *op++ = *ref++;
            }
        }
    }
    while (ip < in_end);

    return (static_cast<unsigned int>(op - static_cast<unsigned char *>(out_data)));
}

pcd_t *load_pcd_from_buffer(const char *buf)
{
    int            width = 0;
    int            height = 0;
    int            pos = 0;

    const char     *p;
    char           *decompressed_buf;
    unsigned int    compressed_size, decompressed_size;
    pcd_t          *ret;

    int	            i, j;

    p = strstr(buf, "WIDTH ");
    width = atoi(p + 6);

    p = strstr(buf, "HEIGHT ");
    height = atoi(p + 7);

    p = strstr(buf, "DATA binary_compressed");
    p += 23;

    memcpy(&compressed_size, p, sizeof(unsigned int));
    p += 4;

    memcpy(&decompressed_size, p, sizeof(unsigned int));
    p += 4;

    decompressed_buf = (char *)malloc(decompressed_size);

    if (lzfDecompress(p, compressed_size, decompressed_buf, decompressed_size) == 0) {
        free(decompressed_buf);
        return NULL;
    }
    int size = width * height;
    ret = (pcd_t *) malloc(sizeof(pcd_t) + sizeof(point_t) * (size - 1));

    ret->width = width;
    ret->height = height;

    p = decompressed_buf;

    for (i = 0; i < size; i++) {
        memcpy(&(ret->points[i].x), p + sizeof(float) * i, sizeof(float));
        memcpy(&(ret->points[i].y), p + sizeof(float) * i + (sizeof(float) * size), sizeof(float));
	memcpy(&(ret->points[i].z), p + sizeof(float) * i + (sizeof(float) * size * 2), sizeof(float));
        // ret->points[i].intensity = *(p + i + (sizeof(float) * size * 3));
        // memcpy(&(ret->points[i].timestamp), p + (sizeof(float) * size * 3) + size + (sizeof(double) * i), sizeof(double));
    }

    free(decompressed_buf);
    return ret;
}
