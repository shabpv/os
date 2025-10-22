#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <iomanip>
#include <clocale>
#include <algorithm>
#include <pthread.h>

class Matrix {
private:
    size_t n_;
    std::vector<double> data_;
    mutable pthread_mutex_t mutex;

    void check_bounds(size_t i, size_t j) const {
        if (i >= n_ || j >= n_) {
            throw std::out_of_range("Matrix index out of range");
        }
    }

public:
    explicit Matrix(size_t n) : n_(n), data_(n* n, 0.0) {
        pthread_mutex_init(&mutex, NULL);
    }

    ~Matrix() {
        pthread_mutex_destroy(&mutex);
    }

    Matrix(const Matrix& other) : n_(other.n_), data_(other.data_) {
        pthread_mutex_init(&mutex, NULL);
    }

    Matrix& operator=(const Matrix& other) {
        if (this != &other) {
            pthread_mutex_lock(&mutex);
            n_ = other.n_;
            data_ = other.data_;
            pthread_mutex_unlock(&mutex);
        }
        return *this;
    }

    size_t size() const { return n_; }

    double get(size_t i, size_t j) const {
        return data_[i * n_ + j];
    }

    void set(size_t i, size_t j, double val) {
        data_[i * n_ + j] = val;
    }

    void add_safe(size_t i, size_t j, double val) {
        pthread_mutex_lock(&mutex);
        data_[i * n_ + j] += val;
        pthread_mutex_unlock(&mutex);
    }

    void fill_random() {
        std::mt19937 gen(42);
        std::uniform_real_distribution<double> dis(0.0, 10.0);
        for (size_t i = 0; i < n_; ++i) {
            for (size_t j = 0; j < n_; ++j) {
                set(i, j, dis(gen));
            }
        }
    }

    static Matrix multiply_sequential(const Matrix& a, const Matrix& b) {
        size_t n = a.size();
        Matrix c(n);
        for (size_t i = 0; i < n; ++i) {
            for (size_t j = 0; j < n; ++j) {
                double sum = 0.0;
                for (size_t p = 0; p < n; ++p) {
                    sum += a.get(i, p) * b.get(p, j);
                }
                c.set(i, j, sum);
            }
        }
        return c;
    }
};

struct ThreadData {
    const Matrix* a;
    const Matrix* b;
    Matrix* c;
    size_t rowA, colA, colB, blockSize, n;
};

static void* multiply_block_pthread(void* arg) {
    ThreadData* data = static_cast<ThreadData*>(arg);

    size_t i_end = (std::min)(data->rowA + data->blockSize, data->n);
    size_t j_end = (std::min)(data->colB + data->blockSize, data->n);
    size_t k_end = (std::min)(data->colA + data->blockSize, data->n);

    for (size_t i = data->rowA; i < i_end; ++i) {
        for (size_t j = data->colB; j < j_end; ++j) {
            double sum = 0.0;
            for (size_t k = data->colA; k < k_end; ++k) {
                sum += data->a->get(i, k) * data->b->get(k, j);
            }
            data->c->add_safe(i, j, sum);
        }
    }
    delete data;
    return NULL;
}

static long long threaded_block_multiply_pthread(const Matrix& a, const Matrix& b, Matrix& result, size_t block_size) {
    size_t n = a.size();
    std::vector<pthread_t> threads;
    auto start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < n; i += block_size) {
        for (size_t j = 0; j < n; j += block_size) {
            for (size_t k = 0; k < n; k += block_size) {
                ThreadData* data = new ThreadData{ &a, &b, &result, i, k, j, block_size, n };
                pthread_t thread;
                if (pthread_create(&thread, NULL, multiply_block_pthread, data) == 0) {
                    threads.push_back(thread);
                }
            }
        }
    }

    for (pthread_t thread : threads) {
        pthread_join(thread, NULL);
    }

    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
}

int main() {
    setlocale(LC_ALL, "Russian");

    size_t n;
    std::cout << "Введите размер матрицы N (>=5): ";
    std::cin >> n;
    if (n < 5) n = 5;

    Matrix a(n), b(n);
    a.fill_random();
    b.fill_random();

    auto start_seq = std::chrono::high_resolution_clock::now();
    Matrix c_seq = Matrix::multiply_sequential(a, b);
    auto time_seq = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::high_resolution_clock::now() - start_seq).count();

    std::cout << "\nВремя обычного (последовательного) умножения: " << time_seq << " мкс\n\n";
    std::cout << "Размер блока\tБлоков на сторону\tПотоков\t\tВремя (мкс)\n";
    std::cout << std::string(75, '-') << "\n";

    for (size_t block = 1; block <= n; ++block) {
        Matrix c_res(n);
        long long time = threaded_block_multiply_pthread(a, b, c_res, block);

        size_t blocks_per_side = (n + block - 1) / block;
        size_t num_threads = blocks_per_side * blocks_per_side * blocks_per_side;

        std::cout << block << "\t\t" << blocks_per_side << "\t\t\t" << num_threads << "\t\t" << time << "\n";
    }

    return 0;
}