#ifndef CAPTURER_DECODER_H
#define CAPTURER_DECODER_H

#include "logging.h"
#include "producer.h"
#include "ringvector.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

class Decoder : public Producer<AVFrame>
{
    enum
    {
        DEMUXING_EOF  = 0x10,
        VDECODING_EOF = 0x01,
        ADECODING_EOF = 0x02,
        DECODING_EOF  = DEMUXING_EOF | VDECODING_EOF | ADECODING_EOF
    };

public:
    ~Decoder() override { destroy(); }

    // reset and stop the thread
    void reset() override;

    // open input
    int open(const std::string& name, std::map<std::string, std::string> options = {});

    // start thread
    int run() override;

    int produce(AVFrame *frame, int type) override;

    bool empty(int type) override
    {
        switch (type) {
        case AVMEDIA_TYPE_VIDEO: return video_buffer_.empty();
        case AVMEDIA_TYPE_AUDIO: return audio_buffer_.empty();
        default: return true;
        }
    }

    // AV_TIME_BASE
    void seek(const std::chrono::microseconds& ts) override;

    bool has(int) const override;
    std::string format_str(int) const override;
    AVRational time_base(int) const override;
    bool eof() override;

    std::vector<av::vformat_t> vformats() const override { return { vfmt }; }

    std::vector<av::aformat_t> aformats() const override { return { afmt }; }

    int64_t duration() const override { return fmt_ctx_ ? fmt_ctx_->duration : AV_NOPTS_VALUE; }

private:
    int run_f();
    void destroy();

    std::string name_{ "unknown" };

    AVFormatContext *fmt_ctx_{ nullptr };
    AVCodecContext *video_decoder_ctx_{ nullptr };
    AVCodecContext *audio_decoder_ctx_{ nullptr };

    int video_stream_idx_{ -1 };
    int audio_stream_idx_{ -1 };

    int64_t VIDEO_OFFSET_TIME{ 0 };
    int64_t AUDIO_OFFSET_TIME{ 0 };

    AVPacket *packet_{ nullptr };
    AVFrame *decoded_frame_{ nullptr };

    RingVector<AVFrame *, 8> video_buffer_{
        []() { return av_frame_alloc(); },
        [](AVFrame **frame) { av_frame_free(frame); },
    };

    RingVector<AVFrame *, 32> audio_buffer_{
        []() { return av_frame_alloc(); },
        [](AVFrame **frame) { av_frame_free(frame); },
    };
};

#endif //! CAPTURER_DECODER_H
