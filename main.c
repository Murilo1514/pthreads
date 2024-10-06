#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

#define N 30  // Tamanho da floresta
#define LIVRE '-'
#define SENSOR 'T'
#define FOGO '@'
#define QUEIMADO '/'

// Floresta representada por uma matriz
char floresta[N][N];

// Mutexes para garantir exclusão mútua na manipulação da matriz
pthread_mutex_t floresta_mutex[N][N];
pthread_mutex_t central_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_central = PTHREAD_COND_INITIALIZER;

pthread_t thread_central;

// Função para registrar logs
void registrar_log(const char *mensagem) {
    FILE *log_file = fopen("logs_incendio.txt", "a");
    if (log_file == NULL) {
        perror("Erro ao abrir arquivo de log");
        return;
    }
    time_t now;
    time(&now);
    fprintf(log_file, "%s - %s", ctime(&now), mensagem);
    printf("%s - %s", ctime(&now), mensagem);  // Também imprime no terminal
    fclose(log_file);
}

// Função para exibir o estado da floresta
void exibir_forest() {
    pthread_mutex_lock(&central_mutex);
    printf("\n");
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            printf("%c ", floresta[i][j]);
        }
        printf("\n");
    }
    printf("\n");
    pthread_mutex_unlock(&central_mutex);
}

// Função para combater o incêndio
void combater_fogo(int i, int j) {
    pthread_mutex_lock(&floresta_mutex[i][j]);
    if (floresta[i][j] == FOGO) {
        floresta[i][j] = QUEIMADO;
        char msg[100];
        sprintf(msg, "Combate ao fogo na célula (%d, %d)\n", i, j);
        registrar_log(msg);
    }
    pthread_mutex_unlock(&floresta_mutex[i][j]);
}

// Função executada pela thread de cada nó sensor
void* sensor_thread(void* args) {
    int* pos = (int*)args;
    int i = pos[0];
    int j = pos[1];
    free(args);

    while (1) {
        pthread_mutex_lock(&floresta_mutex[i][j]);

        // Verifica se a célula está em chamas
        if (floresta[i][j] == FOGO) {
            // Log de detecção de fogo
            char msg[100];
            sprintf(msg, "Nó sensor em (%d, %d) detectou fogo!\n", i, j);
            registrar_log(msg);

            // Propaga o incêndio para os vizinhos
            if (i > 0 && floresta[i-1][j] == SENSOR) floresta[i-1][j] = FOGO;
            if (i < N-1 && floresta[i+1][j] == SENSOR) floresta[i+1][j] = FOGO;
            if (j > 0 && floresta[i][j-1] == SENSOR) floresta[i][j-1] = FOGO;
            if (j < N-1 && floresta[i][j+1] == SENSOR) floresta[i][j+1] = FOGO;

            // Se for um nó da borda, notifica a central
            if (i == 0 || i == N-1 || j == 0 || j == N-1) {
                pthread_mutex_lock(&central_mutex);
                registrar_log("Nó de borda detectou fogo e notificou a central.\n");
                pthread_cond_signal(&cond_central);
                pthread_mutex_unlock(&central_mutex);
            }
        }

        pthread_mutex_unlock(&floresta_mutex[i][j]);
        sleep(1);  // Intervalo entre verificações
    }
    return NULL;
}

// Função executada pela thread central de controle
void* central_thread(void* args) {
    while (1) {
        pthread_mutex_lock(&central_mutex);
        pthread_cond_wait(&cond_central, &central_mutex);  // Espera notificação dos nós de borda
        registrar_log("Central recebeu notificação de incêndio na borda. Iniciando combate ao fogo...\n");
        exibir_forest();  // Exibe a floresta após notificação
        pthread_mutex_unlock(&central_mutex);
    }
    return NULL;
}

// Função para gerar incêndios aleatórios
void* gerar_incendios(void* args) {
    while (1) {
        sleep(3);  // Gera um incêndio a cada 3 segundos
        int i = rand() % N;
        int j = rand() % N;

        pthread_mutex_lock(&floresta_mutex[i][j]);
        if (floresta[i][j] == SENSOR) {
            floresta[i][j] = FOGO;
            char msg[100];
            sprintf(msg, "Incêndio iniciado aleatoriamente em (%d, %d)\n", i, j);
            registrar_log(msg);
        }
        pthread_mutex_unlock(&floresta_mutex[i][j]);
    }
    return NULL;
}

int main() {
    pthread_t sensores[N][N];
    pthread_t thread_geradora_incendios;

    // Inicializa a floresta com sensores
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            floresta[i][j] = SENSOR;
            pthread_mutex_init(&floresta_mutex[i][j], NULL);
        }
    }

    // Cria a thread central
    pthread_create(&thread_central, NULL, central_thread, NULL);

    // Cria as threads para cada nó sensor
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            int* pos = malloc(2 * sizeof(int));
            pos[0] = i;
            pos[1] = j;
            pthread_create(&sensores[i][j], NULL, sensor_thread, pos);
        }
    }

    // Cria a thread que gera incêndios aleatórios
    pthread_create(&thread_geradora_incendios, NULL, gerar_incendios, NULL);

    // Exibe periodicamente o estado da floresta
    while (1) {
        exibir_forest();
        sleep(5);  // Atualiza a visualização a cada 5 segundos
    }

    return 0;
}
