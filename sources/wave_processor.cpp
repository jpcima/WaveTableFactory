#include "wave_processor.h"
#include "wavetable.h"
#include <octave/interpreter.h>
#include <octave/error.h>

struct WaveProcessor::Impl {
    octave::interpreter interp_;
    int startup_status_ = 0;
};

WaveProcessor::WaveProcessor()
{
    Impl *impl = new Impl;
    impl_.reset(impl);

    octave::interpreter &interp = impl_->interp_;
    interp.initialize_history(false);
    interp.initialize_load_path(false);
    interp.initialize();
    impl->startup_status_ = interp.execute();
}

WaveProcessor::~WaveProcessor()
{
}

WaveProcessor::operator bool() const noexcept
{
    Impl &impl = *impl_;
    return impl.startup_status_ == 0;
}

Wavetable *WaveProcessor::process(const std::string &wavecode, unsigned count, unsigned frames, std::string *errmsg)
{
    Impl &impl = *impl_;
    Wavetable_u wt;

    if (count < 1 || frames < 1)
        return nullptr;

    try {
        wt.reset(new Wavetable);
        wt->count = count;
        wt->frames = frames;
        wt->data.reset(new float[count * frames]);

        octave::interpreter &interp = impl.interp_;
        octave::symbol_table &symtab = interp.get_symbol_table();

        // create phases 0-1 (the "X" array)
        Array<double> phases(dim_vector(1, frames));
        for (unsigned i = 0; i < frames; ++i)
            phases(i) = double(i) / double(frames);

        for (unsigned nth = 0; nth < count; ++nth) {
            symtab.clear_all();
            symtab.assign("X", phases);
            symtab.assign("Y", double(nth) / double(count - 1));

            bool silent = true;
            int parse_status = 0;
            interp.eval_string(wavecode, silent, parse_status);

            octave_value wave = symtab.varval("wave");
            if (wave.is_undefined()) {
                if (errmsg)
                    *errmsg = "Result variable 'wave' is not defined.";
                wt.reset();
                break;
            }

            Matrix mat;
            bool valid_mat = wave.is_matrix_type();
            if (valid_mat) {
                mat = wave.matrix_value();
                valid_mat = mat.rows() == 1 && mat.cols() == frames;
            }

            if (!valid_mat) {
                if (errmsg)
                    *errmsg = "Result must be a column vector of size " + std::to_string(frames) + ".";
                wt.reset();
                break;
            }

            float *data = &wt->data[nth * frames];
            for (unsigned i = 0; i < frames; ++i)
                data[i] = mat(i);
        }
    }
    catch (octave::execution_exception &ex) {
        if (errmsg)
            *errmsg = last_error_message();
        wt.reset();
    }

    return wt.release();
}
