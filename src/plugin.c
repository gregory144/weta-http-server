#include "config.h"

#include <uv.h>

#include "plugin.h"

#include "util.h"
#include "http/http.h"

plugin_t * plugin_init(plugin_t * plugin, log_context_t * log, char * plugin_file,
    struct server_s * server)
{
  bool free_plugin = false;

  if (!plugin) {
    free_plugin = true;
    plugin = malloc(sizeof(plugin_t));
  }

  plugin->log = log;
  plugin->handlers = malloc(sizeof(plugin_handlers_t));

  plugin->data = NULL;
  uv_lib_t * lib = &plugin->lib;

  if (uv_dlopen(plugin_file, lib)) {
    log_append(plugin->log, LOG_FATAL, "Error loading plugin: %s", uv_dlerror(&plugin->lib));

    if (free_plugin) {
      free(plugin);
    }

    return NULL;
  } else {
    log_append(plugin->log, LOG_DEBUG, "Plugin loaded");
  }

  plugin_initializer init;

  if (uv_dlsym(lib, "plugin_initialize", (void **) &init)) {
    log_append(plugin->log, LOG_FATAL, "Error loading plugin initializer: %s", uv_dlerror(lib));

    if (free_plugin) {
      free(plugin);
    }

    return NULL;
  }

  init(plugin, server);

  return plugin;
}

void plugin_request_handler(plugin_t * plugin, struct worker_s * worker, http_request_t * request,
                             http_response_t * response)
{
  plugin->handlers->request(plugin, worker, request, response);
}

void plugin_data_handler(plugin_t * plugin, struct worker_s * worker, http_request_t * request,
                          http_response_t * response,
                          uint8_t * buf, size_t length, bool last, bool free_buf)
{
  plugin->handlers->data(plugin, worker, request, response, buf, length, last, free_buf);
}

void plugin_start(plugin_t * plugin)
{
  plugin->handlers->start(plugin);
}

void plugin_stop(plugin_t * plugin)
{
  plugin->handlers->stop(plugin);

  free(plugin->handlers);
}

