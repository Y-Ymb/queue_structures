#include <iostream>
#include <atomic>

#define N 10000

namespace queue_structures {



typedef struct cell_t {
  int rank;
  int gap;
  uint64_t data;

  cell_t(): rank(-1), gap(-1) {}
  ~cell_t() {}
} cell_t;

typedef struct rank_gap_pair {
 public:
    std::atomic<int> rank;
    std::atomic<int> gap;

    rank_gap_pair(): rank(-1), gap(-1) {}
    ~rank_gap_pair() {}

    bool double_compare_and_swap(int expected_rank, int new_rank,
                                 int expected_gap, int new_gap) {
      bool success_rank = rank.compare_exchange_strong(expected_rank, new_rank);
      bool success_gap = gap.compare_exchange_strong(expected_gap, new_gap);
      return (success_rank & success_gap);
    }
} rank_gap_pair;

typedef struct cell_mpmc {
  rank_gap_pair pair;
  uint64_t data;
} cell_mpmc;

class raw_ffq {
 public:
    std::atomic<uint32_t> head;
    uint32_t tail;
    cell_t cell[N];

    raw_ffq():head(0), tail(0) {}
    ~raw_ffq() {}

    void enqueue(uint64_t data) {
      bool success = false;
      while (!success) {
        cell_t* cell_tmp = &cell[tail % N];
        if (cell_tmp->rank >= 0) {
          cell_tmp->gap = tail;
        } else {
          cell_tmp->data = data;
          cell_tmp->rank = tail;
          success = true;
        }
        tail += 1;
      }
    }

    uint64_t dequeue()  {
      uint32_t rank_tmp = head.fetch_add(1);
      cell_t* cell_tmp = &cell[rank_tmp % N];
      uint64_t result_data;
      bool success = false;
      while (!success) {
        if (cell_tmp->rank == rank_tmp) {
          result_data = cell_tmp->data;
          cell_tmp->rank = -1;
          success = true;
        } else if (cell_tmp->gap >= rank_tmp) {
          rank_tmp = head.fetch_add(1);
          cell_tmp = &cell[rank_tmp % N];
        }
      }
      return result_data;
    }
};


class mpmc_ffq {
 public:
    std::atomic<uint32_t> head;
    std::atomic<uint32_t> tail;
    cell_mpmc cell[N];

    mpmc_ffq(): head(0), tail(0) {}
    ~mpmc_ffq() {}

    void enqueue(uint64_t data) {
      bool success = false;
      while (!success) {
        auto rank_tmp = tail.fetch_add(1);
        cell_mpmc* cell_tmp = &cell[rank_tmp % N];
        while (cell_tmp->pair.gap < rank_tmp) {
          if (cell_tmp->pair.rank >= 0) {
            cell_tmp->pair.double_compare_and_swap(cell_tmp->pair.rank,
                     cell_tmp->pair.rank, cell_tmp->pair.gap, rank_tmp);
          } else if (cell_tmp->pair.double_compare_and_swap(-1, -2,
                     cell_tmp->pair.gap, cell_tmp->pair.gap)) {
            cell_tmp->data = data;
            cell_tmp->pair.rank = rank_tmp;
            success = true;
          }
        }
      }
    }

    uint64_t dequeue() {
      uint32_t rank_tmp = head.fetch_add(1);
      cell_mpmc* cell_tmp = &cell[rank_tmp % N];
      uint64_t result_data;
      bool success = false;
      while (!success) {
        if (cell_tmp->pair.rank == rank_tmp) {
          result_data = cell_tmp->data;
          cell_tmp->pair.rank = -1;
          success = true;
        } else if (cell_tmp->pair.gap >= rank_tmp) {
          rank_tmp = head.fetch_add(1);
          cell_tmp = &cell[rank_tmp % N];
        }
      }
      return result_data;
    }
};









}  //  namespace queue_structures

int main() {
  std::cout << "sample program" << std::endl;
  return 1;
}
