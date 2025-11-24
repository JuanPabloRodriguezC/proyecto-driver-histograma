// cliente.c
// Cliente MPI para el Proyecto: lectura, división, envío, Sobel en esclavos,
// guardado de bloques, reconstrucción, histograma y display en hardware.
//
// Requisitos de build: MPI (mpicc), stb_image, stb_image_write
// Incluir el header de la librería provista: histogram_lib.h

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <mpi.h>

#include "stb_image.h"
#include "stb_image_write.h"

#include "histogram_lib.h" 

#define MASTER 0

static void compute_histogram_uint32(const unsigned char *img, int width, int height, uint32_t hist[256]) {
    memset(hist, 0, sizeof(uint32_t) * 256);
    int sz = width * height;
    for (int i = 0; i < sz; ++i) {
        hist[ img[i] ]++;
    }
}

// Aplica filtro Sobel en un bloque grayscale.
// in: ancho x alto (bytes), out: mismo tamaño.
// width: ancho, height: alto
static void apply_sobel_block(const unsigned char *in, unsigned char *out, int width, int height) {
    // kernels
    // Gx = [ [-1 0 1], [-2 0 2], [-1 0 1] ]
    // Gy = [ [ 1 2 1], [ 0 0 0], [-1 -2 -1] ]
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int gx = 0, gy = 0;
            for (int ky = -1; ky <= 1; ++ky) {
                int yy = y + ky;
                if (yy < 0) yy = 0;
                if (yy >= height) yy = height - 1;
                for (int kx = -1; kx <= 1; ++kx) {
                    int xx = x + kx;
                    if (xx < 0) xx = 0;
                    if (xx >= width) xx = width - 1;
                    int pixel = in[yy * width + xx];
                    int wx = 0, wy = 0;
                    // weight for Gx
                    if (ky == -1) {
                        if (kx == -1) wx = -1;
                        else if (kx == 0) wx = 0;
                        else wx = 1;
                        if (kx == -1) wy = 1;
                        else if (kx == 0) wy = 2;
                        else wy = 1;
                    } else if (ky == 0) {
                        if (kx == -1) wx = -2;
                        else if (kx == 0) wx = 0;
                        else wx = 2;
                        wy = 0;
                    } else { // ky == 1
                        if (kx == -1) wx = -1;
                        else if (kx == 0) wx = 0;
                        else wx = 1;
                        if (kx == -1) wy = -1;
                        else if (kx == 0) wy = -2;
                        else wy = -1;
                    }
                    gx += wx * pixel;
                    gy += wy * pixel;
                }
            }
            int mag = abs(gx) + abs(gy); // aproximación 
            if (mag > 255) mag = 255;
            if (mag < 0) mag = 0;
            out[y * width + x] = (unsigned char)mag;
        }
    }
}

// Guarda una porción procesada por un slave en archivo PNG
static void save_slave_block(int slave_rank, const unsigned char *data, int width, int height) {
    char filename[128];
    snprintf(filename, sizeof(filename), "slave_%02d_block.png", slave_rank);
    // stbi_write_png requiere stride en bytes (width * channels). Aquí channels=1.
    if (!stbi_write_png(filename, width, height, 1, data, width)) {
        fprintf(stderr, "Slave %d: error guardando %s\n", slave_rank, filename);
    } else {
        printf("Slave %d: guardado %s (%dx%d)\n", slave_rank, filename, width, height);
    }
}

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);

    int rank, nprocs;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);

    if (nprocs < 2) {
        if (rank == MASTER) fprintf(stderr, "Se requiere al menos 2 procesos (1 master + 1 slave)\n");
        MPI_Finalize();
        return EXIT_FAILURE;
    }

    if (rank == MASTER) {
        // -------- Master --------
        char imagen_path[512];
        if (argc >= 2) {
            strncpy(imagen_path, argv[1], sizeof(imagen_path)-1);
            imagen_path[sizeof(imagen_path)-1] = '\0';
        } else {
            printf("Ingrese nombre de la imagen: ");
            if (scanf("%511s", imagen_path) != 1) {
                fprintf(stderr, "Entrada inválida\n");
                MPI_Abort(MPI_COMM_WORLD, 1);
            }
        }

        int w, h, channels;
        unsigned char *rgb = stbi_load(imagen_path, &w, &h, &channels, 3);
        if (!rgb) {
            fprintf(stderr, "Error al leer imagen %s\n", imagen_path);
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        printf("Master: imagen cargada %s (%dx%d) canales=%d\n", imagen_path, w, h, 3);

        // Convertir a grayscale
        unsigned char *gray = malloc(w * h);
        if (!gray) { perror("malloc"); MPI_Abort(MPI_COMM_WORLD, 1); }
        for (int i = 0; i < w*h; ++i) {
            int r = rgb[3*i+0];
            int g_ = rgb[3*i+1];
            int b = rgb[3*i+2];
            gray[i] = (unsigned char)(0.299*r + 0.587*g_ + 0.114*b);
        }
        free(rgb);

        // División entre slaves (por filas)
        int slaves = nprocs - 1;
        int base_h = h / slaves;
        int rem = h % slaves;

        // Para cada slave calculamos start_row y rows (sin contar solape).
        int cur_row = 0;
        for (int wkr = 1; wkr <= slaves; ++wkr) {
            int rows = base_h + (wkr <= rem ? 1 : 0);
            int start_row = cur_row;
            int end_row = start_row + rows - 1;

            // Calcular solape: enviamos 1 fila extra arriba si start_row>0,
            // y 1 fila extra abajo si end_row < h-1
            int send_start = (start_row > 0) ? (start_row - 1) : start_row;
            int send_end = (end_row < (h-1)) ? (end_row + 1) : end_row;
            int send_rows = send_end - send_start + 1;

            // Encabezado: width, orig_rows (rows), orig_start_row, send_rows, send_start (opcional)
            int header[5];
            header[0] = w;
            header[1] = rows;         // filas que slave debe procesar (sin solape) — para guardar y para master al recibir
            header[2] = start_row;    // fila original donde inicia este bloque (sin solape)
            header[3] = send_rows;    // filas realmente enviadas (con solape)
            header[4] = send_start;   // fila de donde parte en la imagen global

            // Enviar header
            MPI_Send(header, 5, MPI_INT, wkr, 0, MPI_COMM_WORLD);

            // Enviar datos (send_rows * w bytes), empezando en gray + send_start * w
            MPI_Send(gray + (send_start * w), send_rows * w, MPI_UNSIGNED_CHAR, wkr, 0, MPI_COMM_WORLD);

            cur_row += rows;
        }

        // Recibir resultados de cada slave (solo las filas originales, sin solape)
        unsigned char *final_img = malloc(w * h);
        if (!final_img) { perror("malloc"); MPI_Abort(MPI_COMM_WORLD, 1); }

        for (int wkr = 1; wkr <= slaves; ++wkr) {
            // Primero recibimos header de confirmación
            int recv_header[3];
            MPI_Recv(recv_header, 3, MPI_INT, wkr, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            int recv_w = recv_header[0];
            int recv_rows = recv_header[1];
            int recv_start_row = recv_header[2];
            if (recv_w != w) {
                fprintf(stderr, "Master: ancho recibido diferente del esperado por slave %d\n", wkr);
                MPI_Abort(MPI_COMM_WORLD, 1);
            }
            int count = recv_rows * w;
            MPI_Recv(final_img + (recv_start_row * w), count, MPI_UNSIGNED_CHAR, wkr, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            printf("Master: recibido bloque de slave %d -> filas %d..%d\n", wkr, recv_start_row, recv_start_row + recv_rows - 1);
        }

        free(gray);

        // Calcular histograma
        uint32_t hist[256];
        compute_histogram_uint32(final_img, w, h, hist);

        // Inicializar librería / driver
        if (histogram_init() != 0) {
            fprintf(stderr, "Master: histogram_init() falló. Verificar driver y permisos.\n");
        } else {
            if (histogram_display_auto(hist) != 0) {
                fprintf(stderr, "Master: histogram_display_auto falló.\n");
            } else {
                printf("Master: histograma enviado al hardware.\n");
            }
            histogram_cleanup();
        }

        // Guardar imagen final (gris) como PNG
        if (!stbi_write_png("resultado.png", w, h, 1, final_img, w)) {
            fprintf(stderr, "Master: error guardando resultado.png\n");
        } else {
            printf("Master: resultado guardado en resultado.png\n");
        }

        free(final_img);
    }
    else {
        // -------- Slave --------
        // Recibir header
        int header[5];
        MPI_Recv(header, 5, MPI_INT, MASTER, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        int width = header[0];
        int orig_rows = header[1];
        int orig_start_row = header[2];
        int send_rows = header[3];
        int send_start = header[4];

        if (width <= 0 || send_rows <= 0) {
            fprintf(stderr, "Slave %d: header inválido\n", rank);
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        unsigned char *recv_buf = malloc(width * send_rows);
        unsigned char *out_buf = malloc(width * send_rows);
        if (!recv_buf || !out_buf) { perror("malloc"); MPI_Abort(MPI_COMM_WORLD, 1); }

        MPI_Recv(recv_buf, width * send_rows, MPI_UNSIGNED_CHAR, MASTER, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        // Aplicar Sobel al bloque recibido (incluye bordes solapados)
        apply_sobel_block(recv_buf, out_buf, width, send_rows);

        // Debemos enviar SOLO las orig_rows centrales (sin solape) al master,
        // y además guardar en filesystem esa porción (cumplimiento del PDF).
        int copy_offset_in_recv = 0;
        if (send_start < orig_start_row) {
            // esto significa que el bloque tiene solape arriba -> la fila original empieza en recv fila 1
            copy_offset_in_recv = 1;
        } else {
            copy_offset_in_recv = 0;
        }
        // copy_offset_in_recv es 0 o 1
        unsigned char *to_send = out_buf + (copy_offset_in_recv * width);
        int to_send_rows = orig_rows; // filas exactas que corresponden al slave (sin solape)

        // Guardar el bloque procesado que le corresponde al slave (sin solape)
        save_slave_block(rank, to_send, width, to_send_rows);

        // Enviar header confirmación: width, rows, start_row
        int send_header[3] = { width, to_send_rows, orig_start_row };
        MPI_Send(send_header, 3, MPI_INT, MASTER, 0, MPI_COMM_WORLD);

        // Enviar datos
        MPI_Send(to_send, width * to_send_rows, MPI_UNSIGNED_CHAR, MASTER, 0, MPI_COMM_WORLD);

        free(recv_buf);
        free(out_buf);
    }

    MPI_Finalize();
    return 0;
}
