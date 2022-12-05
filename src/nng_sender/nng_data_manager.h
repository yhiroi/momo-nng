#ifndef NNG_DATA_MANAGER_H_
#define NNG_DATA_MANAGER_H_

#include <api/data_channel_interface.h>
#include <nngpp/nngpp.h>
#include <rtc_base/platform_thread.h>
#include <rtc_base/synchronization/mutex.h>

#include "rtc/rtc_data_manager.h"

class NNGSender;

class NNGDataManager : public RTCDataManager {
   public:
    NNGDataManager(NNGSender* sender);
    ~NNGDataManager();

    void Send(const uint8_t* data, size_t length);

    void OnDataChannel(
        rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel) override;

   protected:
    class NNGDataChannel : public webrtc::DataChannelObserver {
       public:
        NNGDataChannel(
            NNGSender* sender,
            rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel);
        ~NNGDataChannel();

        void OnStateChange() override;
        void OnMessage(const webrtc::DataBuffer& buffer) override;
        void OnBufferedAmountChange(uint64_t previous_amount) override {}
        void Send(const nng::buffer& buffer);

       private:
        NNGSender* sender_;
        rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel_;
    };

   private:
    static void ReceiveThread(void*);

    NNGSender* sender_;
    webrtc::Mutex data_channels_lock_;
    std::vector<NNGDataChannel*> data_channels_;
    webrtc::Mutex socket_lock_;
    nng::socket socket_;
    // std::unique_ptr<rtc::PlatformThread> receive_thread_;
    rtc::PlatformThread receive_thread_;
};

#endif