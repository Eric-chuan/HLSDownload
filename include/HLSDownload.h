#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

#define STRING 0x0001
#define BINARY 0x0002
#define MASTER_PLAYLIST 0
#define MEDIA_PLAYLIST 1

#define PROGRAM_STREAM_MAP  0xBC
#define PADDING_STREAM  0xBE
#define PRIVATE_STREAM_2  0xBF
#define ECM_STREAM  0xF0
#define EMM_STREAM 0xF1
#define PROGRAM_STREAM_DIRECTORY  0xFF
#define DSMCC_STREAM  0xF2
#define E_STREAM  0xF8
#define MAX_INT32_LEN   (sizeof("-2147483648") - 1)

typedef struct HLS_Media_Segment {
    char *url;
    int64_t offset;
    int64_t size;
    uint64_t duration_ms;
    struct HLS_Media_Segment *next;
    struct HLS_Media_Segment *prev;
    uint8_t *data;
    int data_len;
} HLSMediaSegment;

typedef struct HLS_Media_Playlist {
    char *url;
    char *source;
    uint64_t target_duration_ms;
    uint64_t total_duration_ms;
    bool is_endlist;
    int first_media_sequence;
    int last_media_sequence;
    HLSMediaSegment *media_segments[8];
    HLSMediaSegment *first_media_segment;
    HLSMediaSegment *last_media_segment;
    struct HLS_Media_Playlist *next;
} HLSMediaPlaylist;

typedef struct HLS_Master_Playlist {
    char *url;
    char *source;
} HLSMasterPlaylist;

typedef struct Memory_Struct {
    char *memory;
    size_t size;
    size_t reserved;
    CURL *c;
} MemoryStruct;

typedef struct TS_Header {
    uint8_t* TSHeader_data;
    unsigned int syntax_indicator;                //8b
    unsigned int transport_error_indicator;       //1b
    unsigned int payload_uint_start_indicator;    //1b
    unsigned int transport_pritxy;                //1b
    unsigned int PID;                             //13b
    unsigned int scrmling_control;                //2b
    unsigned int adapation_field_control;         //2b
    unsigned int continue_counter;                //4b
    TS_Header(uint8_t* data):
        TSHeader_data(data),
        syntax_indicator(TSHeader_data[0]),
        transport_error_indicator((data[1]&0x80)>>7),
        payload_uint_start_indicator((data[1]&0x40)>>6),
        transport_pritxy((TSHeader_data[1]&0x20)>>5),
        PID((TSHeader_data[1]&0x1F)<<8 | data[2]),
        scrmling_control((TSHeader_data[3]&0xC0)>>6),
        adapation_field_control((TSHeader_data[3]&0x30)>>4),
        continue_counter(TSHeader_data[3]&0x0F)
        {
            if(syntax_indicator != 0x47){
                /*   */
            }
        }
} TSHeader;

typedef struct PES_Info {
    uint8_t* PES_data;
    uint8_t* elementy_stream_position;
    unsigned int PES_packet_data_length;

    unsigned int packet_start_code_prefix;        //24b
    unsigned int stream_id;                       //8b
    unsigned int PES_packet_length;               //16b
    // '10';                                  //2b
    unsigned int PES_scrambling_control;          //2b
    unsigned int PES_priority;                    //1b
    unsigned int data_alignment_indicator;        //1b
    unsigned int copyright;                       //1b
    unsigned int original_or_copy;                //1b
    unsigned int PTS_DTS_flags;                   //2b
    unsigned int ESCR_flag;                       //1b
    unsigned int ES_rate_flag;                    //1b
    unsigned int DSM_trick_mode_flag;             //1b
    unsigned int additional_copy_info_flag;       //1b
    unsigned int PES_CRC_flag;                    //1b
    unsigned int PES_extension_flag;              //1b
    unsigned int PES_header_data_length;          //8b
    PES_Info(uint8_t* data):
        PES_data(data),
        packet_start_code_prefix((PES_data[0]<<16)|(PES_data[1]<<8)|(PES_data[2])),
        stream_id(PES_data[3]),
        PES_packet_length((PES_data[4]<<8)|(PES_data[5]))
        {
            if(stream_id != PROGRAM_STREAM_MAP &&
                stream_id != PADDING_STREAM &&
                stream_id != PRIVATE_STREAM_2 &&
                stream_id != ECM_STREAM &&
                stream_id != EMM_STREAM &&
                stream_id != PROGRAM_STREAM_DIRECTORY &&
                stream_id != DSMCC_STREAM &&
                stream_id != E_STREAM)
            {
                PES_scrambling_control = (PES_data[6]>>4)&0x03;
                PES_priority = (PES_data[6]>>3)&0x1;
                data_alignment_indicator = (PES_data[6]>>2)&0x1;
                copyright = (PES_data[6]>>1)&0x1;
                original_or_copy = (PES_data[6])&0x1;
                /*  flags~~~~*/
                PES_header_data_length = (PES_data[8]);
                elementy_stream_position = PES_data +PES_header_data_length + 9;
                PES_packet_data_length = elementy_stream_position - PES_data;
            }
        }

}PESInfo;

static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp);

class HLSDownload
{
public:
    char* master_url;
    int stream_num;
    HLSMasterPlaylist* master_playlist;
    HLSMediaPlaylist* media_playlists[25];
    uint8_t* segment_ts_data;
    int segment_ts_len;
public:
    void init(char* input_url);
    void destroy();
    int init_media_playlists();
    int init_media_segments();
    long get_hls_data_from_url(char* url, char** data, size_t *size, int type);
    void vod_download_segment();
    bool get_ts_info();
    void ts_to_es();
    void update_m3u8();
};
