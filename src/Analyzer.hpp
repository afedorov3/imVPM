#pragma once

#define _USE_MATH_DEFINES
#include <cmath>
#include <memory>    // unique_ptr, shared_ptr
#include <algorithm> // min, max
#ifdef ANALYZER_DEBUG
#  include <fstream>
#endif

#include <fft4g.hpp>

#ifndef ANALYZER_SAMPLE_FREQ
#define ANALYZER_SAMPLE_FREQ 44100 // Hz
#endif // ANALYZER_SAMPLE_FREQ

#ifndef ANALYZER_ANALYZE_FREQ
#define ANALYZER_ANALYZE_FREQ 30   // Hz
#endif // ANALYZER_ANALYZE_FREQ

#ifndef ANALYZER_ANALYZE_SPAN
#define ANALYZER_ANALYZE_SPAN 30   // seconds
#endif // ANALYZER_ANALYZE_SPAN

#ifndef ANALYZER_BASE_FREQ
#define ANALYZER_BASE_FREQ FREQ_A3
#endif // ANALYZER_BASE_FREQ

// use parabolic interpolation instead of original averaging code
//#define ANALYZER_INTERPOLATION

class Analyzer {
public:
    typedef          float  sample_t;           // sample type
    static constexpr double sample_fsval = 1.0; // sample full-scale value

    static constexpr double FREQ_A8 = 7040.0;
    static constexpr double FREQ_A7 = 3520.0;
    static constexpr double FREQ_A6 = 1760.0;
    static constexpr double FREQ_A5 = 880.0;
    static constexpr double FREQ_A4 = 440.0;
    static constexpr double FREQ_A3 = 220.0;
    static constexpr double FREQ_A2 = 110.0;
    static constexpr double FREQ_A1 = 55.0;
    static const     double FREQ_C1, FREQ_C2, FREQ_C3, FREQ_C4, FREQ_C5, FREQ_C6, FREQ_C7, FREQ_C8;
    static const     double LOG2_FREQ_C1;

    static const     double SAMPLE_FREQ;
    static const     size_t FFTSIZE;
    static const     size_t ANALYZE_INTERVAL;
    static const     size_t PITCH_BUF_SIZE;

    Analyzer() :
        threshold(2.0),
        analyze_cnt(0),
        total_analyze_cnt(0),
        peak_freq(-1.0),
        han_window(new double[FFTSIZE]),
        acf_data(new double[FFTSIZE]()),
        fft((int)FFTSIZE),
        fft_data(new double[FFTSIZE]()),
        pitch_buf(new float[PITCH_BUF_SIZE]),
        pitch_buf_pos(0),
        wave_data(new sample_t[FFTSIZE]()),
        wave_data_pos(0)
    {
        for(size_t i = 0; i < FFTSIZE; ++i)
            han_window[i] = (0.5 - std::cos((double)i * M_PI * 2 / (double)FFTSIZE) * 0.5) / sample_fsval;

        for(size_t i = 0; i < PITCH_BUF_SIZE; ++i)
            pitch_buf[i] = -1.0f;
    }

    void addData(sample_t sample) {
        wave_data[wave_data_pos] = sample;
        wave_data_pos = (wave_data_pos + 1) % FFTSIZE;
        if (++analyze_cnt == ANALYZE_INTERVAL) {
            analyze();
            analyze_cnt = 0;
        }
    }

    void clearData() {
        wave_data_pos = 0;
        analyze_cnt = 0;
    }

    double get_peak_freq() {
        return peak_freq;
    }

    std::shared_ptr<const double[]> get_fft_buf() {
        return fft_data;
    }

    std::shared_ptr<const float[]> get_pitch_buf() {
        return pitch_buf;
    }

    size_t get_pitch_buf_pos() {
        return pitch_buf_pos;
    }

    size_t get_total_analyze_cnt() {
        return total_analyze_cnt;
    }

    void set_total_analyze_cnt(size_t count) {
        total_analyze_cnt = count;
    }

    void set_threshold(double thres) {
        threshold = thres;
    }

    static float get_interval_sec()
    {
        return (float)ANALYZE_INTERVAL / (float)SAMPLE_FREQ;
    }

    static double sharp_of(const double freq)
    {
        return std::pow(2.0, 1.0/12.0) * freq;
    }

    static float freq_to_cent(double freq)
    {
        if (freq <= 0.0)
            return -1.0;
        else
            return (float)((std::log2(freq) - LOG2_FREQ_C1) * 12.0 * 100.0);
    }

    static double cent_to_freq(float cent)
    {
        if (cent <= 0.0)
            return -1.0;
        else
            return std::pow(2.0, (double)cent / 100.0 / 12.0 + LOG2_FREQ_C1);
    }

    static double power(double v1, double v2)
    {
        return v1*v1 + v2*v2;
    }

protected:
    double threshold;
    size_t analyze_cnt;
    size_t total_analyze_cnt;
    double peak_freq;
    std::unique_ptr<double[]> han_window;
    std::unique_ptr<double[]> acf_data;
    fft4g fft;
    std::shared_ptr<double[]> fft_data;
    std::shared_ptr<float[]> pitch_buf;
    size_t pitch_buf_pos;
    std::unique_ptr<sample_t[]> wave_data;
    size_t wave_data_pos;

    void analyze()
    {
        for (size_t i = 0; i < FFTSIZE; ++i)
            fft_data[i] = han_window[i] * wave_data[(wave_data_pos + i) % FFTSIZE];

        fft.rdft(1, fft_data.get());
        acf_data[0] = power(fft_data[0], 0.0); // it is NOT math power
        acf_data[1] = power(fft_data[1], 0.0);
        for (size_t i = 2; i < FFTSIZE; i += 2) {
            acf_data[i] = power(fft_data[i], fft_data[i + 1]);
            acf_data[i + 1] = 0.0;
        }
        fft.rdft(-1, acf_data.get());

        if (std::sqrt(acf_data[0]) >= threshold)
            peak_freq = detect_pitch();
        else
            peak_freq = -1.0;

        pitch_buf[pitch_buf_pos] = freq_to_cent(peak_freq);
        pitch_buf_pos = (pitch_buf_pos + 1) % PITCH_BUF_SIZE;

        ++total_analyze_cnt;
    }

    double get_fft_value_around_f(double freq) {
        int bin  = (int)(freq * (47.0/48.0) / SAMPLE_FREQ * (double)FFTSIZE);
        int stop = (int)(freq * (49.0/48.0) / SAMPLE_FREQ * (double)FFTSIZE);
        double result = 0.0;
        int at = 0;
        for (; bin <= stop; ++bin) {
            double pw = power(fft_data[bin * 2], fft_data[bin * 2 + 1]);
            if (pw > result) {
                at = bin;
                result = pw;
            }
        }

        result += power(fft_data[(at - 1) * 2], fft_data[(at - 1) * 2 + 1]);
        result += power(fft_data[(at + 1) * 2], fft_data[(at + 1) * 2 + 1]);

        return std::sqrt(result / 3.0);
    }

#ifdef ANALYZER_INTERPOLATION
    inline double parabolic(const double *data, size_t x)
    {
        if (x < 1 || x >= FFTSIZE - 1)
            return (double)x;

        double den = data[x + 1] + data[x - 1] - 2.0 * data[x];
        double delta = data[x - 1] - data[x + 1];
        return (den == 0.0) ? x : (double)x + delta / (2.0 * den);
    }
#endif // ANALYZER_INTERPOLATION

    double detect_pitch()
    {
        int start = (int)(SAMPLE_FREQ / FREQ_C8) - 1;
        int stop = (int)(SAMPLE_FREQ / FREQ_C1) + 1;
        double v = acf_data[start];
        double peakv = 0.0;
        int peaki = 0;
        double slp = -1.0;
        for (int i = 1; i < 5; ++i)
            v = std::max(v, acf_data[start + i]);
        for (int i = start; i < stop; ++i) {
            double nv = acf_data[i + 1];
            for (int j = 1; j < 5; ++j)
                nv = std::max(nv, acf_data[i + j + 1]);

            double nslp = nv - v;
            if (nslp < 0.0 && slp > 0.0 && v > peakv) {
                peaki = i;
                peakv = v;
            }
            if (nslp != 0.0) {
                v = nv;
                slp = nslp;
            }
        }
        if (peaki == 0 || peakv < acf_data[0] * 0.5)
            return -1.0;

        double f0;
        do {
#ifdef ANALYZER_INTERPOLATION
            double f1 = SAMPLE_FREQ / parabolic(acf_data.get(), peaki);
#else
            double f1 = SAMPLE_FREQ / (double)peaki;
#endif // ANALYZER_INTERPOLATION
            double f1mag = get_fft_value_around_f(f1);
            constexpr double mag_thr = 0.24;
            constexpr double odd_scale = 0.06;
            constexpr double even_scale = 1.25;
            // lower harmonics
            if (f1mag >= mag_thr) {
                f0 = f1 / 3.0;
                if (f0 >= FREQ_C1 && get_fft_value_around_f(f0 * 2.0) > f1mag * 3.15)
                    break;

                f0 = f1 / 2.0;
                if (f0 >= FREQ_C1 && get_fft_value_around_f(f1 * 1.5) > f1mag * 1.0)
                    break;
            }

            // higher harmonics
            double f2 = f1 * 2.0;
            double f2mag = get_fft_value_around_f(f2);
            double f3 = f1 * 3.0;
            double f3mag = get_fft_value_around_f(f3);

            if (f2mag >= mag_thr &&
                f2mag > f1mag * even_scale &&
                f3mag < f2mag * odd_scale &&
                f2 <= FREQ_C8) {
                f0 = f2;
                break;
            }

            f0 = f1;
            if (f3mag >= mag_thr &&
                f2mag < f3mag * odd_scale &&
                f3mag > f1mag * even_scale &&
                f3 <= FREQ_C8)
                f0 = f3;

            if (std::sqrt(std::pow(f2mag, 2) + std::pow(f1mag, 2) + std::pow(f3mag, 2)) < 0.7)
                return -1.0;
        } while(0);

#ifdef ANALYZER_INTERPOLATION
        int maxperiods = std::min((int)((double)(FFTSIZE / 2 - 1) * f0 / SAMPLE_FREQ), 5);
        double px = SAMPLE_FREQ / f0;
#else
        int maxperiods = (int)((double)(FFTSIZE / 2 - 1) * f0 / SAMPLE_FREQ);
#endif // ANALYZER_INTERPOLATION
        int peaks = 1;
        for (int period = 2; period <= maxperiods; ++period) {
            int center = (int)((double)period * SAMPLE_FREQ / (f0 / (double)peaks));
            start = center - 3;
            stop = std::min(center + 3, (int)FFTSIZE / 2 - 1);
            peaki = stop;
            peakv = acf_data[peaki];
            for (int i = start; i <= stop; ++i) {
                if (acf_data[i] >= peakv) {
                    peaki = i;
                    peakv = acf_data[i];
                }
            }

#ifdef ANALYZER_INTERPOLATION
            if (peaki == start || peaki == stop)
                break;

            double x = parabolic(acf_data.get(), peaki);
            f0 += SAMPLE_FREQ / (x - px);
            ++peaks;
            px = x;
#else
            if (peaki != start && peaki != stop) {
                f0 += (double)period * SAMPLE_FREQ / (double)peaki;
                ++peaks;
            }
#endif // ANALYZER_INTERPOLATION
        }

        return f0 / (double)peaks;
    }

#ifdef ANALYZER_DEBUG
public:
    // dumps internal buffers to disk
    void dump() {
        std::ofstream f;
        f.open("fft.txt");
        if (f.is_open()) {
            for (size_t i = 0; i < FFTSIZE / 2; i+=2)
                f << fft_data[i] << " " << fft_data[i+1] << std::endl;
            f.close();
        }
        f.open("acf.txt");
        if (f.is_open()) {
            for (size_t i = 0; i < FFTSIZE / 2; ++i)
                f << acf_data[i] << std::endl;
            f.close();
        }
        f.open("han.txt");
        if (f.is_open()) {
            for (size_t i = 0; i < FFTSIZE; ++i)
                f << han_window[i] << std::endl;
            f.close();
        }
        f.open("wave.txt");
        if (f.is_open()) {
            for (size_t i = 0; i < FFTSIZE; ++i)
                f << wave_data[(wave_data_pos + i) % FFTSIZE] << std::endl;
            f.close();
        }
        f.open("pitch.txt");
        if (f.is_open()) {
            for (size_t i = 0; i < PITCH_BUF_SIZE; ++i)
                f << pitch_buf[(pitch_buf_pos + i) % PITCH_BUF_SIZE] << std::endl;
            f.close();
        }
        printf("last pitch: %g\n", peak_freq);
    }
    // fills pitch buffer with aligned to tempo 'checker' pattern, marks current and usable start/end positions
    void test_pattern(size_t BPM)
    {
        pitch_buf_pos = 0;
        total_analyze_cnt = 0;
        const float intervals = float(SAMPLE_FREQ / ANALYZE_INTERVAL) * 120.0f / (float)BPM;
        for (size_t i = 0; i < PITCH_BUF_SIZE; i++)
        {
            pitch_buf[pitch_buf_pos] = std::fmod((double)total_analyze_cnt, intervals) < intervals / 2.0f ? 4500.0f : 4450.0f;
            pitch_buf_pos = (pitch_buf_pos + 1) % PITCH_BUF_SIZE;

            ++total_analyze_cnt;
        }

        // current, start, end markers
        pitch_buf[pitch_buf_pos] = 4600.0f;
        pitch_buf[(pitch_buf_pos + 1) % PITCH_BUF_SIZE] = 4300.0f;
        pitch_buf[(pitch_buf_pos + PITCH_BUF_SIZE - 1) % PITCH_BUF_SIZE] = 4400.0f;
    }
#endif // ANALYZER_DEBUG
};

const double Analyzer::FREQ_C8 = std::pow(2.0, (-9.0/12.0)) * FREQ_A8;
const double Analyzer::FREQ_C7 = std::pow(2.0, (-9.0/12.0)) * FREQ_A7;
const double Analyzer::FREQ_C6 = std::pow(2.0, (-9.0/12.0)) * FREQ_A6;
const double Analyzer::FREQ_C5 = std::pow(2.0, (-9.0/12.0)) * FREQ_A5;
const double Analyzer::FREQ_C4 = std::pow(2.0, (-9.0/12.0)) * FREQ_A4;
const double Analyzer::FREQ_C3 = std::pow(2.0, (-9.0/12.0)) * FREQ_A3;
const double Analyzer::FREQ_C2 = std::pow(2.0, (-9.0/12.0)) * FREQ_A2;
const double Analyzer::FREQ_C1 = std::pow(2.0, (-9.0/12.0)) * FREQ_A1;
const double Analyzer::LOG2_FREQ_C1 = std::log2(FREQ_C1);

const double Analyzer::SAMPLE_FREQ = ANALYZER_SAMPLE_FREQ;
const size_t Analyzer::FFTSIZE = (size_t)std::pow(2.0, std::ceil(std::log2((double)(ANALYZER_SAMPLE_FREQ) / (sharp_of(ANALYZER_BASE_FREQ) - (ANALYZER_BASE_FREQ)))));
const size_t Analyzer::ANALYZE_INTERVAL = (size_t)((double)(ANALYZER_SAMPLE_FREQ) / (ANALYZER_ANALYZE_FREQ));
const size_t Analyzer::PITCH_BUF_SIZE = (ANALYZER_ANALYZE_FREQ) * (ANALYZER_ANALYZE_SPAN);

// perf using X5675 PC3‑10600
// clang -O3, 4096 FFT size, no interpolation, 440.wav
// 13726.46 ev/s avg
