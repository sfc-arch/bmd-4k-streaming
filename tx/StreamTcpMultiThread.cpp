#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <csignal>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "DeckLinkAPI.h"
#include "Stream.h"
#include "Config.h"

static pthread_mutex_t g_sleepMutex;
static pthread_cond_t g_sleepCond;
static int g_videoOutputFile = -1;
static int g_audioOutputFile = -1;
static bool g_do_exit = false;
static int g_video_sock;
static int g_audio_sock;
static struct sockaddr_in g_video_addr;
static struct sockaddr_in g_audio_addr;

static BMDConfig g_config;

static IDeckLinkInput* g_deckLinkInput = NULL;

static unsigned long g_frameCount = 0;

DeckLinkCaptureDelegate::DeckLinkCaptureDelegate() : m_refCount(0)
{
  pthread_mutex_init(&m_mutex, NULL);
}

DeckLinkCaptureDelegate::~DeckLinkCaptureDelegate()
{
  pthread_mutex_destroy(&m_mutex);
}

ULONG DeckLinkCaptureDelegate::AddRef(void)
{
  pthread_mutex_lock(&m_mutex);
    m_refCount++;
  pthread_mutex_unlock(&m_mutex);

  return (ULONG)m_refCount;
}

ULONG DeckLinkCaptureDelegate::Release(void)
{
  pthread_mutex_lock(&m_mutex);
    m_refCount--;
  pthread_mutex_unlock(&m_mutex);

  if (m_refCount == 0)
  {
    delete this;
    return 0;
  }

  return (ULONG)m_refCount;
}

HRESULT DeckLinkCaptureDelegate::VideoInputFrameArrived(IDeckLinkVideoInputFrame* videoFrame, IDeckLinkAudioInputPacket* audioFrame)
{
  IDeckLinkVideoFrame*        rightEyeFrame = NULL;
  IDeckLinkVideoFrame3DExtensions*  threeDExtensions = NULL;
  void*                frameBytes;
  void*                audioFrameBytes;

  int video_buffer_value;
  int audio_buffer_value;
  ioctl(g_video_sock, TIOCOUTQ, &video_buffer_value);
  ioctl(g_audio_sock, TIOCOUTQ, &audio_buffer_value);

  // void* streamFrameBuffer;
  // void* audioStreamFrameBuffer = ;

  // Handle Video Frame
  if (videoFrame)
  {
    // If 3D mode is enabled we retreive the 3D extensions interface which gives.
    // us access to the right eye frame by calling GetFrameForRightEye() .
    if ( (videoFrame->QueryInterface(IID_IDeckLinkVideoFrame3DExtensions, (void **) &threeDExtensions) != S_OK) ||
      (threeDExtensions->GetFrameForRightEye(&rightEyeFrame) != S_OK))
    {
      rightEyeFrame = NULL;
    }

    if (threeDExtensions)
      threeDExtensions->Release();

    if (videoFrame->GetFlags() & bmdFrameHasNoInputSource)
    {
      printf("Frame received (#%lu) - No input signal detected\n", g_frameCount);
    }
    else
    {
      const char *timecodeString = NULL;
      if (g_config.m_timecodeFormat != 0)
      {
        IDeckLinkTimecode *timecode;
        if (videoFrame->GetTimecode(g_config.m_timecodeFormat, &timecode) == S_OK)
        {
          timecode->GetString(&timecodeString);
        }
      }

      if (video_buffer_value > 2000000)
      {
        printf("Video Drop!\n");
        goto bail;
      }

      printf("Frame received (#%lu) %lux%lu [%s] - %s - Size: %li bytes - Video buffer: %d bytes - Audio buffer: %d bytes\n",
        g_frameCount,
        videoFrame->GetWidth(),
        videoFrame->GetHeight(),
        timecodeString != NULL ? timecodeString : "No timecode",
        rightEyeFrame != NULL ? "Valid Frame (3D left/right)" : "Valid Frame",
        videoFrame->GetRowBytes() * videoFrame->GetHeight(),
        video_buffer_value,
        audio_buffer_value);

      videoFrame->GetBytes(&frameBytes);

      // struct stream_info info = { T_STREAM_VIDEO, videoFrame->GetRowBytes() * videoFrame->GetHeight() };
      // send(g_sock, &info, sizeof(stream_info), 0);
      ssize_t video_send_res = send(g_video_sock, frameBytes, videoFrame->GetRowBytes() * videoFrame->GetHeight(), 0);
      printf("Video Sent: %zd\n", video_send_res);
      // write(g_sock, frameBytes, videoFrame->GetRowBytes() * videoFrame->GetHeight());

      // for (uint32_t i = 0; i < videoFrame->GetHeight(); i++) {
      //   for (uint32_t chunk = 0; chunk < chunk_num; chunk++) {
      //     struct stream_header header = { i, chunk };
      //     int chunk_size = videoFrame->GetRowBytes() / chunk_num;
      //     char buf[sizeof(stream_header) + chunk_size];
      //     memcpy(buf, &header, sizeof(stream_header));
      //     memcpy(buf + sizeof(stream_header),
      //            frameBytes + (videoFrame->GetRowBytes() * i) + (chunk_size * chunk),
      //            chunk_size);
      //     int sent_length = sendto(g_sock, buf, sizeof(buf), MSG_DONTWAIT, (struct sockaddr *) &g_addr, sizeof(g_addr));
      //     // printf("Send: [%u:%u] %ibytes\n", i, chunk, sent_length);
      //   }
      //   // printf("Frame send (#%4u)\n", i);
      //   // sendto(g_sock, buf, sizeof(buf), 0, (struct sockaddr *) &g_addr, sizeof(g_addr));
      // }

      if (timecodeString)
        free((void*)timecodeString);

      // if (g_videoOutputFile != -1)
      // {
      //   videoFrame->GetBytes(&frameBytes);
      //   write(g_videoOutputFile, frameBytes, videoFrame->GetRowBytes() * videoFrame->GetHeight());
      //
      //   if (rightEyeFrame)
      //   {
      //     rightEyeFrame->GetBytes(&frameBytes);
      //     write(g_videoOutputFile, frameBytes, videoFrame->GetRowBytes() * videoFrame->GetHeight());
      //   }
      // }
    }

    if (rightEyeFrame)
      rightEyeFrame->Release();

    g_frameCount++;
  }

  // Handle Audio Frame
  if (audioFrame)
  {
    audioFrame->GetBytes(&audioFrameBytes);
    // printf("Audio frame received Sample: %lu, Channel: %d, Depth: %d - Size: %li bytes\n",
    //   audioFrame->GetSampleFrameCount(),
    //   g_config.m_audioChannels,
    //   g_config.m_audioSampleDepth,
    //   audioFrame->GetSampleFrameCount() * g_config.m_audioChannels * (g_config.m_audioSampleDepth / 8));

    if (audio_buffer_value > 60000) {
      printf("Audio Drop!\n");
      goto bail;
    }

    // struct stream_info info = { T_STREAM_AUDIO, audioFrame->GetSampleFrameCount() * g_config.m_audioChannels * (g_config.m_audioSampleDepth / 8) };
    // send(g_audio_sock, &info, sizeof(stream_info), 0);
    ssize_t audo_send_res = send(g_audio_sock, audioFrameBytes, audioFrame->GetSampleFrameCount() * g_config.m_audioChannels * (g_config.m_audioSampleDepth / 8), 0);
    printf("Audio Sent: %zd\n", audo_send_res);

    // printf("Sample Frame Count: %ld\n", audioFrame->GetSampleFrameCount());
    // if (g_audioOutputFile != -1)
    // {
    //   audioFrame->GetBytes(&audioFrameBytes);
    //   write(g_audioOutputFile, audioFrameBytes, audioFrame->GetSampleFrameCount() * g_config.m_audioChannels * (g_config.m_audioSampleDepth / 8));
    // }
  } else {
    printf("Audio frame missing! Maybe video frame delaying?\n");

    // g_deckLinkInput->StopStreams();
    //
    // result = g_deckLinkInput->EnableVideoInput(mode->GetDisplayMode(), pixelFormat, g_config.m_inputFlags);
    // if (result != S_OK)
    // {
    //   fprintf(stderr, "Failed to switch video mode\n");
    //   goto bail;
    // }
    //
    // g_deckLinkInput->StartStreams();
    // g_do_exit = true;
    // pthread_cond_signal(&g_sleepCond);
  }

  // if (g_config.m_maxFrames > 0 && videoFrame && g_frameCount >= g_config.m_maxFrames)
  // {
  //   g_do_exit = true;
  //   pthread_cond_signal(&g_sleepCond);
  // }

  // sleep(1);
bail:
  return S_OK;
}

HRESULT DeckLinkCaptureDelegate::VideoInputFormatChanged(BMDVideoInputFormatChangedEvents events, IDeckLinkDisplayMode *mode, BMDDetectedVideoInputFormatFlags formatFlags)
{
  // This only gets called if bmdVideoInputEnableFormatDetection was set
  // when enabling video input
  HRESULT  result;
  char*  displayModeName = NULL;
  BMDPixelFormat  pixelFormat = bmdFormat10BitYUV;

  if (formatFlags & bmdDetectedVideoInputRGB444)
    pixelFormat = bmdFormat10BitRGB;

  mode->GetName((const char**)&displayModeName);
  printf("Video format changed to %s %s\n", displayModeName, formatFlags & bmdDetectedVideoInputRGB444 ? "RGB" : "YUV");

  if (displayModeName)
    free(displayModeName);

  if (g_deckLinkInput)
  {
    g_deckLinkInput->StopStreams();

    result = g_deckLinkInput->EnableVideoInput(mode->GetDisplayMode(), g_config.m_pixelFormat, g_config.m_inputFlags);
    if (result != S_OK)
    {
      fprintf(stderr, "Failed to switch video mode\n");
      goto bail;
    }

    g_deckLinkInput->StartStreams();
  }

bail:
  return S_OK;
}

static void sigfunc(int signum)
{
  if (signum == SIGINT || signum == SIGTERM)
    g_do_exit = true;

  pthread_cond_signal(&g_sleepCond);
}

int main(int argc, char *argv[]) {
  HRESULT result;
  int exitStatus = 1;
  int idx;

  IDeckLinkIterator* deckLinkIterator = NULL;
  IDeckLink* deckLink = NULL;

  IDeckLinkAttributes* deckLinkAttributes = NULL;
  bool formatDetectionSupported;

  IDeckLinkDisplayModeIterator* displayModeIterator = NULL;
  IDeckLinkDisplayMode* displayMode = NULL;
  char* displayModeName = NULL;
  BMDDisplayModeSupport displayModeSupported;

  DeckLinkCaptureDelegate* delegate = NULL;

  pthread_mutex_init(&g_sleepMutex, NULL);
  pthread_cond_init(&g_sleepCond, NULL);

  signal(SIGINT, sigfunc);
  signal(SIGTERM, sigfunc);
  signal(SIGHUP, sigfunc);

  // Network
  g_video_sock = socket(AF_INET, SOCK_STREAM, 0);
  g_video_addr.sin_family = AF_INET;
  g_video_addr.sin_port = htons(62310);
  g_video_addr.sin_addr.s_addr = inet_addr("192.168.100.31");
  connect(g_video_sock, (struct sockaddr *)&g_video_addr, sizeof(g_video_addr));

  g_audio_sock = socket(AF_INET, SOCK_STREAM, 0);
  g_audio_addr.sin_family = AF_INET;
  g_audio_addr.sin_port = htons(62311);
  g_audio_addr.sin_addr.s_addr = inet_addr("192.168.100.31");
  connect(g_audio_sock, (struct sockaddr *)&g_audio_addr, sizeof(g_audio_addr));

  // Process the command line arguments
  if (!g_config.ParseArguments(argc, argv)) {
    g_config.DisplayUsage(exitStatus);
    goto bail;
  }

  // Get the DeckLink device
  deckLinkIterator = CreateDeckLinkIteratorInstance();
  if (!deckLinkIterator) {
    fprintf(stderr, "This application requires the DeckLink drivers installed.\n");
    goto bail;
  }

  idx = g_config.m_deckLinkIndex;

  while ((result = deckLinkIterator->Next(&deckLink)) == S_OK) {
    if (idx == 0)
      break;
    --idx;

    deckLink->Release();
  }

  if (result != S_OK || deckLink == NULL) {
    fprintf(stderr, "Unable to get DeckLink device %u\n", g_config.m_deckLinkIndex);
    goto bail;
  }

  // Get the input (capture) interface of the DeckLink device
  result = deckLink->QueryInterface(IID_IDeckLinkInput, (void**)&g_deckLinkInput);
  if (result != S_OK)
    goto bail;

  // Get the display mode
  if (g_config.m_displayModeIndex == -1) {
    // Check the card supports format detection
    result = deckLink->QueryInterface(IID_IDeckLinkAttributes, (void**)&deckLinkAttributes);
    if (result == S_OK) {
      result = deckLinkAttributes->GetFlag(BMDDeckLinkSupportsInputFormatDetection, &formatDetectionSupported);
      if (result != S_OK || !formatDetectionSupported) {
        fprintf(stderr, "Format detection is not supported on this device\n");
        goto bail;
      }
    }

    g_config.m_inputFlags |= bmdVideoInputEnableFormatDetection;

    // Format detection still needs a valid mode to start with
    idx = 0;
  } else {
    idx = g_config.m_displayModeIndex;
  }

  result = g_deckLinkInput->GetDisplayModeIterator(&displayModeIterator);
  if (result != S_OK) {
    goto bail;
  }

  while ((result = displayModeIterator->Next(&displayMode)) == S_OK) {
    if (idx == 0) {
      break;
    }
    --idx;

    displayMode->Release();
  }

  if (result != S_OK || displayMode == NULL) {
    fprintf(stderr, "Unable to get display mode %d\n", g_config.m_displayModeIndex);
    goto bail;
  }

  // Get display mode name
  result = displayMode->GetName((const char**)&displayModeName);
  if (result != S_OK) {
    displayModeName = (char *)malloc(32);
    snprintf(displayModeName, 32, "[index %d]", g_config.m_displayModeIndex);
  }

  // Check display mode is supported with given options
  result = g_deckLinkInput->DoesSupportVideoMode(displayMode->GetDisplayMode(), g_config.m_pixelFormat, bmdVideoInputFlagDefault, &displayModeSupported, NULL);
  if (result != S_OK)
    goto bail;

  if (displayModeSupported == bmdDisplayModeNotSupported)
  {
    fprintf(stderr, "The display mode %s is not supported with the selected pixel format\n", displayModeName);
    goto bail;
  }

  if (g_config.m_inputFlags & bmdVideoInputDualStream3D)
  {
    if (!(displayMode->GetFlags() & bmdDisplayModeSupports3D))
    {
      fprintf(stderr, "The display mode %s is not supported with 3D\n", displayModeName);
      goto bail;
    }
  }

  // Print the selected configuration
  g_config.DisplayConfiguration();

  // Configure the capture callback
  delegate = new DeckLinkCaptureDelegate();
  g_deckLinkInput->SetCallback(delegate);

  // Open output files
  // if (g_config.m_videoOutputFile != NULL)
  // {
  //   g_videoOutputFile = open(g_config.m_videoOutputFile, O_WRONLY|O_CREAT|O_TRUNC, 0664);
  //   if (g_videoOutputFile < 0)
  //   {
  //     fprintf(stderr, "Could not open video output file \"%s\"\n", g_config.m_videoOutputFile);
  //     goto bail;
  //   }
  // }
  //
  // if (g_config.m_audioOutputFile != NULL)
  // {
  //   g_audioOutputFile = open(g_config.m_audioOutputFile, O_WRONLY|O_CREAT|O_TRUNC, 0664);
  //   if (g_audioOutputFile < 0)
  //   {
  //     fprintf(stderr, "Could not open audio output file \"%s\"\n", g_config.m_audioOutputFile);
  //     goto bail;
  //   }
  // }

  // Block main thread until signal occurs
  while (!g_do_exit)
  {
    // Start capturing
    result = g_deckLinkInput->EnableVideoInput(displayMode->GetDisplayMode(), g_config.m_pixelFormat, g_config.m_inputFlags);
    if (result != S_OK)
    {
      fprintf(stderr, "Failed to enable video input. Is another application using the card?\n");
      goto bail;
    }

    result = g_deckLinkInput->EnableAudioInput(bmdAudioSampleRate48kHz, g_config.m_audioSampleDepth, g_config.m_audioChannels);
    if (result != S_OK)
      goto bail;

    result = g_deckLinkInput->StartStreams();
    if (result != S_OK)
      goto bail;

    // All Okay.
    exitStatus = 0;

    pthread_mutex_lock(&g_sleepMutex);
    pthread_cond_wait(&g_sleepCond, &g_sleepMutex);
    pthread_mutex_unlock(&g_sleepMutex);

    fprintf(stderr, "Stopping Capture\n");
    g_deckLinkInput->StopStreams();
    g_deckLinkInput->DisableAudioInput();
    g_deckLinkInput->DisableVideoInput();
  }

bail:
  if (g_videoOutputFile != 0)
    close(g_videoOutputFile);

  if (g_audioOutputFile != 0)
    close(g_audioOutputFile);

  if (displayModeName != NULL)
    free(displayModeName);

  if (displayMode != NULL)
    displayMode->Release();

  if (displayModeIterator != NULL)
    displayModeIterator->Release();

  if (g_deckLinkInput != NULL)
  {
    g_deckLinkInput->Release();
    g_deckLinkInput = NULL;
  }

  if (deckLinkAttributes != NULL)
    deckLinkAttributes->Release();

  if (deckLink != NULL)
    deckLink->Release();

  if (deckLinkIterator != NULL)
    deckLinkIterator->Release();

  close(g_video_sock);
  close(g_audio_sock);

  return exitStatus;
}
