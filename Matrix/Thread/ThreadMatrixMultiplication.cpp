#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <random>
#include <iomanip>
#include <clocale>
#include <mutex>
#include <algorithm>

class Matrix {
private:
    size_t n_;
    std::vector<double> data_;
    mutable std::mutex mtx;

    void check_bounds(size_t i, size_t j) const {
        if (i >= n_ || j >= n_) {
            throw std::out_of_range("Matrix index out of range");
        }
    }

public:
    explicit Matrix(size_t n) : n_(n), data_(n* n, 0.0) {}

    Matrix(const Matrix& other) {
        std::lock_guard<std::mutex> lock(other.mtx);
        n_ = other.n_;
        data_ = other.data_;
    }

    Matrix& operator=(const Matrix& other) {
        if (this != &other) {
            std::lock_guard<std::mutex> lock1(mtx);
            std::lock_guard<std::mutex> lock2(other.mtx);
            n_ = other.n_;
            data_ = other.data_;
        }
        return *this;
    }

    size_t size() const { return n_; }

    double get(size_t i, size_t j) const {
        check_bounds(i, j);
        return data_[i * n_ + j];
    }

    void set(size_t i, size_t j, double val) {
        check_bounds(i, j);
        data_[i * n_ + j] = val;
    }

    void add_safe(size_t i, size_t j, double val) {
        std::lock_guard<std::mutex> lock(mtx);
        data_[i * n_ + j] += val;
    }

    double& operator()(size_t i, size_t j) {
        return data_[i * n_ + j];
    }

    const double& operator()(size_t i, size_t j) const {
        return data_[i * n_ + j];
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

static void multiply_block_logic(const Matrix& a, const Matrix& b, Matrix& c,
    size_t rowA, size_t colA, size_t colB, size_t blockSize, size_t n) {

    size_t i_end = std::min(rowA + blockSize, n);
    size_t j_end = std::min(colB + blockSize, n);
    size_t k_end = std::min(colA + blockSize, n);

    for (size_t i = rowA; i < i_end; ++i) {
        for (size_t j = colB; j < j_end; ++j) {
            double sum = 0.0;
            for (size_t k = colA; k < k_end; ++k) {
                sum += a(i, k) * b(k, j);
            }
            c.add_safe(i, j, sum);
        }
    }
}

static long long threaded_block_multiply(const Matrix& a, const Matrix& b, Matrix& result, size_t block_size) {
    size_t n = a.size();
    std::vector<std::thread> threads;
    auto start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < n; i += block_size) {
        for (size_t j = 0; j < n; j += block_size) {
            for (size_t k = 0; k < n; k += block_size) {
                threads.emplace_back(multiply_block_logic,
                    std::cref(a), std::cref(b), std::ref(result),
                    i, k, j, block_size, n);
            }
        }
    }

    for (auto& t : threads) {
        if (t.joinable()) t.join();
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
        long long time = threaded_block_multiply(a, b, c_res, block);

        size_t blocks_per_side = (n + block - 1) / block;
        size_t num_threads = blocks_per_side * blocks_per_side * blocks_per_side;

        std::cout << block << "\t\t" << blocks_per_side << "\t\t\t" << num_threads << "\t\t" << time << "\n";
    }

    return 0;
}