#include <curl/curl.h>
#include "HLSDownload.h"


int main(int argc, char *argv[])
{
    char input_url[100] = "http://localhost/HLS/master.m3u8";
    HLSDownload hlsdl;
    hlsdl.init(input_url);
    hlsdl.vod_download_segment();
    hlsdl.ts_to_es();
    return 0;
}