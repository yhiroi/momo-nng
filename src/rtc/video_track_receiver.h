#ifndef VIDEO_TRACK_RECEIVER_H_
#define VIDEO_TRACK_RECEIVER_H_

#include <string>

// WebRTC
#include <api/media_stream_interface.h>

class VideoTrackReceiver {
   public:
    virtual void AddTrack(webrtc::VideoTrackInterface* track,
                          const std::vector<std::string>& stream_ids) = 0;
    virtual void RemoveTrack(webrtc::VideoTrackInterface* track) = 0;
};

#endif
