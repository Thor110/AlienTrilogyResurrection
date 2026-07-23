#include "VideoPlayer.h"
#include "Bootstrap/AppWindow.h"

#include <SDL3/SDL.h>
#include <algorithm>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

namespace ALTEngine::Video
{
    namespace
    {
        struct FFmpegContext
        {
            AVFormatContext* formatCtx = nullptr;
            AVCodecContext* videoCodecCtx = nullptr;
            AVCodecContext* audioCodecCtx = nullptr;
            SwsContext* swsCtx = nullptr;
            SwrContext* swrCtx = nullptr;
            int videoStreamIndex = -1;
            int audioStreamIndex = -1;

            ~FFmpegContext()
            {
                if (swsCtx) { sws_freeContext(swsCtx); }
                if (swrCtx) { swr_free(&swrCtx); }
                if (videoCodecCtx) { avcodec_free_context(&videoCodecCtx); }
                if (audioCodecCtx) { avcodec_free_context(&audioCodecCtx); }
                if (formatCtx) { avformat_close_input(&formatCtx); }
            }
        };

        bool OpenStreams(FFmpegContext& ctx, const std::string& pathStr)
        {
            if (avformat_open_input(&ctx.formatCtx, pathStr.c_str(), nullptr, nullptr) != 0)
            {
                SDL_Log("VideoPlayer: could not open %s", pathStr.c_str());
                return false;
            }
            if (avformat_find_stream_info(ctx.formatCtx, nullptr) < 0)
            {
                SDL_Log("VideoPlayer: could not find stream info for %s", pathStr.c_str());
                return false;
            }

            ctx.videoStreamIndex = av_find_best_stream(ctx.formatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
            ctx.audioStreamIndex = av_find_best_stream(ctx.formatCtx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);

            if (ctx.videoStreamIndex < 0)
            {
                SDL_Log("VideoPlayer: no video stream in %s", pathStr.c_str());
                return false;
            }

            AVCodecParameters* videoParams = ctx.formatCtx->streams[ctx.videoStreamIndex]->codecpar;
            const AVCodec* videoDecoder = avcodec_find_decoder(videoParams->codec_id);
            if (!videoDecoder)
            {
                SDL_Log("VideoPlayer: no decoder available for video codec in %s", pathStr.c_str());
                return false;
            }
            ctx.videoCodecCtx = avcodec_alloc_context3(videoDecoder);
            avcodec_parameters_to_context(ctx.videoCodecCtx, videoParams);
            if (avcodec_open2(ctx.videoCodecCtx, videoDecoder, nullptr) < 0)
            {
                SDL_Log("VideoPlayer: could not open video codec for %s", pathStr.c_str());
                return false;
            }

            // Audio is optional - a video-only file (or one whose audio
            // codec we can't decode) still plays, just silently.
            if (ctx.audioStreamIndex >= 0)
            {
                AVCodecParameters* audioParams = ctx.formatCtx->streams[ctx.audioStreamIndex]->codecpar;
                const AVCodec* audioDecoder = avcodec_find_decoder(audioParams->codec_id);
                if (audioDecoder)
                {
                    ctx.audioCodecCtx = avcodec_alloc_context3(audioDecoder);
                    avcodec_parameters_to_context(ctx.audioCodecCtx, audioParams);
                    if (avcodec_open2(ctx.audioCodecCtx, audioDecoder, nullptr) < 0)
                    {
                        avcodec_free_context(&ctx.audioCodecCtx);
                        ctx.audioStreamIndex = -1;
                    }
                }
                else
                {
                    ctx.audioStreamIndex = -1;
                }
            }

            return true;
        }
    }

    bool VideoPlayer::Play(const std::filesystem::path& path)
    {
        FFmpegContext ctx;
        std::string pathStr = path.string();
        if (!OpenStreams(ctx, pathStr))
        {
            return true; // couldn't play this one - skip it, don't abort the whole boot
        }

        ALTEngine::Bootstrap::AppWindow& app = ALTEngine::Bootstrap::AppWindow::Instance();
        if (!app.EnsureCreated())
        {
            return false;
        }
        SDL_Renderer* renderer = app.Renderer();

        int videoW = ctx.videoCodecCtx->width;
        int videoH = ctx.videoCodecCtx->height;

        SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, videoW, videoH);
        SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);

        ctx.swsCtx = sws_getContext(
            videoW, videoH, ctx.videoCodecCtx->pix_fmt,
            videoW, videoH, AV_PIX_FMT_RGBA,
            SWS_BILINEAR, nullptr, nullptr, nullptr);

        SDL_AudioStream* audioStream = nullptr;
        if (ctx.audioStreamIndex >= 0)
        {
            // Don't guess the SDL stream format from the source codec -
            // that only worked for the original game's araw (literally
            // raw PCM) audio. Real codecs (AAC etc, needed for the
            // upscaled movie replacements) commonly decode to planar
            // float, not S16/U8, and planar formats split channels into
            // separate buffers rather than interleaving them - reading
            // frame->data[0] alone as if it were full interleaved audio
            // (the old code's assumption) reads garbage past a single
            // channel's worth of data. swresample converts whatever the
            // decoder actually produces into one fixed, known format
            // (interleaved S16) up front, so the rest of this function
            // never needs to care what the source codec's native decode
            // format is.
            int ret = swr_alloc_set_opts2(&ctx.swrCtx,
                &ctx.audioCodecCtx->ch_layout, AV_SAMPLE_FMT_S16, ctx.audioCodecCtx->sample_rate,
                &ctx.audioCodecCtx->ch_layout, ctx.audioCodecCtx->sample_fmt, ctx.audioCodecCtx->sample_rate,
                0, nullptr);
            if (ret < 0 || !ctx.swrCtx || swr_init(ctx.swrCtx) < 0)
            {
                SDL_Log("VideoPlayer: swr_alloc_set_opts2/swr_init failed - playing without audio");
                if (ctx.swrCtx) { swr_free(&ctx.swrCtx); }
            }
            else
            {
                SDL_AudioSpec srcSpec{};
                srcSpec.format = SDL_AUDIO_S16;
                srcSpec.channels = ctx.audioCodecCtx->ch_layout.nb_channels;
                srcSpec.freq = ctx.audioCodecCtx->sample_rate;
                audioStream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &srcSpec, nullptr, nullptr);
                if (audioStream) { SDL_ResumeAudioStreamDevice(audioStream); }
            }
        }

        AVPacket* packet = av_packet_alloc();
        AVFrame* frame = av_frame_alloc();
        AVFrame* rgbaFrame = av_frame_alloc();
        std::vector<uint8_t> rgbaBuffer(static_cast<size_t>(videoW) * videoH * 4);
        av_image_fill_arrays(rgbaFrame->data, rgbaFrame->linesize, rgbaBuffer.data(), AV_PIX_FMT_RGBA, videoW, videoH, 1);

        double videoTimeBase = av_q2d(ctx.formatCtx->streams[ctx.videoStreamIndex]->time_base);
        Uint64 playbackStartTicks = SDL_GetTicks();

        bool closedByUser = false;
        bool stop = false;

        while (!stop)
        {
            SDL_Event event;
            while (SDL_PollEvent(&event))
            {
                if (event.type == SDL_EVENT_QUIT) { closedByUser = true; stop = true; }
                else if (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) { stop = true; }
            }
            if (stop) { break; }

            int readResult = av_read_frame(ctx.formatCtx, packet);
            if (readResult < 0) { break; } // end of stream

            if (packet->stream_index == ctx.videoStreamIndex)
            {
                if (avcodec_send_packet(ctx.videoCodecCtx, packet) == 0)
                {
                    while (avcodec_receive_frame(ctx.videoCodecCtx, frame) == 0)
                    {
                        sws_scale(ctx.swsCtx, frame->data, frame->linesize, 0, videoH, rgbaFrame->data, rgbaFrame->linesize);

                        // Pace to the frame's own presentation timestamp -
                        // correct regardless of the container's nominal
                        // frame rate (documented as varying ~14.99-15.01).
                        if (frame->best_effort_timestamp != AV_NOPTS_VALUE)
                        {
                            double targetSeconds = static_cast<double>(frame->best_effort_timestamp) * videoTimeBase;
                            Uint64 targetTicks = playbackStartTicks + static_cast<Uint64>(targetSeconds * 1000.0);
                            Uint64 now = SDL_GetTicks();
                            if (targetTicks > now) { SDL_Delay(static_cast<Uint32>(targetTicks - now)); }
                        }

                        SDL_UpdateTexture(texture, nullptr, rgbaBuffer.data(), videoW * 4);

                        int windowW = 0, windowH = 0;
                        SDL_GetRenderOutputSize(renderer, &windowW, &windowH);
                        float scale = std::min(static_cast<float>(windowW) / videoW, static_cast<float>(windowH) / videoH);
                        float destW = videoW * scale;
                        float destH = videoH * scale;
                        SDL_FRect dest{ (windowW - destW) / 2.0f, (windowH - destH) / 2.0f, destW, destH };

                        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
                        SDL_RenderClear(renderer);
                        SDL_RenderTexture(renderer, texture, nullptr, &dest);
                        SDL_RenderPresent(renderer);
                    }
                }
            }
            else if (packet->stream_index == ctx.audioStreamIndex && audioStream)
            {
                if (avcodec_send_packet(ctx.audioCodecCtx, packet) == 0)
                {
                    while (avcodec_receive_frame(ctx.audioCodecCtx, frame) == 0)
                    {
                        // Worst case (no resampling, since in/out sample
                        // rate match - this only converts format/layout)
                        // output sample count equals input sample count.
                        int maxOutSamples = frame->nb_samples;
                        std::vector<uint8_t> converted(static_cast<size_t>(maxOutSamples) * frame->ch_layout.nb_channels * 2); // S16 = 2 bytes/sample
                        uint8_t* outPtr = converted.data();

                        int convertedSamples = swr_convert(ctx.swrCtx, &outPtr, maxOutSamples,
                                                            const_cast<const uint8_t**>(frame->data), frame->nb_samples);
                        if (convertedSamples > 0)
                        {
                            int dataSize = convertedSamples * frame->ch_layout.nb_channels * 2;
                            SDL_PutAudioStreamData(audioStream, converted.data(), dataSize);
                        }
                    }
                }
            }

            av_packet_unref(packet);
        }

        if (audioStream) { SDL_DestroyAudioStream(audioStream); }
        av_frame_free(&frame);
        av_frame_free(&rgbaFrame);
        av_packet_free(&packet);
        SDL_DestroyTexture(texture);

        return !closedByUser;
    }
}
