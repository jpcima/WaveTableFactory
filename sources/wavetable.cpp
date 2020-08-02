#include "wavetable.h"
#include <QFile>
#include <array>
#include <type_traits>

template <class T>
static std::array<quint8, sizeof(T)> leImpl(T x)
{
    typename std::make_unsigned<T>::type u = x;
    constexpr size_t n = sizeof(T);
    std::array<quint8, n> b;
    for (size_t i = 0; i < n; ++i)
        b[i] = quint8(u >> (8 * i));
    return b;
}

static std::array<quint8, 2> le16(quint16 x) { return leImpl<quint16>(x); }
static std::array<quint8, 4> le32(quint32 x) { return leImpl<quint32>(x); }

static qint64 writeLE16(QIODevice &stream, quint16 x)
{
    return stream.write(reinterpret_cast<const char *>(le16(x).data()), 2);
}
static qint64 writeLE32(QIODevice &stream, quint32 x)
{
    return stream.write(reinterpret_cast<const char *>(le32(x).data()), 4);
}
static qint64 writeF32(QIODevice &stream, float x)
{
    union { quint32 i; float f; } u;
    u.f = x;
    return writeLE32(stream, u.i);
}

void Wavetables::saveToWAVFile(QFile &stream, const Wavetable &wt, const QString &code)
{
    const unsigned sampleRate = 44100;
    const unsigned channels = 1;

    stream.write("RIFF", 4);
    stream.write("\0\0\0\0", 4);
    qint64 riffStart = stream.pos();
    stream.write("WAVE");

    // fmt chunk
    stream.write("fmt ", 4);
    stream.write("\0\0\0\0", 4);
    qint64 fmtStart = stream.pos();
    writeLE16(stream, 3); // float 32 bits
    writeLE16(stream, channels); // mono
    writeLE32(stream, sampleRate); // sample rate
    writeLE32(stream, sampleRate * channels * sizeof(float)); // bytes per second
    writeLE16(stream, channels * sizeof(float)); // frame alignment
    writeLE16(stream, 8 * sizeof(float)); // bits per sample
    writeLE16(stream, 0); // extension size
    qint64 fmtEnd = stream.pos();

    // fact chunk
    stream.write("fact", 4);
    writeLE32(stream, 4);
    writeLE32(stream, wt.count * wt.frames);

    // data chunk
    stream.write("data", 4);
    writeLE32(stream, channels * wt.count * wt.frames * sizeof(float));
    for (unsigned i = 0, n = channels * wt.count * wt.frames; i < n; ++i) {
        float sample = wt.data[i];
        writeF32(stream, sample);
    }

    // clm chunk
    if (wt.frames <= 8192) {
        stream.write("clm ", 4);
        //
        std::string data;
        data.reserve(256);
        data.append("<!>");
        data.push_back("0123456789"[(wt.frames / 1000) % 10]);
        data.push_back("0123456789"[(wt.frames / 100) % 10]);
        data.push_back("0123456789"[(wt.frames / 10) % 10]);
        data.push_back("0123456789"[wt.frames % 10]);
        data.push_back(' ');
        data.append("10000000");
        data.push_back(' ');
        data.append("wavetable (jpcima.sdf1.org)");
        //
        writeLE32(stream, data.size());
        stream.write(data.data(), data.size());
    }

    // code chunk
    if (!code.isEmpty()) {
        stream.write("WTFs", 4);
        QByteArray data = code.toUtf8();
        if (data.size() & 1)
            data.push_back('\0');
        writeLE32(stream, data.size());
        stream.write(data.data(), data.size());
    }

    qint64 riffEnd = stream.pos();

    stream.flush();
    stream.seek(fmtStart - 4);
    writeLE32(stream, fmtEnd - fmtStart);
    stream.flush();
    stream.seek(riffStart - 4);
    writeLE32(stream, riffEnd - riffStart);
    stream.flush();
}
