#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>

#include "hpack.h"
#include "huffman.h"
#include "util.h"

typedef struct static_entry_t {
  char* name;
  char* value;
} static_entry_t;

static_entry_t static_table[] = {
  { ":authority",                   "" },
  { ":method",                      "GET" },
  { ":method",                      "POST" },
  { ":path",                        "/" },
  { ":path",                        "/index.html" },
  { ":scheme",                      "http" },
  { ":scheme",                      "https" },
  { ":status",                      "200" },
  { ":status",                      "500" },
  { ":status",                      "404" },
  { ":status",                      "403" },
  { ":status",                      "400" },
  { ":status",                      "401" },
  { "accept-charset",               "" },
  { "accept-encoding",              "" },
  { "accept-language",              "" },
  { "accept-ranges",                "" },
  { "accept",                       "" },
  { "access-control-allow-origin",  "" },
  { "age",                          "" },
  { "allow",                        "" },
  { "authorization",                "" },
  { "cache-control",                "" },
  { "content-disposition",          "" },
  { "content-encoding",             "" },
  { "content-language",             "" },
  { "content-length",               "" },
  { "content-location",             "" },
  { "content-range",                "" },
  { "content-type",                 "" },
  { "cookie",                       "" },
  { "date",                         "" },
  { "etag",                         "" },
  { "expect",                       "" },
  { "expires",                      "" },
  { "from",                         "" },
  { "host",                         "" },
  { "if-match",                     "" },
  { "if-modified-since",            "" },
  { "if-none-match",                "" },
  { "if-range",                     "" },
  { "if-unmodified-since",          "" },
  { "last-modified",                "" },
  { "link",                         "" },
  { "location",                     "" },
  { "max-forwards",                 "" },
  { "proxy-authenticate",           "" },
  { "proxy-authorization",          "" },
  { "range",                        "" },
  { "referer",                      "" },
  { "refresh",                      "" },
  { "retry-after",                  "" },
  { "server",                       "" },
  { "set-cookie",                   "" },
  { "strict-transport-security",    "" },
  { "transfer-encoding",            "" },
  { "user-agent",                   "" },
  { "vary",                         "" },
  { "via",                          "" },
  { "www-authenticate",             "" }
};

hpack_decode_quantity_result_t* hpack_decode_quantity(uint8_t* buf,
    size_t length, uint8_t offset) {
  size_t prefix_length = 8 - offset;
  uint8_t limit = pow(2, prefix_length) - 1;
  size_t i = 0;
  if (prefix_length != 0) {
    i = buf[0] & limit;
  }

  size_t index = 1;
  if (i == limit) {
    unsigned int m = 0;
    uint8_t next = buf[index];
    while (index < length) {
      i += ((next & 127) << m);
      m += 7;

      if (next < 128) {
        break;
      }
      
      next = buf[++index];
    }
    index++;
  }

  hpack_decode_quantity_result_t* result = malloc(
      sizeof(hpack_decode_quantity_result_t));
  result->num_bytes = index;
  result->value = i;
  return result;
}

/**
 *
 * From the spec:
 * If I < 2^N - 1, encode I on N bits
 * Else
 *     encode 2^N - 1 on N bits
 *     I = I - (2^N - 1)
 *     While I >= 128
 *          Encode (I % 128 + 128) on 8 bits
 *          I = I / 128
 *     encode (I) on 8 bits
 */
size_t hpack_encode_quantity(uint8_t* buf, size_t offset, size_t i) {
  size_t bytes_written = 0;
  size_t buf_index = offset / 8;
  size_t original_buf_index = buf_index;
  uint8_t byte_offset = offset % 8;
  uint8_t n = 8 - byte_offset;
  uint8_t bitmask = ((1 << byte_offset) - 1) << n;
  uint8_t first_byte = buf[buf_index] & bitmask;
  uint8_t p = (2 << (n - 1)) - 1; // 2^n - 1

  if (i < p) {
    buf[buf_index++] = first_byte | i;
  } else {
    buf[buf_index++] = first_byte | p;
    i -= p;
    while (i >= 128) {
      buf[buf_index++] = (i % 128) + 128;
      i /= 128;
    }
    buf[buf_index++] = i;
  }
  return buf_index - original_buf_index;
}

hpack_context_t* hpack_context_init(size_t header_table_size) {
  hpack_context_t* context = malloc(sizeof(hpack_context_t));

  context->reference_set = malloc(sizeof(hpack_reference_set_t));
  context->reference_set->entries = NULL;

  context->header_table = malloc(sizeof(hpack_header_table_t));
  context->header_table->max_size = header_table_size;
  context->header_table->current_size = 0;
  context->header_table->length = 0;
  context->header_table->num_evicted = 0;
  context->header_table->entries = NULL;

  return context;
}

void hpack_header_table_entry_free(hpack_header_table_entry_t* entry) {
  while (entry) {
    hpack_header_table_entry_t* current = entry;
    entry = entry->next;
    if (!current->from_static_table) {
      free(current->name);
      free(current->value);
    }
    free(current);
  }
}

void hpack_reference_set_entry_free(hpack_reference_set_entry_t* entry) {
  while (entry) {
    hpack_reference_set_entry_t* current = entry;
    entry = entry->next;
    free(current);
  }
}

void hpack_context_free(hpack_context_t* context) {
  if (context->reference_set->entries) {
    hpack_reference_set_entry_free(context->reference_set->entries);
  }
  free(context->reference_set);
  if (context->header_table->entries) {
    hpack_header_table_entry_free(context->header_table->entries);
  }
  free(context->header_table);
  free(context);
}

void hpack_headers_free(hpack_headers_t* headers) {
  while (headers) {
    hpack_headers_t* header = headers;
    headers = headers->next;
    free(header->name);
    free(header->value);
    free(header);
  }
}

size_t hpack_header_table_adjusted_index(hpack_context_t* context, size_t index) {
  size_t length = context->header_table->length;
  size_t num_evicted = context->header_table->num_evicted;
  size_t end = length + num_evicted;
  return end - index + 1;
}

void hpack_reference_set_add(hpack_context_t* context,
    hpack_header_table_entry_t* header) {
  hpack_reference_set_entry_t* refset_entry = malloc(
      sizeof(hpack_reference_set_entry_t));
  refset_entry->added_on_current_request = true;
  refset_entry->entry = header;

  if (context->reference_set) {
    refset_entry->next = context->reference_set->entries;
  }
  context->reference_set->entries = refset_entry;
}

void hpack_reference_set_clear(hpack_context_t* context) {
  hpack_reference_set_entry_free(context->reference_set->entries);
  context->reference_set->entries = NULL;
}

void hpack_reference_set_remove(hpack_context_t* context, size_t index) {
  hpack_reference_set_entry_t* iter = context->reference_set->entries;
  hpack_reference_set_entry_t* prev = NULL;
  size_t target_index = hpack_header_table_adjusted_index(context, index);
  for (; iter; iter = iter->next) {
    if (iter->entry->index == target_index) {
      if (!prev) {
        context->reference_set->entries = iter->next;
      } else {
        prev->next = iter->next;
      }
      free(iter);
    }
    prev = iter;
  }
}

bool hpack_reference_set_contains(hpack_context_t* context, size_t index) {
  size_t target_index = hpack_header_table_adjusted_index(context, index);

  hpack_reference_set_entry_t* iter = context->reference_set->entries;
  for (; iter; iter = iter->next) {
    if (iter->entry->index == target_index) {
      return true;
    }
  }
  return false;
}


void hpack_header_table_evict(hpack_context_t* context) {
  hpack_header_table_t* header_table = context->header_table;

  hpack_header_table_entry_t* trailer = NULL;
  hpack_header_table_entry_t* iter = header_table->entries;
  hpack_header_table_entry_t* last = NULL;

  size_t last_index = header_table->length;
  size_t target_index = hpack_header_table_adjusted_index(context, last_index);

  while (iter) {
    if (iter->index == target_index) {
      last = iter;
      break;
    }
    trailer = iter;
    iter = iter->next;
  }

  if (last) {
    if (trailer) {
      trailer->next = last->next;
    }
    header_table->current_size -= last->size_in_table;
    header_table->length--;

    // remove from reference set as well
    hpack_reference_set_entry_t* refset_iter = context->reference_set->entries;
    hpack_reference_set_entry_t* refset_trailer = NULL;
    hpack_reference_set_entry_t* refset_target = NULL;
    while (refset_iter) {
      if (refset_iter->entry == last) {
        refset_target = refset_iter;
        break;
      }
      refset_trailer = refset_iter;
      refset_iter = refset_iter->next;
    }

    if (refset_target) {
      if (refset_trailer) {
        refset_trailer->next = refset_target->next;
      }
      free(refset_target);
    }

    context->header_table->num_evicted++;
    free(last);
  }
}

void hpack_header_table_adjust_size(hpack_context_t* context, size_t new_size) {
  context->header_table->max_size = new_size;
  hpack_header_table_t* header_table = context->header_table;
  while (header_table->current_size > header_table->max_size) {
    hpack_header_table_evict(context);
  }
}

hpack_headers_t* hpack_emit_header(hpack_headers_t* headers, char* name,
    size_t name_length, char* value, size_t value_length) {

  hpack_headers_t* new_header = malloc(sizeof(hpack_headers_t));
  new_header->name = malloc(sizeof(char) * (name_length + 1));
  strncpy(new_header->name, name, name_length);
  new_header->name[name_length] = '\0';
  new_header->name_length = name_length;
  new_header->value = malloc(sizeof(char) * (value_length + 1));
  strncpy(new_header->value, value, value_length);
  new_header->value_length = value_length;
  new_header->value[value_length] = '\0';
  new_header->next = headers;

  return new_header;
}

hpack_header_table_entry_t* hpack_header_table_add_existing_entry(
    hpack_context_t* context, hpack_header_table_entry_t* header) {

  header->index = context->header_table->length +
    context->header_table->num_evicted + 1;

  hpack_header_table_t* header_table = context->header_table;
  size_t new_header_table_size = header_table->current_size +
    header->size_in_table;
  log_debug("current_size: %ld, new size: %ld, max size: %ld\n", header_table->current_size, new_header_table_size, header_table->max_size);
  if (new_header_table_size > header_table->max_size) {
    // remove from the end of the table
    hpack_header_table_evict(context);
  }

  // make sure it fits in the header table
  if (header->size_in_table <= header_table->max_size) {
    if (context->header_table->entries) {
      context->header_table->entries->prev = header;
    }
    header->next = context->header_table->entries;
    context->header_table->current_size += header->size_in_table;
    context->header_table->entries = header;
    context->header_table->length++;

    log_debug("-------- Updating header table current size to %ld/%ld\n", context->header_table->current_size, context->header_table->max_size);

    // add to reference set
    hpack_reference_set_add(context, header);
  }

  return header;
}

hpack_header_table_entry_t* hpack_header_table_add(hpack_context_t* context,
    char* name, size_t name_length, char* value, size_t value_length) {
  hpack_header_table_entry_t* header = malloc(sizeof(hpack_header_table_entry_t));
  header->from_static_table = false;
  header->name = name;
  header->name_length = name_length;
  header->value = value;
  header->value_length = value_length;

  // add an extra 32 octets - see 
  // http://tools.ietf.org/html/draft-ietf-httpbis-header-compression-05#section-3.3.1
  header->size_in_table = name_length + value_length + 32;

  return hpack_header_table_add_existing_entry(context, header);
}

hpack_header_table_entry_t* hpack_static_table_get(hpack_context_t* context, size_t index) {
  size_t header_table_length = context->header_table->length;
  if (index + 1 > header_table_length) {
    size_t static_table_index = index - header_table_length - 1;
    static_entry_t entry = static_table[static_table_index];
    hpack_header_table_entry_t* header = malloc(sizeof(hpack_header_table_entry_t));
    size_t name_length = strlen(entry.name);
    size_t value_length = strlen(entry.value);
    header->from_static_table = true;
    header->name = entry.name;
    header->name_length = name_length;
    header->value = entry.value;
    header->value_length = value_length;

    // add an extra 32 octets - see 
    // http://tools.ietf.org/html/draft-ietf-httpbis-header-compression-05#section-3.3.1
    header->size_in_table = name_length + value_length + 32;

    // TODO - this will need to be free'd by caller, but the caller won't
    // know whether it can - because we also return non-freeable entries below
    return header;
  }
  return NULL;
}

hpack_header_table_entry_t* hpack_header_table_get(hpack_context_t* context, size_t index) {
  size_t target_index = hpack_header_table_adjusted_index(context, index);
  log_debug("Real index: %ld, target_index: %ld\n", index, target_index);
  if (index + 1 <= context->header_table->length) {
    hpack_header_table_entry_t* iter = context->header_table->entries;
    while (iter) {
      if (iter->index == target_index) {
        return iter;
      }
      iter = iter->next;
    }
  }
  return NULL;
}

string_and_length_t* hpack_decode_string_literal(
    hpack_context_t* context, uint8_t* buf, size_t length,
    size_t* current) {
  bool first_bit = get_bit(buf + (*current), 0); // is it huffman encoded?
  hpack_decode_quantity_result_t* key_name_result = hpack_decode_quantity(buf + (*current), length - (*current), 1);
  *current += key_name_result->num_bytes;
  size_t key_name_length = key_name_result->value;
  free(key_name_result);
  log_debug("Decoding string literal length: %ld\n", key_name_length);
  char* key_name;
  if (first_bit) {
    huffman_result_t* huffman_result = huffman_decode(buf + (*current), key_name_length);
    *current += key_name_length;
    key_name_length = huffman_result->length;
    key_name = malloc(sizeof(char) * (key_name_length + 1));
    strncpy(key_name, huffman_result->value, key_name_length);
    key_name[key_name_length] = '\0';
    free(huffman_result->value);
    free(huffman_result);
  } else {
    key_name = malloc(sizeof(char) * key_name_length);
    memcpy(key_name, buf + (*current), key_name_length);
    *current += key_name_length;
  }
  return string_and_length(key_name, key_name_length);
}

hpack_headers_t* hpack_decode_literal_header(
    hpack_context_t* context, hpack_headers_t* headers, uint8_t* buf,
    size_t length, size_t* current, bool add_to_header_table) {
  hpack_decode_quantity_result_t* index_result = hpack_decode_quantity(buf + (*current), length - (*current), 2);
  size_t header_table_index = index_result->value;
  *current += index_result->num_bytes;
  log_debug("Adding literal header field: %ld, %ld\n", index_result->value, index_result->num_bytes);
  free(index_result);
  char* key_name = NULL;
  size_t key_name_length = 0;
  if (header_table_index == 0) {
    // literal name
    string_and_length_t* sl = hpack_decode_string_literal(context, buf, length, current);
    key_name = sl->value;
    key_name_length = sl->length;
    free(sl);
    log_debug("Literal name: %s, %ld\n", key_name, key_name_length);
  } else {
    // indexed name
    log_debug("getting from header table %ld\n", header_table_index);
    hpack_header_table_entry_t* entry = hpack_header_table_get(context, header_table_index);
    if (!entry) {
      log_debug("getting from static table %ld\n", header_table_index);
      entry = hpack_static_table_get(context, header_table_index);
    }
    key_name = malloc(sizeof(char) * (entry->name_length + 1));
    strncpy(key_name, entry->name, entry->name_length);
    key_name[entry->name_length] = '\0';
    key_name_length = entry->name_length;
    log_debug("Indexed name: %s, %ld\n", key_name, key_name_length);
    free(entry);
  }
  // literal value
  string_and_length_t* sl = hpack_decode_string_literal(context, buf, length, current);
  char* value = sl->value;
  size_t value_length = sl->length;
  free(sl);
  log_debug("Emitting header literal value: %s (%ld), %s (%ld)\n", key_name, key_name_length, value, value_length);

  if (add_to_header_table) {
    hpack_header_table_entry_t* header = hpack_header_table_add(context,
        key_name, key_name_length, value, value_length);
    return hpack_emit_header(headers, header->name, header->name_length,
        header->value, header->value_length);
  } else {
    return hpack_emit_header(headers, key_name, key_name_length,
        value, value_length);
  }
}

hpack_headers_t* hpack_decode_indexed_header(
    hpack_context_t* context, hpack_headers_t* headers, uint8_t* buf,
    size_t length, size_t* current) {
  hpack_decode_quantity_result_t* result = hpack_decode_quantity(buf + (*current), length - (*current), 1);
  *current += result->num_bytes;
  log_debug("Adding indexed header field: %ld\n", result->value);
  if (result->value == 0) {
    log_debug("Empty reference set\n");
    hpack_reference_set_clear(context);
  } else {
    // if the value is in the reference set - remove it from the reference set
    if (hpack_reference_set_contains(context, result->value)) {
      hpack_reference_set_remove(context, result->value);
    } else {
      hpack_header_table_entry_t* entry = hpack_header_table_get(context,
          result->value);
      if (!entry) {
        entry = hpack_static_table_get(context, result->value);
        hpack_header_table_add_existing_entry(context, entry);
      }
      headers = hpack_emit_header(headers, entry->name,
          entry->name_length, entry->value, entry->value_length);
      log_debug("From index: %s: %s\n", entry->name, entry->value);
    }
  }
  free(result);
  return headers;
}

hpack_headers_t* hpack_decode(hpack_context_t* context, uint8_t* buf, size_t length) {

  size_t current = 0;
  hpack_headers_t* headers = NULL;

  log_debug("BEFORE:\n");
  log_debug("Reference Set:\n");
  hpack_reference_set_entry_t* refset_iter2 = context->reference_set->entries;
  while (refset_iter2) {
    hpack_header_table_entry_t* header = refset_iter2->entry;
    log_debug("refset: \"%s\" (%ld): \"%s\" (%ld)\n", header->name,
      header->name_length, header->value, header->value_length);
    refset_iter2 = refset_iter2->next;
  }

  log_debug("Header Table:\n");
  hpack_header_table_entry_t* ht_iter = context->header_table->entries;
  while (ht_iter) {
    hpack_header_table_entry_t* header = ht_iter;
    log_debug("header table: %ld: \"%s\" (%ld): \"%s\" (%ld)\n", header->index, header->name,
      header->name_length, header->value, header->value_length);
    ht_iter = ht_iter->next;
  }

  log_debug("Decompressing headers: %ld, %ld\n", current, length);
  while (current < length) {
    bool first_bit = get_bit(buf + current, 0);
    if (first_bit) {
      // indexed header field (4.2)
      headers = hpack_decode_indexed_header(context, headers, buf, length, &current);
    } else {
      bool second_bit = get_bit(buf + current, 1);
      if (second_bit) {
        // literal header field without indexing (4.3.1)
        headers = hpack_decode_literal_header(context, headers, buf, length, &current, false);
      } else {
        // literal header field with incremental indexing (4.3.2)
        headers = hpack_decode_literal_header(context, headers, buf, length, &current, true);
      }
    }
  }
  // emit reference set headers
  hpack_reference_set_entry_t* refset_iter = context->reference_set->entries;
  while (refset_iter) {
    if (!refset_iter->added_on_current_request) {
      hpack_header_table_entry_t* header = refset_iter->entry;
      headers = hpack_emit_header(headers, header->name,
        header->name_length, header->value, header->value_length);
    }
    refset_iter->added_on_current_request = false;
    refset_iter = refset_iter->next;
  }

  log_debug("AFTER:\n");
  log_debug("Reference Set:\n");
  refset_iter2 = context->reference_set->entries;
  while (refset_iter2) {
    hpack_header_table_entry_t* header = refset_iter2->entry;
    log_debug("refset: \"%s\" (%ld): \"%s\" (%ld)\n", header->name,
      header->name_length, header->value, header->value_length);
    refset_iter2 = refset_iter2->next;
  }

  log_debug("Header Table:\n");
  ht_iter = context->header_table->entries;
  while (ht_iter) {
    hpack_header_table_entry_t* header = ht_iter;
    log_debug("header table: %ld: \"%s\" (%ld): \"%s\" (%ld)\n", header->index, header->name,
      header->name_length, header->value, header->value_length);
    ht_iter = ht_iter->next;
  }

  return headers;
}

hpack_encode_result_t* hpack_encode(hpack_context_t* context, hpack_headers_t* headers) {
  // naive hpack encoding - never add to the header table
  uint8_t* encoded = malloc(4096); // TODO - we need to construct this dynamically
  size_t encoded_index = 0;
  hpack_headers_t* header = headers;
  while (header) {
    log_debug("Encoding Reponse Header: %s (%ld): %s (%ld)\n", header->name, header->name_length, header->value, header->value_length);
    encoded[encoded_index++] = 0x40; // 4.3.1. Literal Header Field without Indexing

    encoded[encoded_index] = 0x80; // set huffman encoded bit
    huffman_result_t* encoded_name = huffman_encode(header->name, header->name_length);
    encoded_index += hpack_encode_quantity(encoded, (encoded_index * 8) + 1, encoded_name->length);
    memcpy(encoded + encoded_index, encoded_name->value, encoded_name->length);
    encoded_index += encoded_name->length;
    free(encoded_name->value);
    free(encoded_name);

    encoded[encoded_index] = 0x80; // set huffman encoded bit
    huffman_result_t* encoded_value = huffman_encode(header->value, header->value_length);
    encoded_index += hpack_encode_quantity(encoded, (encoded_index * 8) + 1, encoded_value->length);
    memcpy(encoded + encoded_index, encoded_value->value, encoded_value->length);
    encoded_index += encoded_value->length;
    free(encoded_value->value);
    free(encoded_value);

    header = header->next;
  }
  hpack_encode_result_t* result = malloc(sizeof(hpack_encode_result_t));
  result->buf = encoded;
  result->buf_length = encoded_index;
  log_debug("Encoded headers into %ld bytes\n", encoded_index);
  return result;
}
