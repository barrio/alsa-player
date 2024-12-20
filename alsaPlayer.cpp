#include <alsa/asoundlib.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>

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

bool read_wav_header(const std::string& filename, 
                     WavHeader& header, 
                     std::vector<uint8_t>& data) {

    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Fehler beim Öffnen der Datei: " << filename << std::endl;
        return false;
    }

    // Header lesen
    file.read(reinterpret_cast<char*>(&header), sizeof(WavHeader));
    if (strncmp(header.riff_tag, "RIFF", 4) != 0 ||
        strncmp(header.wave_tag, "WAVE", 4) != 0) {
        std::cerr << "Ungültige WAV-Datei: " << filename << std::endl;
        return false;
    }

    // Daten lesen
    data.resize(header.data_length);
    file.read(reinterpret_cast<char*>(data.data()), header.data_length);
    return true;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Verwendung: " << argv[0] << " <wav-datei>" << std::endl;
        return 1;
    }

    const std::string filename = argv[1];
    WavHeader header;
    std::vector<uint8_t> data;

    if (!read_wav_header(filename, header, data)) {
        return 1;
    }

    std::cout << "WAV-Datei: " << filename << std::endl;
    std::cout << "Abtastrate: " << header.sample_rate << " Hz\n";
    std::cout << "Kanäle: " << header.num_channels << "\n";
    std::cout << "Bits pro Sample: " << header.bits_per_sample << "\n";

    snd_pcm_t* pcm_handle;
    snd_pcm_hw_params_t* params;

    // ALSA PCM-Gerät öffnen
    if (snd_pcm_open(&pcm_handle, "pulse", SND_PCM_STREAM_PLAYBACK, 0) < 0) {
        std::cerr << "Fehler beim Öffnen des PCM-Geräts." << std::endl;
        return 1;
    }

    snd_pcm_hw_params_alloca(&params);
    snd_pcm_hw_params_any(pcm_handle, params);
    snd_pcm_hw_params_set_access(pcm_handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(pcm_handle, params, 
        header.bits_per_sample == 16 ? SND_PCM_FORMAT_S16_LE : SND_PCM_FORMAT_U8);
    snd_pcm_hw_params_set_channels(pcm_handle, params, header.num_channels);
    snd_pcm_hw_params_set_rate(pcm_handle, params, header.sample_rate, 0);

    if (snd_pcm_hw_params(pcm_handle, params) < 0) {
        std::cerr << "Fehler beim Festlegen der PCM-Parameter." << std::endl;
        snd_pcm_close(pcm_handle);
        return 1;
    }

    snd_pcm_prepare(pcm_handle);

    // Audio-Daten wiedergeben
    size_t frame_size = header.num_channels * (header.bits_per_sample / 8);
    size_t frames = data.size() / frame_size;

    for (size_t i = 0; i < frames;) {
        snd_pcm_sframes_t frames_written = snd_pcm_writei(pcm_handle, data.data() + i * frame_size, frames - i);
        if (frames_written < 0) {
            frames_written = snd_pcm_recover(pcm_handle, frames_written, 0);
        }
        if (frames_written < 0) {
            std::cerr << "Fehler beim Schreiben der PCM-Daten." << std::endl;
            break;
        }
        i += frames_written;
    }

    snd_pcm_drain(pcm_handle);
    snd_pcm_close(pcm_handle);

    return 0;
}
