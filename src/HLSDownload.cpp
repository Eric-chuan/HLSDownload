#include "HLSDownload.h"

static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    MemoryStruct *mem = (MemoryStruct*)userp;
    if (mem->reserved == 0)
    {
        CURLcode res;
        double filesize = 0.0;

        res = curl_easy_getinfo(mem->c, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &filesize);
        if((CURLE_OK == res) && (filesize>0.0))
        {
            mem->memory = (char*)realloc(mem->memory, (int)filesize + 2);
            if (mem->memory == NULL) {
                return 0;
            }
            mem->reserved = (int)filesize + 1;
        }
    }
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;
    return realsize;
}

void HLSDownload::init(char* input_url)
{
    this->master_url = input_url;
    curl_global_init(CURL_GLOBAL_ALL);

    this->master_playlist = new HLSMasterPlaylist();
    memset(master_playlist, 0x00, sizeof(HLSMasterPlaylist));
    this->master_playlist->url = master_url;
    size_t size = 0;
    get_hls_data_from_url(this->master_url, &(this->master_playlist->source), &size, STRING);
    init_media_playlists();
    init_media_segments();
}
void HLSDownload::destroy()
{
    curl_global_cleanup();
}
int HLSDownload::init_media_playlists()
{
    int me_index = 0;
    bool url_expected = false;
    char *src = this->master_playlist->source;
    while(*src != '\0'){
        char *end_ptr = strchr(src, '\n');
        if (!end_ptr) {
            break;
        }
        *end_ptr = '\0';
        if (*src == '#') {
            url_expected = false;
            if (!strncmp(src, "#EXT-X-STREAM-INF:", 18)) {
                url_expected = true;
            }
        } else if (url_expected) {
            size_t len = strlen(src);
            // here we will fill new playlist
            this->media_playlists[me_index] = (HLSMediaPlaylist*)malloc(sizeof(HLSMediaPlaylist));
            memset(this->media_playlists[me_index], 0x00, sizeof(HLSMediaPlaylist));
            size_t max_length = len + strlen(this->master_url) + 10;
            char* extend_url = (char*)malloc(max_length);
            sprintf(extend_url, "%s/../%s", this->master_url, src);
            this->media_playlists[me_index]->url = extend_url;
            me_index++;
            url_expected = false;
        }
        src = end_ptr + 1;
    }
    this->stream_num = me_index;
    return 0;
}
int HLSDownload::init_media_segments()
{
    size_t size = 0;
    for (int i = 0; i < stream_num; i++) {
        get_hls_data_from_url(this->media_playlists[i]->url, &this->media_playlists[i]->source, &size, STRING);
        bool url_expected = false;
        int seg_index = 0;
        char *src = this->media_playlists[i]->source;
        while(*src != '\0'){
            char *end_ptr = strchr(src, '\n');
            if (!end_ptr) {
                break;
            }
            *end_ptr = '\0';
            if (*src == '#') {
                url_expected = false;
                if (!strncmp(src, "#EXTINF:", 8)) {
                    url_expected = true;
                }
            } else if (url_expected) {
                size_t len = strlen(src);
                this->media_playlists[i]->media_segments[seg_index] = (HLSMediaSegment*)malloc(sizeof(HLSMediaSegment));
                memset(this->media_playlists[i]->media_segments[seg_index], 0x00, sizeof(HLSMediaSegment));
                // here we will fill new playlist
                size_t max_length = len + strlen(this->media_playlists[i]->url) + 10;
                char* extend_url = (char*)malloc(max_length);
                char* sub_url = strstr(this->media_playlists[i]->url, "master");
                char* head_url = (char*)malloc(sub_url - this->media_playlists[i]->url);
                memcpy(head_url, this->media_playlists[i]->url, sub_url -this->media_playlists[i]->url);
                sprintf(extend_url, "%s%s/../%s", head_url, sub_url + 15, src);
                this->media_playlists[i]->media_segments[seg_index]->url = extend_url;
                seg_index++;
                url_expected = false;
            }
            src = end_ptr + 1;
        }
    }
    return 0;
}
long HLSDownload::get_hls_data_from_url(char* url, char** data, size_t *size, int type)
{
    CURL *c = curl_easy_init();
    CURLcode res;
    long http_code = 0;
    MemoryStruct chunk;
    chunk.memory = (char*)malloc(1);
    chunk.memory[0] = '\0';
    chunk.size = 0;
    chunk.reserved = 0;
    chunk.c = c;
    curl_easy_setopt(c, CURLOPT_URL, url);
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, (void *)&chunk);
    if (type == BINARY) {
        curl_easy_setopt(c, CURLOPT_LOW_SPEED_LIMIT, 2L);
        curl_easy_setopt(c, CURLOPT_LOW_SPEED_TIME, 3L);
    }
    curl_easy_setopt(c, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(c, CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
    res = curl_easy_perform(c);
    curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &http_code);
    if (type == STRING) {
        *data = strdup(chunk.memory);
    } else if (type == BINARY) {
        *data = (char*)malloc(chunk.size);
        if (chunk.size > 16 && (chunk.memory[0] == 0x89 && chunk.memory[1] == 0x50 && chunk.memory[2] == 0x4E)) {
            *data = (char*)memcpy(*data, chunk.memory + 16, chunk.size);
        } else {
            *data = (char*)memcpy(*data, chunk.memory, chunk.size);
        }
    }
    *size = chunk.size;
    if (chunk.memory) {
        free(chunk.memory);
    }
    curl_easy_cleanup(c);
    return http_code;
}
void HLSDownload::vod_download_segment()
{
    FILE *out_file = fopen("../out.ts", "wb");
    for (int i = 0; i < 1; i++) {
        HLSMediaSegment* ms = this->media_playlists[1]->media_segments[i];
        int tries = 20;
        long http_code = 0;
        while(tries) {
            http_code = get_hls_data_from_url(ms->url, (char**)&(ms->data), (size_t*)&(ms->data_len), BINARY);
            if (200 != http_code || ms->data_len == 0) {
                --tries;
                usleep(1000);
                continue;
            }
            break;
        }
        fwrite(ms->data, ms->data_len, sizeof(uint8_t), out_file);
        this->segment_ts_len = ms->data_len;
        this->segment_ts_data = (uint8_t*)malloc(ms->data_len * sizeof(uint8_t));
        memcpy(this->segment_ts_data, ms->data, ms->data_len);
    }
    fclose(out_file);
}
bool HLSDownload::get_ts_info()
{
    return true;
}
void HLSDownload::ts_to_es()
{
    unsigned int ts_position = 0;
    unsigned int received_length = 0;
    unsigned int available_length = 0;
    unsigned int max_ts_length = 65536;
    unsigned int es_packet_count = 0;
    bool start_flag = 0;
    bool received_flag = 0;
    uint8_t* payload_position = NULL;
    //char buffer_data[188*204*5] = {};
    uint8_t* es_buffer = (uint8_t*)malloc(max_ts_length);
    uint8_t* current_p = this->segment_ts_data;
    int frame_cnt = 0;
    int buffer_len = 188*204*5;
    unsigned int read_size = (this->segment_ts_len >= buffer_len) ? buffer_len : this->segment_ts_len;
    uint8_t* buffer_data = segment_ts_data;
    TSHeader* ts_header;
    PESInfo* pes;
    FILE* es_fp = fopen("../demo.hevc", "wb");
    int packet_length = 188;

    while(current_p - segment_ts_data < segment_ts_len){
        //printf("len = %d\n", current_p - segment_ts_data);
        //current_p = buffer_data;
        while(current_p < buffer_data + read_size){
            ts_header = new TSHeader(current_p);
            if(ts_header->PID == 256){
                es_packet_count++;
                if(ts_header->adapation_field_control == 1){
                    payload_position = current_p + 4;
                }else if(ts_header->adapation_field_control == 3){
                    payload_position = current_p + 4 + current_p[4] + 1;
                }
                if(ts_header->payload_uint_start_indicator != 0){
                    start_flag =1;
                    frame_cnt++;
                }
                if(start_flag && payload_position){
                    available_length = packet_length + current_p -payload_position;
                    if(ts_header->payload_uint_start_indicator != 0){
                        if(received_length > 0){
                            pes = new PESInfo(es_buffer);
                            if(pes->packet_start_code_prefix != 0x000001){
                                printf("pes is not correct.received %d es packet\n",es_packet_count);
                                return;
                            }
                            if(pes->PES_packet_data_length != 0){
                                fwrite(pes->elementy_stream_position, received_length, 1, es_fp);
                            }
                        memset(es_buffer, 0, received_length);
                        received_length = 0;
                       }
                        received_flag = 1;
                    }
                    if(received_flag){
                        if(received_length + available_length > max_ts_length){
                            max_ts_length = max_ts_length * 2;
                            es_buffer = (uint8_t*)realloc(es_buffer,max_ts_length);
                        }
                        memcpy(es_buffer + received_length, payload_position, available_length);
                        received_length += available_length;
                        if (segment_ts_data + segment_ts_len - current_p == 188) {
                            printf("end at frame %d\n", frame_cnt);
                            pes = new PESInfo(es_buffer);
                            if(pes->PES_packet_data_length != 0){
                                fwrite(pes->elementy_stream_position, received_length, 1, es_fp);
                            }
                        }
                    }

                }
            }
        current_p += packet_length;
        }
        read_size = (segment_ts_data - current_p >= buffer_len - segment_ts_len) ? buffer_len : segment_ts_data - current_p + segment_ts_len;
        buffer_data = current_p;
    }
    printf("the packet number is: %d\n"
           "and the demo.pes, demo.es has been saved at current directory.\n",es_packet_count);
}

