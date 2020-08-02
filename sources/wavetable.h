#pragma once
#include <memory>
class QFile;
class QString;

struct Wavetable {
    unsigned count = 0; // number of subtables
    unsigned frames = 0; // number of frames per subtable
    std::unique_ptr<float[]> data; // wave data [count * frames]
};

typedef std::shared_ptr<Wavetable> Wavetable_s;
typedef std::unique_ptr<Wavetable> Wavetable_u;

namespace Wavetables {
    void saveToWAVFile(QFile &stream, const Wavetable &wt, const QString &code);
}
