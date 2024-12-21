#include <alsa/asoundlib.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <stdexcept>

// WAV-Datei Header-Struktur
struct WavHeader {
    char riff_tag[4];
    uint32_t riff_length;
    char wave_tag[4];
    char fmt_tag[4];
    uint32_t fmt_length;
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    char data_tag[4];
    uint32_t data_length;
};

bool read_wav_header(const std::string& filename, WavHeader& header, std::vector<uint8_t>& data) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        std::cerr << "Error: Unable to open file " << filename << std::endl;
        return false;
    }

    file.read(reinterpret_cast<char*>(&header), sizeof(WavHeader));

    const std::string_view riff_tag(header.riff_tag, 4);

    if (const std::string_view wave_tag(header.wave_tag, 4); riff_tag != "RIFF" || wave_tag != "WAVE") {
        std::cerr << "Error: Invalid WAV file format in " << filename << std::endl;
        return false;
    }

    data.resize(header.data_length);
    file.read(reinterpret_cast<char*>(data.data()), header.data_length);
    return true;
}

void configure_alsa(snd_pcm_t*& pcm_handle, const WavHeader& header) {
    snd_pcm_hw_params_t* params = nullptr;
    snd_pcm_hw_params_alloca(&params);

    if (snd_pcm_hw_params_any(pcm_handle, params) < 0) {
        throw std::runtime_error("Failed to initialize ALSA hardware parameters.");
    }

    if (snd_pcm_hw_params_set_access(pcm_handle, params, SND_PCM_ACCESS_RW_INTERLEAVED) < 0) {
        throw std::runtime_error("Failed to set ALSA access type.");
    }

    snd_pcm_format_t format;
    switch (header.bits_per_sample) {
        case 16:
            format = SND_PCM_FORMAT_S16_LE;
            break;
        case 24:
            format = SND_PCM_FORMAT_S24_3LE;
            break;
        default:
            throw std::runtime_error("Unsupported bit depth: " + std::to_string(header.bits_per_sample));
    }

    if (snd_pcm_hw_params_set_format(pcm_handle, params, format) < 0) {
        throw std::runtime_error("Failed to set ALSA format.");
    }

    if (snd_pcm_hw_params_set_channels(pcm_handle, params, header.num_channels) < 0) {
        throw std::runtime_error("Failed to set ALSA channels.");
    }

    if (snd_pcm_hw_params_set_rate(pcm_handle, params, header.sample_rate, 0) < 0) {
        throw std::runtime_error("Failed to set ALSA sample rate.");
    }

    if (snd_pcm_hw_params(pcm_handle, params) < 0) {
        throw std::runtime_error("Failed to apply ALSA hardware parameters.");
    }

    snd_pcm_prepare(pcm_handle);
}

void play_audio(snd_pcm_t* pcm_handle, const std::vector<uint8_t>& data, size_t frame_size) {
    const size_t total_frames = data.size() / frame_size;

    for (size_t i = 0; i < total_frames;) {
        snd_pcm_sframes_t frames_written = snd_pcm_writei(pcm_handle, data.data() + i * frame_size, total_frames - i);
        if (frames_written < 0) {
            frames_written = snd_pcm_recover(pcm_handle, frames_written, 0);
        }

        if (frames_written < 0) {
            throw std::runtime_error("Error writing to ALSA device: " + std::string(snd_strerror(frames_written)));
        }

        i += frames_written;
    }

    snd_pcm_drain(pcm_handle);
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <wav-file>" << std::endl;
        return 1;
    }

    const std::string filename = argv[1];
    WavHeader header{};

    try {
        std::vector<uint8_t> data;
        if (!read_wav_header(filename, header, data)) {
            return 1;
        }

        std::cout << "Playing: " << filename << std::endl;
        std::cout << "Sample Rate: " << header.sample_rate << " Hz" << std::endl;
        std::cout << "Channels: " << header.num_channels << std::endl;
        std::cout << "Bit Depth: " << header.bits_per_sample << " bits" << std::endl;

        snd_pcm_t* pcm_handle = nullptr;
        if (snd_pcm_open(&pcm_handle, "default", SND_PCM_STREAM_PLAYBACK, 0) < 0) {
            throw std::runtime_error("Failed to open ALSA PCM device.");
        }

        configure_alsa(pcm_handle, header);

        const size_t frame_size = header.num_channels * (header.bits_per_sample / 8);
        play_audio(pcm_handle, data, frame_size);

        snd_pcm_close(pcm_handle);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
