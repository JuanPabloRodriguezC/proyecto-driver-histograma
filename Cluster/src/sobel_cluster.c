#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#include "image_utils.h"

#define MASTER 0
#define TAG_META 1
#define TAG_DATA 2
#define TAG_RESULT 3
#define TAG_TIME 4

static int load_sobel_kernels_from_file(const char *path,
                                        int kx[3][3],
                                        int ky[3][3])
{
    FILE *f = fopen(path, "r");
    if (!f)
    {
        perror("fopen kernel");
        return -1;
    }

    // Leer 9 ints para KX
    printf("[MASTER] Leyendo kernel X desde %s\n", path);
    for (int j = 0; j < 3; ++j)
    {
        for (int i = 0; i < 3; ++i)
        {
            if (fscanf(f, "%d", &kx[j][i]) != 1)
            {
                fprintf(stderr, "Error leyendo KX[%d][%d]\n", j, i);
                fclose(f);
                return -1;
            }
        }
    }

    // Leer 9 ints para KY
    printf("[MASTER] Leyendo kernel Y desde %s\n", path);
    for (int j = 0; j < 3; ++j)
    {
        for (int i = 0; i < 3; ++i)
        {
            if (fscanf(f, "%d", &ky[j][i]) != 1)
            {
                fprintf(stderr, "Error leyendo KY[%d][%d]\n", j, i);
                fclose(f);
                return -1;
            }
        }
    }

    fclose(f);
    return 0;
}

static void apply_sobel(const uint8_t *in, uint8_t *out,
                        int w, int h,
                        const int kx[3][3],
                        const int ky[3][3])
{
    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            if (x == 0 || y == 0 || x == w - 1 || y == h - 1)
            {
                out[y * w + x] = 0;
                continue;
            }

            int gx = 0, gy = 0;
            for (int j = -1; j <= 1; ++j)
            {
                for (int i = -1; i <= 1; ++i)
                {
                    int px = x + i;
                    int py = y + j;
                    int val = in[py * w + px];
                    gx += val * kx[j + 1][i + 1];
                    gy += val * ky[j + 1][i + 1];
                }
            }

            int mag = (int)round(sqrt((double)(gx * gx + gy * gy)));
            if (mag > 255)
                mag = 255;
            if (mag < 0)
                mag = 0;
            out[y * w + x] = (uint8_t)mag;
        }
    }
}

int main(int argc, char *argv[])
{
    int rank, size;
    int kx[3][3];
    int ky[3][3];
    size_t total_bytes_sent = 0;
    size_t total_bytes_received = 0;

    size_t meta_bytes_per_msg = sizeof(int) * 2;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (size < 2)
    {
        if (rank == MASTER)
        {
            fprintf(stderr, "Necesitas al menos 2 procesos (1 master + esclavo(s))\n");
        }
        MPI_Finalize();
        return 1;
    }

    if (rank == MASTER)
    {
        if (argc < 3)
        {
            fprintf(stderr, "Uso: %s imagen.jpg kernel.cfg\n", argv[0]);
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        const char *kernel_path = argv[2];

        if (load_sobel_kernels_from_file(kernel_path, kx, ky) != 0)
        {
            fprintf(stderr, "Error cargando kernel desde %s\n", kernel_path);
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
    }
    MPI_Bcast(&kx[0][0], 9, MPI_INT, MASTER, MPI_COMM_WORLD);
    MPI_Bcast(&ky[0][0], 9, MPI_INT, MASTER, MPI_COMM_WORLD);

    if (rank == MASTER)
    {
        const char *image_path = argv[1];
        int width, height;
        uint8_t *img = load_jpeg_as_gray(image_path, &width, &height);
        if (!img)
        {
            fprintf(stderr, "No se pudo cargar la imagen JPEG\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        uint8_t *filtered = (uint8_t *)malloc((size_t)width * height);
        if (!filtered)
        {
            fprintf(stderr, "Sin memoria para imagen filtrada\n");
            free(img);
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        int num_slaves = size - 1;
        if (height < num_slaves)
        {
            fprintf(stderr, "Advertencia: hay más esclavos que filas de imagen\n");
        }

        // arrays para recordar cuántas filas y desde dónde empieza cada chunk
        int *rows_per_rank = (int *)calloc(size, sizeof(int));
        int *start_row = (int *)calloc(size, sizeof(int));
        if (!rows_per_rank || !start_row)
        {
            fprintf(stderr, "Sin memoria para metadatos\n");
            free(img);
            free(filtered);
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        int base_rows = height / num_slaves;
        int remainder = height % num_slaves;

        int current_start = 0;
        for (int r = 1; r <= num_slaves; ++r)
        {
            int extra = (r <= remainder) ? 1 : 0;
            rows_per_rank[r] = base_rows + extra;
            start_row[r] = current_start;
            current_start += rows_per_rank[r];
        }

        double t_total_start = MPI_Wtime();

        // Enviar metadatos + datos a cada esclavo
        for (int r = 1; r <= num_slaves; ++r)
        {
            int rows = rows_per_rank[r];
            int meta[2] = {width, rows};
            MPI_Send(meta, 2, MPI_INT, r, TAG_META, MPI_COMM_WORLD);

            size_t n = (size_t)width * rows;
            const uint8_t *chunk_ptr = img + (size_t)start_row[r] * width;

            MPI_Send((void *)chunk_ptr, (int)n, MPI_UINT8_T, r, TAG_DATA, MPI_COMM_WORLD);
            total_bytes_sent += meta_bytes_per_msg + n;
        }

        // Recibir resultados de cada esclavo
        for (int r = 1; r <= num_slaves; ++r)
        {
            int meta[2];
            MPI_Recv(meta, 2, MPI_INT, r, TAG_META, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            int rows = meta[1];
            size_t n = (size_t)width * rows;

            uint8_t *chunk_filtered = (uint8_t *)malloc(n);
            if (!chunk_filtered)
            {
                fprintf(stderr, "Sin memoria para chunk filtrado\n");
                MPI_Abort(MPI_COMM_WORLD, 1);
            }

            MPI_Recv(chunk_filtered, (int)n, MPI_UINT8_T, r, TAG_RESULT,
                     MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            total_bytes_received += meta_bytes_per_msg + n;

            int srow = start_row[r];
            memcpy(filtered + (size_t)srow * width, chunk_filtered, n);
            free(chunk_filtered);
        }

        double t_total_end = MPI_Wtime();
        double t_total = t_total_end - t_total_start;

        printf("[MASTER] Procesamiento distribuido terminado. Tiempo = %f s\n", t_total);

        double *compute_times = (double *)malloc(num_slaves * sizeof(double));
        if (!compute_times)
        {
            fprintf(stderr, "Sin memoria para tiempos\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        for (int r = 1; r <= num_slaves; ++r)
        {
            MPI_Recv(&compute_times[r - 1], 1, MPI_DOUBLE, r, TAG_TIME,
                     MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
        double max_compute = 0.0;
        double sum_compute = 0.0;
        for (int i = 0; i < num_slaves; ++i)
        {
            if (compute_times[i] > max_compute)
                max_compute = compute_times[i];
            sum_compute += compute_times[i];
        }

        double estimated_overhead = t_total - max_compute;
        if (estimated_overhead < 0)
            estimated_overhead = 0;

        // Guardar imagen filtrada
        if (save_gray_as_jpeg("output_sobel.jpg", width, height, filtered, 90) == 0)
        {
            printf("[MASTER] Imagen filtrada guardada en output_sobel.jpg\n");
        }
        else
        {
            fprintf(stderr, "[MASTER] Error guardando output_sobel.jpg\n");
        }
        uint8_t *seq_out = (uint8_t *)malloc((size_t)width * height);
        if (!seq_out)
        {
            fprintf(stderr, "Sin memoria para seq_out\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        double t_seq_start = MPI_Wtime();
        apply_sobel(img, seq_out, width, height, kx, ky);
        double t_seq_end = MPI_Wtime();
        double t_seq = t_seq_end - t_seq_start;
        double speedup = t_seq / t_total;
        double efficiency = speedup / num_slaves;

        printf("\n===== Métricas =====\n");
        printf("Tiempo total distribuido (T_paralelo): %f s\n", t_total);
        printf("Tiempos de cómputo por nodo esclavo:\n");
        for (int r = 1; r <= num_slaves; ++r)
        {
            printf("  Nodo %d: %f s\n", r, compute_times[r - 1]);
        }
        printf("Tiempo máximo de cómputo (max_compute): %f s\n", max_compute);
        printf("Overhead aproximado (comunicación + sincronización): %f s\n", estimated_overhead);
        printf("Bytes enviados por el master:    %zu bytes\n", total_bytes_sent);
        printf("Bytes recibidos por el master:   %zu bytes\n", total_bytes_received);
        printf("Tiempo secuencial (T_secuencial): %f s\n", t_seq);
        printf("Speedup = T_secuencial / T_paralelo = %f\n", speedup);
        printf("Eficiencia (speedup / #esclavos) = %f\n", efficiency);
        printf("=================================\n");

        free(seq_out);
        free(compute_times);
        free(rows_per_rank);
        free(start_row);
        free(img);
        free(filtered);
    }
    else
    {
        // ESCLAVOS
        int meta[2];
        MPI_Recv(meta, 2, MPI_INT, MASTER, TAG_META, MPI_COMM_WORLD,
                 MPI_STATUS_IGNORE);
        int width = meta[0];
        int rows = meta[1];

        size_t n = (size_t)width * rows;
        uint8_t *chunk_in = NULL;
        uint8_t *chunk_out = NULL;

        if (rows > 0)
        {
            chunk_in = (uint8_t *)malloc(n);
            chunk_out = (uint8_t *)malloc(n);
            if (!chunk_in || !chunk_out)
            {
                fprintf(stderr, "[RANK %d] Sin memoria para chunks\n", rank);
                MPI_Abort(MPI_COMM_WORLD, 1);
            }

            MPI_Recv(chunk_in, (int)n, MPI_UINT8_T, MASTER, TAG_DATA, MPI_COMM_WORLD,
                     MPI_STATUS_IGNORE);

            double t_compute_start = MPI_Wtime();
            apply_sobel(chunk_in, chunk_out, width, rows, kx, ky);
            double t_compute_end = MPI_Wtime();
            double t_compute = t_compute_end - t_compute_start;

            // Guardar chunk procesado
            char fname[64];
            snprintf(fname, sizeof(fname), "chunk_rank%d.jpg", rank);

            if (save_gray_as_jpeg(fname, width, rows, chunk_out, 90) == 0)
            {
                printf("[RANK %d] Chunk procesado guardado en %s\n", rank, fname);
            }
            else
            {
                fprintf(stderr, "[RANK %d] Error guardando %s\n", rank, fname);
            }

            // Enviar de regreso
            int meta_res[2] = {width, rows};
            MPI_Send(meta_res, 2, MPI_INT, MASTER, TAG_META, MPI_COMM_WORLD);
            MPI_Send(chunk_out, (int)n, MPI_UINT8_T, MASTER, TAG_RESULT, MPI_COMM_WORLD);
            MPI_Send(&t_compute, 1, MPI_DOUBLE, MASTER, TAG_TIME, MPI_COMM_WORLD);
        }
        else
        {
            int meta_res[2] = {width, rows};
            MPI_Send(meta_res, 2, MPI_INT, MASTER, TAG_META, MPI_COMM_WORLD);
            MPI_Send(NULL, 0, MPI_UINT8_T, MASTER, TAG_RESULT, MPI_COMM_WORLD);

            double t_compute = 0.0;
            MPI_Send(&t_compute, 1, MPI_DOUBLE, MASTER, TAG_TIME, MPI_COMM_WORLD);
        }

        free(chunk_in);
        free(chunk_out);
    }

    MPI_Finalize();
    return 0;
}
