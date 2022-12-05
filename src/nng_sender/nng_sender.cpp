#include "nng_sender.h"

#include <nngpp/protocol/pub0.h>
#include <third_party/libyuv/include/libyuv/convert_from.h>
#include <third_party/libyuv/include/libyuv/video_common.h>

#include <iostream>

NNGSender::NNGSender() : socket_(nng::pub::v0::open()), data_manager_(this) {
    socket_.dial("tcp://127.0.0.1:5567", nng::flag::nonblock);
    std::cout << "NNGSender::ctor" << std::endl;
}

NNGSender::~NNGSender() {
    std::cout << "NNGSender::dtor" << std::endl;
    webrtc::MutexLock lock(&sinks_lock_);
    sinks_.clear();
}

void NNGSender::AddTrack(webrtc::VideoTrackInterface* track,
                         const std::vector<std::string>& stream_ids) {
    if (stream_ids.size() != 1) return;
    const std::string msg(std::string("track/add/") + stream_ids[0] + "/" +
                          track->id());
    SendStringMessage(msg);

    std::cout << "AddTrack" << std::endl;
    std::unique_ptr<Sink> sink(new Sink(this, track, stream_ids[0]));

    webrtc::MutexLock lock(&sinks_lock_);
    sinks_.push_back(std::make_pair(track, std::move(sink)));
}

void NNGSender::RemoveTrack(webrtc::VideoTrackInterface* track) {
    const std::string msg(std::string("track/remove/") + "/" + track->id());
    SendStringMessage(msg);
    std::cout << "RemoveTrack" << std::endl;

    webrtc::MutexLock lock(&sinks_lock_);
    sinks_.erase(
        std::remove_if(sinks_.begin(), sinks_.end(),
                       [track](const VideoTrackSinkVector::value_type& sink) {
                           return sink.first == track;
                       }),
        sinks_.end());
}

void NNGSender::SendStringMessage(const std::string& msg) {
    auto buf = nng::make_buffer(msg.size());
    memcpy(buf.data(), msg.data(), msg.size());
    webrtc::MutexLock lock(&socket_lock_);
    socket_.send(std::move(buf));
}

void NNGSender::SendFrameMessage(const std::string& stream_id,
                                 const std::string& track_id,
                                 uint32_t frame_count, uint32_t width,
                                 uint32_t height, const uint8_t* image_ptr) {
    std::string topic_header("frame/" + stream_id + "/" + track_id + "/");
    size_t sz = topic_header.size() + sizeof(int32_t) * 3 + width * height * 3;

    auto nngbuf = nng::make_buffer(sz);
    uint8_t* buf = static_cast<uint8_t*>(nngbuf.data());

    size_t pos = 0;

    memcpy(&buf[pos], topic_header.data(), topic_header.size());
    pos += topic_header.size();
    //愚直に詰め込み・・・
    buf[pos++] = static_cast<uint8_t>(frame_count >> 24);
    buf[pos++] = static_cast<uint8_t>(frame_count >> 16);
    buf[pos++] = static_cast<uint8_t>(frame_count >> 8);
    buf[pos++] = static_cast<uint8_t>(frame_count);
    buf[pos++] = static_cast<uint8_t>(width >> 24);
    buf[pos++] = static_cast<uint8_t>(width >> 16);
    buf[pos++] = static_cast<uint8_t>(width >> 8);
    buf[pos++] = static_cast<uint8_t>(width);
    buf[pos++] = static_cast<uint8_t>(height >> 24);
    buf[pos++] = static_cast<uint8_t>(height >> 16);
    buf[pos++] = static_cast<uint8_t>(height >> 8);
    buf[pos++] = static_cast<uint8_t>(height);
    memcpy(&buf[pos], image_ptr, width * height * 3);

    webrtc::MutexLock lock(&socket_lock_);
    socket_.send(std::move(nngbuf));
}

void NNGSender::SendDataChannelMessage(int id,
                                       const webrtc::DataBuffer& buffer) {
    std::string topic_header("data/message/" + std::to_string(id) + "/");
    uint32_t data_size = buffer.size();
    size_t sz = topic_header.size() + sizeof(uint32_t) + data_size;

    auto nngbuf = nng::make_buffer(sz);
    uint8_t* buf = static_cast<uint8_t*>(nngbuf.data());
    size_t pos = 0;
    memcpy(&buf[pos], topic_header.data(), topic_header.size());
    pos += topic_header.size();
    buf[pos++] = static_cast<uint8_t>(data_size >> 24);
    buf[pos++] = static_cast<uint8_t>(data_size >> 16);
    buf[pos++] = static_cast<uint8_t>(data_size >> 8);
    buf[pos++] = static_cast<uint8_t>(data_size);
    memcpy(&buf[pos], buffer.data.data<uint8_t>(), data_size);

    webrtc::MutexLock lock(&socket_lock_);
    socket_.send(std::move(nngbuf));
}

NNGSender::Sink::Sink(NNGSender* sender, webrtc::VideoTrackInterface* track,
                      const std::string& stream_id)
    : sender_(sender),
      stream_id_(stream_id),
      track_(track),
      frame_count_(0),
      input_width_(0),
      input_height_(0) {
    std::cout << "Sink ctor: " << stream_id_ << "/" << track->id() << std::endl;
    track_->AddOrUpdateSink(this, rtc::VideoSinkWants());
}

NNGSender::Sink::~Sink() {
    track_->RemoveSink(this);
    std::cout << "Sink dtor: " << stream_id_ << "/" << track_->id()
              << std::endl;
}

webrtc::Mutex* NNGSender::Sink::GetMutex() { return &frame_params_lock_; }

void NNGSender::Sink::OnFrame(const webrtc::VideoFrame& frame) {
    if (frame.width() == 0 || frame.height() == 0) return;
    webrtc::MutexLock lock(GetMutex());
    if (frame.width() != input_width_ || frame.height() != input_height_) {
        input_width_ = frame.width();
        input_height_ = frame.height();
        image_.reset(new uint8_t[input_width_ * input_height_ * 3]);
    }
    auto i420 = frame.video_frame_buffer()->ToI420();

    libyuv::ConvertFromI420(i420->DataY(), i420->StrideY(), i420->DataU(),
                            i420->StrideU(), i420->DataV(), i420->StrideV(),
                            image_.get(), input_width_ * 3, input_width_,
                            input_height_, libyuv::FOURCC_RAW);

    if (frame_count_++ % 100 == 0) {
        std::cout << "OnFrame: " << frame.width() << "x" << frame.height()
                  << " " << static_cast<int>(frame.video_frame_buffer()->type())
                  << " Chroma: " << i420->ChromaWidth() << "x"
                  << i420->ChromaHeight() << " Stride: " << i420->StrideY()
                  << ", " << i420->StrideU() << ", " << i420->StrideV()
                  << " count: " << frame_count_ << std::endl;
    }

    sender_->SendFrameMessage(stream_id_, track_->id(), frame_count_,
                              input_width_, input_height_, &image_[0]);
}