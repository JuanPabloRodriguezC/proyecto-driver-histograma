#include "image_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <jpeglib.h>

// RAW
uint8_t *load_raw_image(const char *path, int *width, int *height)
{
    FILE *f = fopen(path, "rb");
    if (!f)
    {
        perror("fopen");
        return NULL;
    }

    if (fread(width, sizeof(int), 1, f) != 1)
    {
        fprintf(stderr, "Error leyendo width\n");
        fclose(f);
        return NULL;
    }

    if (fread(height, sizeof(int), 1, f) != 1)
    {
        fprintf(stderr, "Error leyendo height\n");
        fclose(f);
        return NULL;
    }

    size_t n = (size_t)(*width) * (*height);
    uint8_t *img = (uint8_t *)malloc(n);
    if (!img)
    {
        fprintf(stderr, "Sin memoria para imagen\n");
        fclose(f);
        return NULL;
    }

    if (fread(img, 1, n, f) != n)
    {
        fprintf(stderr, "Error leyendo pixeles\n");
        free(img);
        fclose(f);
        return NULL;
    }

    fclose(f);
    return img;
}

int save_raw_image(const char *path, int width, int height, const uint8_t *data)
{
    FILE *f = fopen(path, "wb");
    if (!f)
    {
        perror("fopen");
        return -1;
    }

    if (fwrite(&width, sizeof(int), 1, f) != 1)
    {
        fprintf(stderr, "Error escribiendo width\n");
        fclose(f);
        return -1;
    }

    if (fwrite(&height, sizeof(int), 1, f) != 1)
    {
        fprintf(stderr, "Error escribiendo height\n");
        fclose(f);
        return -1;
    }

    size_t n = (size_t)width * height;
    if (fwrite(data, 1, n, f) != n)
    {
        fprintf(stderr, "Error escribiendo pixeles\n");
        fclose(f);
        return -1;
    }

    fclose(f);
    return 0;
}

// JPEG
uint8_t *load_jpeg_as_gray(const char *path, int *width, int *height)
{
    FILE *infile = fopen(path, "rb");
    if (!infile)
    {
        perror("fopen");
        return NULL;
    }

    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);
    jpeg_stdio_src(&cinfo, infile);

    if (jpeg_read_header(&cinfo, TRUE) != JPEG_HEADER_OK)
    {
        fprintf(stderr, "Error leyendo header JPEG\n");
        jpeg_destroy_decompress(&cinfo);
        fclose(infile);
        return NULL;
    }

    jpeg_start_decompress(&cinfo);

    *width = cinfo.output_width;
    *height = cinfo.output_height;
    int channels = cinfo.output_components;

    size_t row_stride = (size_t)(*width) * channels;
    JSAMPARRAY buffer = (*cinfo.mem->alloc_sarray)((j_common_ptr)&cinfo, JPOOL_IMAGE, row_stride, 1);

    size_t num_pixels = (size_t)(*width) * (*height);
    uint8_t *gray = (uint8_t *)malloc(num_pixels);
    if (!gray)
    {
        fprintf(stderr, "Sin memoria para imagen gris\n");
        jpeg_destroy_decompress(&cinfo);
        fclose(infile);
        return NULL;
    }

    size_t y = 0;
    while (cinfo.output_scanline < cinfo.output_height)
    {
        jpeg_read_scanlines(&cinfo, buffer, 1);
        JSAMPLE *row = buffer[0];

        if (channels == 3)
        {
            // RGB -> gris
            for (int x = 0; x < *width; ++x)
            {
                int r = row[3 * x + 0];
                int g = row[3 * x + 1];
                int b = row[3 * x + 2];

                int gval = (int)(0.299 * r + 0.587 * g + 0.114 * b + 0.5);
                if (gval > 255)
                    gval = 255;
                if (gval < 0)
                    gval = 0;
                gray[y * (*width) + x] = (uint8_t)gval;
            }
        }
        else if (channels == 1)
        {
            memcpy(&gray[y * (*width)], row, (size_t)(*width));
        }
        else
        {
            for (int x = 0; x < *width; ++x)
            {
                gray[y * (*width) + x] = row[channels * x];
            }
        }

        y++;
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    fclose(infile);

    return gray;
}

int save_gray_as_jpeg(const char *path, int width, int height,
                      const uint8_t *data, int quality)
{
    FILE *outfile = fopen(path, "wb");
    if (!outfile)
    {
        perror("fopen");
        return -1;
    }

    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    jpeg_stdio_dest(&cinfo, outfile);

    cinfo.image_width = width;
    cinfo.image_height = height;
    cinfo.input_components = 1;
    cinfo.in_color_space = JCS_GRAYSCALE;

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, quality, TRUE);

    jpeg_start_compress(&cinfo, TRUE);

    JSAMPROW row_pointer[1];
    while (cinfo.next_scanline < cinfo.image_height)
    {
        row_pointer[0] = (JSAMPROW)&data[cinfo.next_scanline * width];
        jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);
    fclose(outfile);

    return 0;
}
