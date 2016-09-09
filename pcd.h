#ifndef PCD_H
#define PCD_H

typedef struct {
    float x;
    float y;
    float z;
    // unsigned char intensity;
    // double timestamp;
} point_t;

typedef struct {
    int width;
    int height;
    point_t points[1];
} pcd_t;

pcd_t *load_pcd_from_buffer(const char *buf);

#endif /* PCD_H */
