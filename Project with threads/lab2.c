#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include <getopt.h>

// Определение констант
#define DELTA_T 0.1 // Шаг по времени
#define DELTA_X 0.1 // Шаг по координате
#define FILEPATH "data.txt" // Путь к файлу с данными

// Структура для хранения информации о потоке
typedef struct {
    pthread_t tid;
    int first;
    int last;
} ThreadRecord;

int threads_count = 0; // Количество потоков
double time_interval = 0.0; // Временной интервал
int nodes_count = 0; // Количество узлов

int boundary_condition = 0; // Граничное условие
double amplitude = 0.05; // Амплитуда

double current_time = 0; // Текущее время


double **Z; // Массив для хранения значений функции
int done = 0; // Флаг завершения

ThreadRecord *threads; // Массив потоков
pthread_barrier_t barrier; // Барьер


void *solver (void *); // Функция для решения уравнения
void explicit_scheme(const int, const int); // Функция для явной схемы
double f(const int, const double); // Функция для вычисления значения функции

void write_into_file(FILE*, const int); // Функция для записи данных в файл

int main (int argc, char* argv[]) {
    // Проверка аргументов
    if (argc < 4) {
        fprintf(stderr, "use: %s <threads_count> <time_interval> <nodes_count> -b -a\n", argv[0]);
        return 1;
    }
    // Получение аргументов
    threads_count = atoi(argv[1]);
    time_interval = atof(argv[2]);
    nodes_count = atoi(argv[3]);

    // Проверка количества узлов
    if (nodes_count % 8 != 0 || nodes_count % threads_count != 0) {
        fprintf(stderr, "The nodes count must multiple of 8 and must be divisible by the number of threads\n");
        return 1;
    }

    // Получение аргументов
    int opt;
        while ((opt = getopt(argc, argv, "b:a:")) != -1) {
            switch (opt) {
                // Граничное условие
                case 'b':
                    boundary_condition = atoi(optarg);
                    if (boundary_condition > 100 || boundary_condition < -100) {
                        perror("Insert boundary in [-100, 100].");
                        return 1;
                    }
                    break;
                // Амплитуда
		        case 'a':
                    amplitude = atoi(optarg);
                    if (amplitude > 0 || amplitude < 1) {
                        perror("Insert amplitude in [0, 1].");
                        return 1;
                    }
                    break;
                default:
                    break;
        }
    }

    Z = (double **) malloc(2 * sizeof(double *)); // Выделение памяти для массива
    for (int i = 0; i < 2; i++) {
        Z[i] = (double *)malloc(nodes_count * sizeof(double));
    }

    // Инициализация массива
    for (int i = 0; i < nodes_count; i++) {
        Z[0][i] = boundary_condition;
    }
    
    // Инициализация баррьера
    pthread_barrier_init(&barrier, NULL, threads_count + 1);

    pthread_attr_t pattr;
    pthread_attr_init (&pattr);
    pthread_attr_setscope (&pattr, PTHREAD_SCOPE_SYSTEM);

    threads = (ThreadRecord *) calloc (threads_count, sizeof(ThreadRecord));

    int current_start_node = 1; // Начальный узел
    int step = nodes_count / threads_count; // Шаг

    // Создание потоков 
    for (int i = 0; i < threads_count; i++) {
        threads[i].first = current_start_node; // Первый узел
        threads[i].last = current_start_node + step - 1; // Последний узел
        if (pthread_create(&(threads[i].tid), &pattr, solver, (void *)&(threads[i]))) {
            fprintf(stderr, "pthread_create"); // Ошибка создания потока
            return 1;
        }
        current_start_node += step; // Увеличение начального узла
    }

    FILE* out = fopen(FILEPATH, "w");

    struct timespec start; // Начальное время
    clock_gettime(CLOCK_MONOTONIC, &start); // Получение времени

    for (; current_time < time_interval; current_time += DELTA_T) {
        pthread_barrier_wait(&barrier); // Ожидание завершения потоков
        // Переход к следующему времени
        if (current_time >= time_interval - DELTA_T) {
            done = 1; 
        }
        // Ожидание завершения потоков
        pthread_barrier_wait(&barrier);
        int cur = ((int)(current_time / DELTA_T) + 1) % 2;
        // Запись данных в файл
#ifdef PLOT
        write_into_file(out, cur);
#endif
    }

    struct timespec finish;
    clock_gettime(CLOCK_MONOTONIC, &finish); // Получение времени
    // Вычисление времени
    double elapsed = finish.tv_sec - start.tv_sec + (double)(finish.tv_nsec - start.tv_nsec) / 10e9;
    printf("Calculation time: %f seconds.\n", elapsed);


    fclose(out);
    printf("Data saved in '%s'\n", FILEPATH);
    
    pthread_barrier_destroy(&barrier); // Уничтожение барьера

    for (int i = 0; i < 2; i++) {
        free(Z[i]);
    }
    free(Z); // Освобождение памяти

    free(threads); // Освобождение памяти

    FILE* fp = fopen("config.dat", "w");
    // Запись данных в файл для визуализации в gnuplot
    fprintf(fp, "set xrange[0:%f]\n", (nodes_count - 1) * DELTA_X);
    fprintf(fp, "set yrange[-5:5]\n");
    fprintf(fp, "do for [i=0:%d] {\n", (int)(time_interval / DELTA_T) - 1);
    fprintf(fp, "   plot '%s' index i smooth bezier\n", FILEPATH);
    fprintf(fp, "   pause 0.05\n");
    fprintf(fp, "}\n");
    fprintf(fp, "pause -1\n");
    fclose(fp);
    exit(0);
}

void *solver(void *arg_p) {
    ThreadRecord *thr = (ThreadRecord *)arg_p;
    // Бесконечный цикл
    while (!done) {
        pthread_barrier_wait(&barrier); // Ожидание барьера
        explicit_scheme(thr->first, thr->last); // Явная схема
        pthread_barrier_wait(&barrier); // Ожидание барьера
    }
}

void explicit_scheme(const int first, const int last) {
    int cur = ((int)(current_time / DELTA_T) + 1) % 2; // Текущее время
    int prev = 1 - cur; // Предыдущее время

    // Цикл по узлам
    for (int x = first; x <= last; x++) {
        int prev_x = x;
        int next_x = x;
        if (x == 0 || x == nodes_count - 1) { // Граничные условия
            continue;
        }
        next_x = x + 1; // Следующий узел
        prev_x = x - 1; // Предыдущий узел

        double d2zdx2 = (Z[prev][next_x] - 2 * Z[prev][x] + Z[prev][prev_x]) / (DELTA_X * DELTA_X); //  Вторая производная
        Z[cur][x] = DELTA_T * DELTA_T * (amplitude * amplitude * d2zdx2 + f(x, current_time)) + 2 * Z[prev][x] - Z[cur][x]; // Вычисление значения функции
    }
}

double f(const int x, double t) { // Функция для вычисления значения функции
    if (current_time < 5 * DELTA_T && fabs(x - nodes_count / 2) < 15) { 
        return 0.5 * sin(2 * M_PI * t);
    }

    return 0;
}

void write_into_file(FILE* out, const int i) { // Функция для записи данных в файл
    for (int j = 0; j < nodes_count; j++){
        fprintf(out, "%f %f\n", j * DELTA_X, Z[i][j]);
    }
    fprintf(out, "\n\n");
}
