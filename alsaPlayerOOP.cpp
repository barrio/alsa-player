#include <algorithm>
#include <alsa/asoundlib.h>
#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <vector>

// WAV-Datei Header-Struktur
struct WavHeader {
  std::array<char, 4> riff_tag = {};
  uint32_t riff_length = 0;
  std::array<char, 4> wave_tag = {};
  std::array<char, 4> fmt_tag = {};
  uint32_t fmt_length = 0;
  uint16_t audio_format = 0;
  uint16_t num_channels = 0;
  uint32_t sample_rate = 0;
  uint32_t byte_rate = 0;
  uint16_t block_align = 0;
  uint16_t bits_per_sample = 0;
  std::array<char, 4> data_tag = {};
  uint32_t data_length = 0;
};

class WavFile {
public:
  explicit WavFile(const std::string &filename) { readHeader(filename); }

  [[nodiscard]] const WavHeader &getHeader() const { return header; }
  [[nodiscard]] const std::vector<uint8_t> &getData() const { return data; }

private:
  WavHeader header;
  std::vector<uint8_t> data;

  bool readHeader(const std::string &filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) { // Angepasste Bedingung
      throw std::runtime_error("Error: Unable to open file " + filename);
    }

    file.read(reinterpret_cast<char *>(&header), sizeof(WavHeader));
    if (!file) {
      throw std::runtime_error("Error: Unable to read WAV header from " +
                               filename);
    }

    validateHeader();

    data.resize(header.data_length);
    file.read(reinterpret_cast<char *>(data.data()), header.data_length);
    if (!file) {
      throw std::runtime_error("Error: Unable to read WAV data from " +
                               filename);
    }

    return true;
  }

  void validateHeader() const {
    const std::string_view riff_tag(header.riff_tag.data(),
                                    header.riff_tag.size());
    const std::string_view wave_tag(header.wave_tag.data(),
                                    header.wave_tag.size());

    if (riff_tag != "RIFF" || wave_tag != "WAVE") {
      throw std::runtime_error("Error: Invalid WAV file format.");
    }

    if (header.data_length == 0 || header.data_length > 0xFFFFFF) {
      throw std::runtime_error("Error: Invalid data length in WAV file.");
    }
  }
};

class AudioPlayer {
public:
  explicit AudioPlayer(const WavFile &wavFile)
      : header(wavFile.getHeader()), data(wavFile.getData()) {
    openDevice();
    configureDevice();
  }

  ~AudioPlayer() { closeDevice(); }

  void play() const {
    const size_t frameSize = calculateFrameSize();
    validateFrameSize(frameSize);
    const size_t totalFrames = data.size() / frameSize;

    for (size_t i = 0; i < totalFrames;) {
      const size_t framesToWrite = std::min<size_t>(totalFrames - i, 1024);
      snd_pcm_sframes_t framesWritten =
          snd_pcm_writei(pcmHandle, data.data() + i * frameSize, framesToWrite);

      if (framesWritten < 0) {
        framesWritten = snd_pcm_recover(pcmHandle, framesWritten, 0);
      }

      if (framesWritten < 0) {
        throw std::runtime_error("Error writing to ALSA device: " +
                                 std::string(snd_strerror(framesWritten)));
      }

      if (static_cast<size_t>(framesWritten) > framesToWrite) {
        throw std::runtime_error("Unexpected number of frames written.");
      }

      i += static_cast<size_t>(framesWritten);
    }

    snd_pcm_drain(pcmHandle);
  }

private:
  const WavHeader &header;
  const std::vector<uint8_t> &data;
  snd_pcm_t *pcmHandle = nullptr;

  void openDevice() {
    if (snd_pcm_open(&pcmHandle, "default", SND_PCM_STREAM_PLAYBACK, 0) < 0) {
      throw std::runtime_error("Failed to open ALSA PCM device.");
    }
  }

  void configureDevice() const {
    snd_pcm_hw_params_t *params = nullptr;
    snd_pcm_hw_params_alloca(&params);

    if (snd_pcm_hw_params_any(pcmHandle, params) < 0) {
      throw std::runtime_error(
          "Failed to initialize ALSA hardware parameters.");
    }

    if (snd_pcm_hw_params_set_access(pcmHandle, params,
                                     SND_PCM_ACCESS_RW_INTERLEAVED) < 0) {
      throw std::runtime_error("Failed to set ALSA access type.");
    }

    snd_pcm_format_t format = selectFormat();
    if (snd_pcm_hw_params_set_format(pcmHandle, params, format) < 0) {
      throw std::runtime_error("Failed to set ALSA format.");
    }

    if (snd_pcm_hw_params_set_channels(pcmHandle, params, header.num_channels) <
        0) {
      throw std::runtime_error("Failed to set ALSA channels.");
    }

    if (snd_pcm_hw_params_set_rate(pcmHandle, params, header.sample_rate, 0) <
        0) {
      throw std::runtime_error("Failed to set ALSA sample rate.");
    }

    if (snd_pcm_hw_params(pcmHandle, params) < 0) {
      throw std::runtime_error("Failed to apply ALSA hardware parameters.");
    }

    snd_pcm_prepare(pcmHandle);
  }

  [[nodiscard]] snd_pcm_format_t selectFormat() const {
    switch (header.bits_per_sample) {
    case 16:
      return SND_PCM_FORMAT_S16_LE;
    case 24:
      return SND_PCM_FORMAT_S24_3LE;
    default:
      throw std::runtime_error("Unsupported bit depth: " +
                               std::to_string(header.bits_per_sample));
    }
  }

  [[nodiscard]] size_t calculateFrameSize() const {
    return header.num_channels * (header.bits_per_sample / 8);
  }

  void validateFrameSize(size_t frameSize) const {
    if (frameSize == 0 || data.size() % frameSize != 0) {
      throw std::runtime_error("Invalid frame size or corrupted data.");
    }
  }

  void closeDevice() const {
    if (pcmHandle) {
      snd_pcm_close(pcmHandle);
    }
  }
};

int main(int argc, char *argv[]) {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " <wav-file>" << std::endl;
    return 1;
  }

  try {
    WavFile wavFile(argv[1]);
    AudioPlayer player(wavFile);
    player.play();
  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}
