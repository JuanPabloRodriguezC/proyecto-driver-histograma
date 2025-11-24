#include <mpi.h>
#include <stdio.h>

#define MASTER 0
#define TAG 1

int main(int argc, char *argv[])
{
    int rank, size;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (size < 2)
    {
        if (rank == MASTER)
        {
            printf("Necesitas al menos 2 procesos (1 master + 1 esclavo)\n");
        }
        MPI_Finalize();
        return 0;
    }

    if (rank == MASTER)
    {
        // MASTER
        printf("[MASTER] Hay %d procesos en total.\n", size);

        for (int dest = 1; dest < size; ++dest)
        {
            int valor = dest;
            printf("[MASTER] Enviando %d al proceso %d\n", valor, dest);
            MPI_Send(&valor, 1, MPI_INT, dest, TAG, MPI_COMM_WORLD);
        }

        for (int src = 1; src < size; ++src)
        {
            int resultado;
            MPI_Recv(&resultado, 1, MPI_INT, src, TAG, MPI_COMM_WORLD,
                     MPI_STATUS_IGNORE);
            printf("[MASTER] Recibi %d desde el proceso %d\n", resultado, src);
        }
    }
    else
    {
        // ESCLAVOS
        int valor_recibido;
        MPI_Recv(&valor_recibido, 1, MPI_INT, MASTER, TAG, MPI_COMM_WORLD,
                 MPI_STATUS_IGNORE);
        printf("[SLAVE %d] Recibi %d del master\n", rank, valor_recibido);

        int resultado = valor_recibido * 10;

        MPI_Send(&resultado, 1, MPI_INT, MASTER, TAG, MPI_COMM_WORLD);
        printf("[SLAVE %d] Envie %d al master\n", rank, resultado);
    }

    MPI_Finalize();
    return 0;
}
