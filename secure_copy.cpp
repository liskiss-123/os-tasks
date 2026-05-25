#include <iostream>
#include <pthread.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <fstream>
#include <string>
#include <time.h>

extern "C" {
    void set_key(char key);
    void caesar(void* src, void* dst, int len);
}

volatile sig_atomic_t keep_running = 1;

void handle_sigint(int) {
    keep_running = 0;
}

pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

void log_event(const std::string& msg) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += 1;

    if (pthread_mutex_timedlock(&log_mutex, &ts) == 0) {
        std::ofstream log_file("copy_log.txt", std::ios::app);
        if (log_file.is_open()) {
            log_file << msg << "\n";
        }
        pthread_mutex_unlock(&log_mutex);
    } else {
        std::cerr << "[Таймаут] Предотвращена взаимоблокировка при записи лога.\n";
    }
}

struct SharedData {
    char buffer[8192];
    ssize_t bytes_ready;
    bool eof;
    pthread_mutex_t mutex;
    pthread_cond_t can_produce;
    pthread_cond_t can_consume;

    const char* in_file;
    const char* out_file;
};

void* producer_func(void* arg) {
    SharedData* data = (SharedData*)arg;
    int fd = open(data->in_file, O_RDONLY);
    
    if (fd < 0) {
        log_event("Ошибка открытия: " + std::string(data->in_file));
        keep_running = 0; 
        pthread_cond_signal(&data->can_consume);
        return nullptr;
    }

    log_event("Начато чтение: " + std::string(data->in_file));

    char local_buf[8192];
    while (keep_running) {
        ssize_t bytes_read = read(fd, local_buf, sizeof(local_buf));
        if (bytes_read < 0) break;

        if (bytes_read > 0) caesar(local_buf, local_buf, bytes_read);

        pthread_mutex_lock(&data->mutex);
        while (data->bytes_ready > 0 && keep_running) {
            pthread_cond_wait(&data->can_produce, &data->mutex);
        }

        if (!keep_running) {
            pthread_mutex_unlock(&data->mutex);
            break;
        }

        memcpy(data->buffer, local_buf, bytes_read);
        data->bytes_ready = bytes_read;
        if (bytes_read == 0) data->eof = true;

        pthread_cond_signal(&data->can_consume);
        pthread_mutex_unlock(&data->mutex);

        if (bytes_read == 0) break; 
    }

    close(fd);
    log_event("Завершено чтение: " + std::string(data->in_file));
    return nullptr;
}

void* consumer_func(void* arg) {
    SharedData* data = (SharedData*)arg;
    int fd = open(data->out_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    
    if (fd < 0) {
        keep_running = 0;
        pthread_cond_signal(&data->can_produce);
        return nullptr;
    }

    while (keep_running) {
        pthread_mutex_lock(&data->mutex);
        while (data->bytes_ready == 0 && !data->eof && keep_running) {
            pthread_cond_wait(&data->can_consume, &data->mutex);
        }

        if (!keep_running && data->bytes_ready == 0) {
            pthread_mutex_unlock(&data->mutex);
            break;
        }

        ssize_t bytes_to_write = data->bytes_ready;
        bool eof = data->eof;

        if (bytes_to_write > 0) {
            ssize_t w = write(fd, data->buffer, bytes_to_write);
            (void)w; 
        }

        data->bytes_ready = 0; 
        pthread_cond_signal(&data->can_produce); 
        pthread_mutex_unlock(&data->mutex);

        if (eof || !keep_running) break;
    }

    close(fd);
    log_event("Завершена запись: " + std::string(data->out_file));
    return nullptr;
}

int main(int argc, char* argv[]) {
    if (argc < 4 || argc % 2 != 0) {
        std::cerr << "Использование: ./secure_copy <ключ> <вход1> <выход1> [<вход2> <выход2> ...]\n";
        return 1;
    }

    struct sigaction sa;
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, nullptr);

    set_key(argv[1][0]); 

    int num_files = (argc - 2) / 2;
    
    SharedData* shared = new SharedData[num_files];
    pthread_t* producers = new pthread_t[num_files];
    pthread_t* consumers = new pthread_t[num_files];

    log_event("=== Запуск. Обработка файлов: " + std::to_string(num_files) + " ===");

    for (int i = 0; i < num_files; i++) {
        shared[i].bytes_ready = 0;
        shared[i].eof = false;
        shared[i].in_file = argv[2 + i * 2];
        shared[i].out_file = argv[3 + i * 2];
        
        pthread_mutex_init(&shared[i].mutex, nullptr);
        pthread_cond_init(&shared[i].can_produce, nullptr);
        pthread_cond_init(&shared[i].can_consume, nullptr);

        pthread_create(&producers[i], nullptr, producer_func, &shared[i]);
        pthread_create(&consumers[i], nullptr, consumer_func, &shared[i]);
    }

    for (int i = 0; i < num_files; i++) {
        pthread_join(producers[i], nullptr);
        pthread_join(consumers[i], nullptr);
        
        pthread_mutex_destroy(&shared[i].mutex);
        pthread_cond_destroy(&shared[i].can_produce);
        pthread_cond_destroy(&shared[i].can_consume);
    }

    delete[] shared;
    delete[] producers;
    delete[] consumers;

    if (!keep_running) {
        std::cout << "\nОперация прервана.\n";
        log_event("=== Прерывание (Ctrl+C) ===");
        return 130;
    }

    std::cout << "Успешно! Лог работы сохранен в copy_log.txt\n";
    log_event("=== Завершено успешно ===");
    return 0;
}