#ifndef ZMQ_SENDER_H_
#define ZMQ_SENDER_H_

#include <nngpp/nngpp.h>
#include <rtc/video_track_receiver.h>
#include <rtc_base/synchronization/mutex.h>

#include "nng_data_manager.h"

class NNGDataManager;

class NNGSender : public VideoTrackReceiver {
   public:
    NNGSender();
    ~NNGSender();

    void AddTrack(webrtc::VideoTrackInterface* track,
                  const std::vector<std::string>& stream_ids) override;
    void RemoveTrack(webrtc::VideoTrackInterface* track) override;
    void SendStringMessage(const std::string& msg);
    void SendFrameMessage(const std::string& stream_id,
                          const std::string& track_id, uint32_t frame_count,
                          uint32_t width, uint32_t height,
                          const uint8_t* image_ptr);
    void SendDataChannelMessage(int id, const webrtc::DataBuffer& buffer);

   protected:
    class Sink : public rtc::VideoSinkInterface<webrtc::VideoFrame> {
       public:
        Sink(NNGSender* sender, webrtc::VideoTrackInterface* track,
             const std::string& stream_id);
        ~Sink();

        void OnFrame(const webrtc::VideoFrame& frame) override;
        webrtc::Mutex* GetMutex();

       private:
        NNGSender* sender_;
        std::string stream_id_;
        rtc::scoped_refptr<webrtc::VideoTrackInterface> track_;
        webrtc::Mutex frame_params_lock_;
        std::unique_ptr<uint8_t[]> image_;
        uint32_t frame_count_;
        uint32_t input_width_;
        uint32_t input_height_;
    };

   public:
    NNGDataManager* GetDataManager() { return &data_manager_; }

   private:
    nng::socket socket_;
    webrtc::Mutex sinks_lock_;
    webrtc::Mutex socket_lock_;
    typedef std::vector<
        std::pair<webrtc::VideoTrackInterface*, std::unique_ptr<Sink> > >
        VideoTrackSinkVector;
    VideoTrackSinkVector sinks_;
    std::function<void(std::function<void()>)> dispatch_;

    NNGDataManager data_manager_;
};

#endif