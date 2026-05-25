#include <iostream>
#include <pthread.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>

extern "C" {
    void set_key(char key);
    void caesar(void* src, void* dst, int len);
}

volatile sig_atomic_t keep_running = 1;

void handle_sigint(int) {
    keep_running = 0;
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
        perror("Ошибка открытия входного файла");
        keep_running = 0; 
        pthread_cond_signal(&data->can_consume);
        return nullptr;
    }

    char local_buf[8192];
    while (keep_running) {
        ssize_t bytes_read = read(fd, local_buf, sizeof(local_buf));
        if (bytes_read < 0) {
            perror("Ошибка чтения");
            break;
        }

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
    return nullptr;
}

void* consumer_func(void* arg) {
    SharedData* data = (SharedData*)arg;
    int fd = open(data->out_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    
    if (fd < 0) {
        perror("Ошибка открытия выходного файла");
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
            ssize_t written = write(fd, data->buffer, bytes_to_write);
            if (written < 0) {
                perror("Ошибка записи");
                keep_running = 0;
            }
        }

        data->bytes_ready = 0;
        pthread_cond_signal(&data->can_produce);
        pthread_mutex_unlock(&data->mutex);

        if (eof || !keep_running) break;
    }

    close(fd);
    return nullptr;
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Использование: ./secure_copy <вход> <выход> <ключ>\n";
        return 1;
    }

    struct sigaction sa;
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, nullptr);

    set_key(argv[3][0]);

    SharedData shared = {};
    shared.bytes_ready = 0;
    shared.eof = false;
    shared.in_file = argv[1];
    shared.out_file = argv[2];
    pthread_mutex_init(&shared.mutex, nullptr);
    pthread_cond_init(&shared.can_produce, nullptr);
    pthread_cond_init(&shared.can_consume, nullptr);

    pthread_t producer_tid, consumer_tid;
    pthread_create(&producer_tid, nullptr, producer_func, &shared);
    pthread_create(&consumer_tid, nullptr, consumer_func, &shared);

    pthread_join(producer_tid, nullptr);
    pthread_join(consumer_tid, nullptr);

    pthread_mutex_destroy(&shared.mutex);
    pthread_cond_destroy(&shared.can_produce);
    pthread_cond_destroy(&shared.can_consume);

    if (!keep_running) {
        std::cout << "\nОперация прервана пользователем" << std::endl;
        return 130;
    }

    std::cout << "Файл успешно скопирован и зашифрован.\n";
    return 0;
}