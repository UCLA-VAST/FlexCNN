#ifndef SACC_UTILS_H_
#define SACC_UTILS_H_
#include <chrono>

class Timer {
  public:
  Timer() : m_beg(clock_::now()) {}
  void reset() { m_beg = clock_::now(); }

  double elapsed() const {
    return std::chrono::duration_cast<std::chrono::milliseconds>(clock_::now() -
                                                                 m_beg)
        .count();
  }

  private:
  typedef std::chrono::high_resolution_clock clock_;
  typedef std::chrono::duration<double, std::ratio<1>> second_;
  std::chrono::time_point<clock_> m_beg;
};
class Uncopyable {
  protected:
  Uncopyable() {}
  ~Uncopyable() {}

  private:
  Uncopyable(const Uncopyable &);
  Uncopyable &operator=(const Uncopyable &);
};

class Singleton {
  private:
  Singleton(){};
  ~Singleton(){};
  Singleton(const Singleton &);
  Singleton &operator=(const Singleton &);

  public:
  static Singleton &getInstance() {
    static Singleton instance;
    return instance;
  }
};
#endif
