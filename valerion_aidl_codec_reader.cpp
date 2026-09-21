
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <unistd.h>
#include <time.h>

using GetItemFn = int (*)(const std::string&, std::string*);

static void stamp(char* buf, size_t n)
{
  struct timespec ts{};
  clock_gettime(CLOCK_REALTIME, &ts);
  struct tm tmv{};
  localtime_r(&ts.tv_sec, &tmv);
  snprintf(buf, n, "%02d:%02d:%02d.%03ld",
           tmv.tm_hour, tmv.tm_min, tmv.tm_sec, ts.tv_nsec / 1000000L);
}

static int extract_codec(const std::string& s)
{
  const char* keys[] = {"\"codec_type\"", "codec_type"};
  for (const char* key : keys)
  {
    size_t p = s.find(key);
    if (p == std::string::npos)
      continue;
    p = s.find(':', p);
    if (p == std::string::npos)
      continue;
    ++p;
    while (p < s.size() && (s[p] == ' ' || s[p] == '\t' || s[p] == '"'))
      ++p;
    char* end = nullptr;
    long v = strtol(s.c_str() + p, &end, 0);
    if (end != s.c_str() + p)
      return static_cast<int>(v);
  }
  return -999;
}

static const char* codec_name(int c)
{
  switch (c)
  {
    case 1: return "PCM";
    case 5: return "DDP";
    case 6: return "TRUEHD";
    case 7: return "MAT";
    case 8: return "DDP_ATMOS";
    case 9: return "TRUEHD_ATMOS";
    case 10: return "MAT_ATMOS";
    case 11: return "DTS";
    case 12: return "DTS_HD";
    case 13: return "DTS_X";
    default: return "UNKNOWN";
  }
}

int main(int argc, char** argv)
{
  int count = 30;
  int interval_ms = 1000;
  if (argc > 1) count = atoi(argv[1]);
  if (argc > 2) interval_ms = atoi(argv[2]);
  if (count <= 0) count = 30;
  if (interval_ms < 100) interval_ms = 100;

  const char* libPath = "/vendor/lib/libhs_av_aidl_client.so";
  void* h = dlopen(libPath, RTLD_NOW | RTLD_LOCAL);
  if (!h)
  {
    fprintf(stderr, "DLOPEN_FAIL %s: %s\n", libPath, dlerror());
    return 10;
  }

  const char* sym =
      "_ZN8platform8aidl_rpc12AvAidlClient7getItemERKNSt3__112basic_stringIcNS2_11char_traitsIcEENS2_9allocatorIcEEEEPS8_";

  dlerror();
  auto getItem = reinterpret_cast<GetItemFn>(dlsym(h, sym));
  const char* err = dlerror();
  if (!getItem || err)
  {
    fprintf(stderr, "DLSYM_FAIL AvAidlClient::getItem: %s\n",
            err ? err : "unknown");
    dlclose(h);
    return 11;
  }

  const std::string request = "{\"cmd\":\"GetCodecType\"}";

  printf("VALERION_AIDL_CODEC_READER_READY count=%d interval_ms=%d\n",
         count, interval_ms);
  printf("request=%s\n", request.c_str());
  printf("expected: codec_type 7=MAT, 10=MAT_ATMOS\n");
  fflush(stdout);

  for (int i = 0; i < count; ++i)
  {
    std::string response;
    int ret = getItem(request, &response);
    int codec = extract_codec(response);

    char tbuf[32];
    stamp(tbuf, sizeof(tbuf));

    if (codec != -999)
      printf("%s | ret=%d | codec=%d (%s) | response=%s\n",
             tbuf, ret, codec, codec_name(codec), response.c_str());
    else
      printf("%s | ret=%d | codec=PARSE_FAIL | response=%s\n",
             tbuf, ret, response.c_str());

    fflush(stdout);
    usleep(static_cast<useconds_t>(interval_ms) * 1000U);
  }

  dlclose(h);
  return 0;
}
