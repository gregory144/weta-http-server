#include <stdbool.h>

#ifndef HTTP_HTTP_H
#define HTTP_HTTP_H

/**
 * Frame types
 */
#define FRAME_TYPE_DATA 0
#define FRAME_TYPE_HEADERS 1
#define FRAME_TYPE_PRIORITY 2
#define FRAME_TYPE_RST_STREAM 3
#define FRAME_TYPE_SETTINGS 4
#define FRAME_TYPE_PUSH_PROMISE 5
#define FRAME_TYPE_PING 6
#define FRAME_TYPE_GOAWAY 7
#define FRAME_TYPE_WINDOW_UPDATE 8
#define FRAME_TYPE_CONTINUATION 9

/**
 * Stream states
 */
#define STREAM_STATE_IDLE 0
#define STREAM_STATE_RESERVED_LOCAL 1
#define STREAM_STATE_RESERVED_REMOTE 2
#define STREAM_STATE_OPEN 3
#define STREAM_STATE_HALF_CLOSED_LOCAL 4
#define STREAM_STATE_HALF_CLOSED_REMOTE 5
#define STREAM_STATE_CLOSED 6

/**
 * Connection setting identifiers
 */
#define SETTINGS_HEADER_TABLE_SIZE 1
#define SETTINGS_ENABLE_PUSH 2
#define SETTINGS_MAX_CONCURRENT_STREAMS 4
#define SETTINGS_INITIAL_WINDOW_SIZE 7

#define DEFAULT_HEADER_TABLE_SIZE 4096
#define DEFAULT_ENABLE_PUSH 1
#define DEFAULT_MAX_CONNCURRENT_STREAMS 100
#define DEFAULT_INITIAL_WINDOW_SIZE 65535

/**
 * HTTP errors
 */

/**
 * The associated condition is not as a result of an error. For example, a 
 * GOAWAY might include this code to indicate graceful shutdown of a connection. 
 */
#define HTTP_ERROR_NO_ERROR 0

/**
 * The endpoint detected an unspecific protocol error. This error is for use
 * when a more specific error code is not available.
 */
#define HTTP_ERROR_PROTOCOL_ERROR 1

/**
 * The endpoint encountered an unexpected internal error.
 */
#define HTTP_ERROR_INTERNAL_ERROR 2


/**
 * The endpoint detected that its peer violated the flow control protocol.
 */
#define HTTP_ERROR_FLOW_CONTROL_ERROR 3

/**
 * The endpoint sent a SETTINGS frame, but did not receive a response in a 
 * timely manner. See Settings Synchronization (Section 6.5.3). 
 */
#define HTTP_ERROR_SETTINGS_TIMEOUT 4

/**
 * The endpoint received a frame after a stream was half closed.
 */
#define HTTP_ERROR_STREAM_CLOSED 5

/**
 * The endpoint received a frame that was larger than the maximum size
 * that it supports.
 */
#define HTTP_ERROR_FRAME_SIZE_ERROR 6

/**
 * The endpoint refuses the stream prior to performing any application
 * processing, see Section 8.1.4 for details. 
 */
#define HTTP_ERROR_REFUSED_STREAM 7

/**
 * Used by the endpoint to indicate that the stream is no longer needed.
 */
#define HTTP_ERROR_CANCEL 8

/**
 * The endpoint is unable to maintain the compression context for the
 * connection.
 */
#define HTTP_ERROR_COMPRESSION_ERROR 9

/**
 * The connection established in response to a CONNECT request (Section 8.3)
 * was reset or abnormally closed. 
 */
#define HTTP_ERROR_CONNECT_ERROR 10

/**
 * The endpoint detected that its peer is exhibiting a behavior over a given
 * amount of time that has caused it to refuse to process further frames. 
 */
#define HTTP_ERROR_ENHANCE_YOUR_CALM 11

/**
 * The underlying transport has properties that do not meet the minimum
 * requirements imposed by this document (see Section 9.2) or the endpoint. 
 */
#define HTTP_ERROR_INADEQUATE_SECURITY 12

typedef struct http_frame_payload_s http_frame_payload_t;
struct http_frame_payload_s {

  char* data;

};

#define HTTP_FRAME_FIELDS               \
  /* Length in octets of the frame */   \
  /* 14 bits                       */   \
  uint16_t length;                      \
                                        \
  /* Frame type                    */   \
  /* 8 bits                        */   \
  uint8_t type;                         \
                                        \
  /* Stream identifier             */   \
  /* 31 bits                       */   \
  uint32_t stream_id;


typedef struct http_frame_s http_frame_t;
struct http_frame_s {

  HTTP_FRAME_FIELDS

};

typedef struct http_frame_settings_t {

  HTTP_FRAME_FIELDS

  // is this the response to our own settings
  // frame
  bool ack;

} http_frame_settings_t;

typedef struct http_header_fragment_s http_header_fragment_t;
struct http_header_fragment_s {
  char* buffer;
  size_t length;
  struct http_header_fragment_s* next;
};

typedef struct http_frame_headers_t {

  HTTP_FRAME_FIELDS

  // is this the last frame in the stream?
  bool end_stream;
  // is this the last settings frame for this stream?
  bool end_headers;
  // is the priority provided in the frame payload?
  bool priority;

  size_t header_block_fragment_size;
  char* header_block_fragment;

} http_frame_headers_t;

typedef struct http_stream_s http_stream_t;
struct http_stream_s {

  /**
   * Stream identifier
   */
  uint32_t id;

  /**
   * The current state of the stream, one of:
   *
   * idle
   * reserved (local)
   * reserved (remote)
   * open
   * half closed (local)
   * half closed (remote)
   * closed
   *
   */
  unsigned int state;

  uint32_t priority;

  http_header_fragment_t* header_fragments;

  char* headers;
  size_t headers_length;

};

typedef void (*write_cb)(void* data, char* buf, size_t len);

typedef void (*close_cb)(void* data);

/**
 * Stores state for a client.
 */
typedef struct http_parser_s http_parser_t;
struct http_parser_s {
  void* data;
  write_cb writer;
  close_cb closer;

  /**
   * Parser state
   */
  bool received_connection_header;
  bool received_settings;
  size_t current_stream_id;

  /**
   * what's currently being read
   */
  char* buffer;
  size_t buffer_length;
  size_t buffer_position;

  /**
   * Connection settings
   */
  size_t header_table_size;
  bool enable_push;
  size_t max_concurrent_streams;
  size_t initial_window_size;

  // TOOD - use a better data structure to get a stream
  http_stream_t **streams;
};

http_parser_t* http_parser_init(void* data, write_cb writer, close_cb closer);

void http_parser_free(http_parser_t* parser);

void http_parser_read(http_parser_t* parser, char* buffer, size_t len);

#endif
