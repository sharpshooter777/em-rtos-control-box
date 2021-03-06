#ifndef LIVERTSP_H
#define LIVERTSP_H

#include "StateMachine.h"
#include <string>
#include <liveMedia/liveMedia.hh>
#include <BasicUsageEnvironment/BasicUsageEnvironment.hh>

#define LOOP_WATCH_VAR_RUNNING              0
#define LOOP_WATCH_VAR_NOT_RUNNING          1

#define LIVERTSP_CLIENT_VERBOSITY_LEVEL     0
#define LIVERTSP_INTERPACKET_GAP_TIME       3 // how long between no packets to declare stream dead and shut down

#define LIVERTSP_EXIT_OUTPUT_FILE_PROBLEM   -4 // doesn't need a teardown
#define LIVERTSP_EXIT_SERVER_NOT_RESPONDING -3 // doesn't need a teardown
#define LIVERTSP_EXIT_GENERAL_RTSP_ERROR    -2 // doesn't need a teardown
#define LIVERTSP_EXIT_EARLY_RTSP_ERROR      -1 // doesn't need a teardown
#define LIVERTSP_EXIT_CLEAN                  0
#define LIVERTSP_EXIT_DURATION_OVER          1

#define REQUEST_STREAMING_OVER_TCP  false
#define SOCKET_FILE_BUFFER_SIZE     262144

// RTSP 'response handlers'
void continueAfterDESCRIBE(RTSPClient*, int, char*);
void continueAfterSETUP(RTSPClient*, int, char*);
void continueAfterPLAY(RTSPClient*, int, char*);
void continueAfterTEARDOWN(RTSPClient*, int, char*);

void closeSubsessionAfterPlaying(void*); // called when a stream's subsession (e.g., audio or video substream) ends
void closeSinkAfterPlaying(void*);
void subsessionByeHandler(void*); // called when a RTCP "BYE" is received for a subsession
void streamTimerHandler(void*); // called at the end of a stream's expected duration (if the stream has not already signaled its end using a RTCP "BYE")

void setupNextSubsession(RTSPClient*); // used to iterate through each stream's 'subsessions', setting up each one

void setOutputFrameRate(RTSPClient*, unsigned short);
void createOutputFiles(RTSPClient*, char const*);
void createPeriodicOutputFiles(RTSPClient*);
void periodicFileOutputTimerHandler(void*);
void closeMediaSinks(RTSPClient*);
void checkInterPacketGaps(void*);

void shutdownStream(RTSPClient*, int); // used to shut down and close a stream (including its "RTSPClient" object)

// Define a class to hold per-stream state that we maintain throughout each stream's lifetime:
class StreamClientState {
  public:
    StreamClientState();
    virtual ~StreamClientState();

    MediaSubsessionIterator* iter;
    MediaSession* session;
    MediaSubsession* subsession;
    TaskToken streamTimerTask;
    TaskToken periodicFileOutputTask;
    TaskToken interPacketGapCheckTimerTask;
    double duration;
};

// If you're streaming just a single stream (i.e., just from a single URL, once), then you can define and use just a single
// "StreamClientState" structure, as a global variable in your application.  However, because - in this demo application - we're
// showing how to play multiple streams, concurrently, we can't do that.  Instead, we have to have a separate "StreamClientState"
// structure for each "RTSPClient".  To do this, we subclass "RTSPClient", and add a "StreamClientState" field to the subclass:

// Implementation of "MultiRTSPClient":
class MultiRTSPClient: public RTSPClient {
  public:
    MultiRTSPClient(UsageEnvironment& env,
                    char const* rtspURL,
                    int verbosityLevel,
                    char const* applicationName,
                    //portNumBits tunnelOverHTTPPortNum,
                    std::string _videoDirectory,
                    unsigned short _camIndex,
                    unsigned short _clipLength,
                    EM_DATA_TYPE* _em_data);
    virtual ~MultiRTSPClient();

    string origRTSPUrl;
    StreamClientState scs;
    std::string videoDirectory;
    unsigned short camIndex;
    unsigned short clipLength;
    unsigned short frameRate;
    EM_DATA_TYPE* em_data;
};

#endif
