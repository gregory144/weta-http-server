#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <inttypes.h>

#include <uv.h>

#include "server.h"
#include "plugin.h"

#include "log.h"
#include "util.h"
#include "http/http.h"
#include "http/h2/h2.h"
#include "http/request.h"

static void debug_plugin_start(plugin_t * plugin)
{
  log_append(plugin->log, LOG_INFO, "Debug plugin started");
}

static void debug_plugin_stop(plugin_t * plugin)
{
  log_append(plugin->log, LOG_INFO, "Debug plugin stopped");
}

static bool debug_plugin_request_handler(plugin_t * plugin, client_t * client, http_request_t * request,
    http_response_t * response)
{
  UNUSED(client);

  if (log_level_enabled(plugin->log, LOG_DEBUG)) {
    log_append(plugin->log, LOG_DEBUG, "Method: '%s'", http_request_method(request));
    log_append(plugin->log, LOG_DEBUG, "Scheme: '%s'", http_request_scheme(request));
    log_append(plugin->log, LOG_DEBUG, "Host: '%s'", http_request_host(request));
    log_append(plugin->log, LOG_DEBUG, "Port: %d", http_request_port(request));
    log_append(plugin->log, LOG_DEBUG, "Path: '%s'", http_request_path(request));
    log_append(plugin->log, LOG_DEBUG, "Query: '%s'", http_request_query_string(request));

    log_append(plugin->log, LOG_DEBUG, "Got headers:");
    header_list_iter_t iter;
    header_list_iterator_init(&iter, request->headers);

    while (header_list_iterate(&iter)) {
      header_field_t * field = iter.field;
      log_append(plugin->log, LOG_DEBUG, "'%s' (%ld): '%s' (%ld)",
          field->name, field->name_length, field->value, field->value_length);
    }

    log_append(plugin->log, LOG_DEBUG, "Got parameters:");
    multimap_iter_t mm_iter;
    multimap_iterator_init(&mm_iter, request->params);

    while (multimap_iterate(&mm_iter)) {
      log_append(plugin->log, LOG_DEBUG, "'%s' (%ld): '%s' (%ld)",
          mm_iter.key, strlen(mm_iter.key), mm_iter.value, strlen(mm_iter.value));
    }
  }

  char * method = http_request_method(request);

  if (strncmp(method, "POST", 4) == 0) {

    http_response_status_set(response, 200);

    char * content_length = http_request_header_get(request, "content-length");

    if (content_length) {
      http_response_header_add(response, "content-length", content_length);
    }

    http_response_header_add(response, "server", PACKAGE_STRING);
    size_t date_buf_length = RFC1123_TIME_LEN + 1;
    char date_buf[date_buf_length];
    char * date = current_date_rfc1123(date_buf, date_buf_length);

    if (date) {
      http_response_header_add(response, "date", date);
    }

    http_response_write(response, NULL, 0, false);

    return true;
  }

  char * resp_text;

  char * resp_len_s = http_request_param_get(request, "resp_len");
  long long resp_len = 0;

  if (resp_len_s) {
    resp_len = strtoll(resp_len_s, NULL, 10);
  }

  if (resp_len > 0) {
    resp_text = malloc(resp_len + 1);
    memset(resp_text, 'a', resp_len);
    resp_text[resp_len - 1] = '\n';
    resp_text[resp_len] = '\0';
  } else {
    multimap_values_t * messages = http_request_param_get_values(request, "msg");

    if (!messages) {
      char * client_user_agent = http_request_header_get(request, "user-agent");

      if (!client_user_agent) {
        client_user_agent = "Unknown";
      }

      size_t resp_length = 100 + strlen(client_user_agent);
      char user_agent_message[resp_length + 1];
      snprintf(user_agent_message, resp_length, "Hello %s\n", client_user_agent);
      resp_text = strdup(user_agent_message);
    } else {
      // Append all messages.
      // First, count the size
      size_t messages_length = 0;
      multimap_values_t * current = messages;

      while (current) {
        messages_length += strlen(current->value) + 1;
        current = current->next;
      }

      resp_text = malloc(sizeof(char) * messages_length + 1);
      current = messages;
      size_t resp_text_index = 0;

      while (current) {
        size_t current_length = strlen(current->value);
        memcpy(resp_text + resp_text_index, current->value, current_length);
        resp_text_index += current_length;
        resp_text[resp_text_index++] = '\n';
        current = current->next;
      }

      resp_text[resp_text_index] = '\0';
    }
  }

  http_response_status_set(response, 200);

  size_t content_length = strlen(resp_text);

  char content_length_s[256];
  snprintf(content_length_s, 255, "%ld", content_length);
  http_response_header_add(response, "content-length", content_length_s);

  http_response_header_add(response, "server", PACKAGE_STRING);

  size_t date_buf_length = RFC1123_TIME_LEN + 1;
  char date_buf[date_buf_length];
  char * date = current_date_rfc1123(date_buf, date_buf_length);

  if (date) {
    http_response_header_add(response, "date", date);
  }

  http_request_t * pushed_request = http_push_init(request);

  if (pushed_request) {
    http_request_header_add(pushed_request, ":method", "GET");
    http_request_header_add(pushed_request, ":scheme", "http");
    http_request_header_add(pushed_request, ":authority", "localhost:7000");
    http_request_header_add(pushed_request, ":path", "/pushed_resource.txt");

    if (http_push_promise(pushed_request)) {

      http_response_t * pushed_response = http_push_response_get(pushed_request);
      http_response_status_set(pushed_response, 200);

      char push_text[256];
      snprintf(push_text, 255, "Pushed Response at %s\n", date);

      size_t push_content_length = strlen(push_text);

      char push_content_length_s[256];
      snprintf(push_content_length_s, 255, "%ld", push_content_length);
      http_response_header_add(pushed_response, "content-length", push_content_length_s);

      http_response_header_add(pushed_response, "server", PACKAGE_STRING);

      if (date) {
        http_response_header_add(pushed_response, "date", date);
      }

      http_response_write(pushed_response, (uint8_t *) strdup(push_text), push_content_length, true);
    }

  }

  http_response_write(response, (uint8_t *) resp_text, content_length, true);

  return true;
}

static bool debug_plugin_data_handler(plugin_t * plugin, client_t * client, http_request_t * request,
                                       http_response_t * response,
                                       uint8_t * buf, size_t length, bool last, bool free_buf)
{
  UNUSED(plugin);
  UNUSED(client);
  UNUSED(request);

  log_append(plugin->log, LOG_TRACE, "Received %ld bytes of data from client (last? %s)",
      length, last ? "yes" : "no");

  uint8_t * out = malloc(sizeof(uint8_t) * length);
  // convert all bytes to lowercase
  size_t i;

  for (i = 0; i < length; i++) {
    out[i] = *(buf + i) | 0x20;
  }

  http_response_write_data(response, out, length, last);

  if (free_buf) {
    free(buf);
  }

  return true;

}


char * frame_type_to_string(enum frame_type_e t)
{
  switch (t) {
    case FRAME_TYPE_DATA:
      return "DATA";
    case FRAME_TYPE_HEADERS:
      return "HEADERS";
    case FRAME_TYPE_PRIORITY:
      return "PRIORITY";
    case FRAME_TYPE_RST_STREAM:
      return "RST_STREAM";
    case FRAME_TYPE_SETTINGS:
      return "SETTINGS";
    case FRAME_TYPE_PUSH_PROMISE:
      return "PUSH_PROMISE";
    case FRAME_TYPE_PING:
      return "PING";
    case FRAME_TYPE_GOAWAY:
      return "GOAWAY";
    case FRAME_TYPE_WINDOW_UPDATE:
      return "WINDOW_UPDATE";
    case FRAME_TYPE_CONTINUATION:
      return "CONTINUATION";
  }
  return "UNKNOWN";

}

static void debug_plugin_preprocess_incoming_frame(plugin_t * plugin, client_t * client,
    h2_frame_t * frame)
{
  log_append(plugin->log, LOG_INFO, "RECEIVED FRAME %s [client: %" PRIu64 ", length: %" PRIu16
      ", stream id: %" PRIu32 "]",
      frame_type_to_string(frame->type), client->id, frame->length, frame->stream_id
  );
}

static bool debug_plugin_handler(plugin_t * plugin, client_t * client, enum plugin_callback_e cb, va_list args)
{
  switch (cb) {
    case HANDLE_REQUEST:
    {
      http_request_t * request = va_arg(args, http_request_t *);
      http_response_t * response = va_arg(args, http_response_t *);
      return debug_plugin_request_handler(plugin, client, request, response);
    }
    case HANDLE_DATA:
    {
      http_request_t * request = va_arg(args, http_request_t *);
      http_response_t * response = va_arg(args, http_response_t *);
      uint8_t * buf = va_arg(args, uint8_t *);
      size_t length = va_arg(args, size_t);
      bool last = (bool) va_arg(args, int);
      bool free_buf = (bool) va_arg(args, int);
      return debug_plugin_data_handler(plugin, client, request, response, buf, length, last, free_buf);
    }
    case PREPROCESS_INCOMING_FRAME:
    {
      h2_frame_t * frame = va_arg(args, h2_frame_t *);
      debug_plugin_preprocess_incoming_frame(plugin, client, frame);
      return false;
    }
    default:
      return false;
  }
}

void plugin_initialize(plugin_t * plugin, server_t * server)
{
  UNUSED(server);

  plugin->handlers->start = debug_plugin_start;
  plugin->handlers->stop = debug_plugin_stop;
  plugin->handlers->handle = debug_plugin_handler;
}
