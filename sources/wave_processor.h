#pragma once
#include <memory>
struct Wavetable;

class WaveProcessor {
public:
    WaveProcessor();
    ~WaveProcessor();

    explicit operator bool() const noexcept;

    Wavetable *process(const std::string &wavecode, unsigned count, unsigned frames, std::string *errmsg);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
