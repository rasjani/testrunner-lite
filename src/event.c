/*
 * This file is part of testrunner-lite
 *
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
 *
 * Contact: Sami Lahtinen <ext-sami.t.lahtinen@nokia.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

/* ------------------------------------------------------------------------- */
/* INCLUDE FILES */
#include <proton/message.h>
#include <proton/messenger.h>
#include <json.h>
#include <libxml/list.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include "event.h"
#include "log.h"

/* ------------------------------------------------------------------------- */
/* EXTERNAL DATA STRUCTURES */
/* None */

/* ------------------------------------------------------------------------- */
/* EXTERNAL GLOBAL VARIABLES */
/* None */

/* ------------------------------------------------------------------------- */
/* EXTERNAL FUNCTION PROTOTYPES */
/* None */

/* ------------------------------------------------------------------------- */
/* GLOBAL VARIABLES */
/* None */

/* ------------------------------------------------------------------------- */
/* CONSTANTS */
const char *address = "amqp://~0.0.0.0";

/* ------------------------------------------------------------------------- */
/* MACROS */
/* None */

/* ------------------------------------------------------------------------- */
/* LOCAL GLOBAL VARIABLES */
static pn_messenger_t  *messenger = NULL;

/* ------------------------------------------------------------------------- */
/* LOCAL CONSTANTS AND MACROS */
/* None */

/* ------------------------------------------------------------------------- */
/* MODULE DATA STRUCTURES */
/* None */

/* ------------------------------------------------------------------------- */
/* LOCAL FUNCTION PROTOTYPES */
/* ------------------------------------------------------------------------- */

/* ------------------------------------------------------------------------- */
/* FORWARD DECLARATIONS */
/* None */

/* ------------------------------------------------------------------------- */
/* ==================== LOCAL FUNCTIONS ==================================== */
/* ------------------------------------------------------------------------- */
static int types_match(json_object *obj, xmlChar *type);
static int add_to_json_object(const void *data, const void *user);
static void get_event_params(td_event *event, json_object *object);
static pn_messenger_t *broker_session();
/* ------------------------------------------------------------------------- */
/* ======================== FUNCTIONS ====================================== */
/* ------------------------------------------------------------------------- */

/** 
 * Compares json_type of an object to an event param type string
 * 
 * @param obj Pointer to JSON object
 * @param type Type string
 * 
 * @return 1 if types match, 0 if not
 */
static int types_match(json_object *obj, xmlChar *type)
{
  /* json_object_get_type() cannot be called for a null object  */
  if (!obj) {
    return xmlStrEqual(type, BAD_CAST "null");
  }

  switch(json_object_get_type(obj)) {
  case json_type_boolean:
    if (!xmlStrEqual(type, BAD_CAST "boolean"))
      return 0;
    break;
  case json_type_double:
  case json_type_int:
    if (!xmlStrEqual(type, BAD_CAST "number"))
      return 0;
    break;
  case json_type_object:
    if (!xmlStrEqual(type, BAD_CAST "object"))
      return 0;
    break;
  case json_type_array:
    if (!xmlStrEqual(type, BAD_CAST "array"))
      return 0;
    break;
  case json_type_string:
    if (!xmlStrEqual(type, BAD_CAST "string"))
      return 0;
    break;
  default:
    return 0;
    break;
  }

  return 1;
}

/** 
 * Adds event parameter to JSON object.
 * 
 * @param data Pointer to td_event_param
 * @param user Pointer to json_object
 * 
 * @return 1 always
 */
static int add_to_json_object(const void *data, const void *user)
{
  const td_event_param *param = (const td_event_param *) data;
  json_object *obj = (json_object *) user;
  json_object *new_obj = NULL;

  if (param->type == NULL || param->name == NULL || 
      param->value == NULL) {
    return 1;
  }

  if (xmlStrEqual(param->type, BAD_CAST "string")) {
    /* We don't expect quotes for a string as json_tokener does */
    new_obj = json_object_new_string((const char*)param->value);
  } else {
    new_obj = json_tokener_parse((const char*)param->value);
  }

  if (is_error(new_obj)) {
    /* error occured in parsing */
    LOG_MSG (LOG_WARNING, "Cannot parse event param '%s'",
       (const char*)param->name);
    return 1;
  }

  if (!types_match(new_obj, param->type)) {
    /* actual type is other than specified in  param->type */
    LOG_MSG (LOG_WARNING, "Type conflict for event param '%s'",
       (const char*)param->name);
    /* free json object */
    json_object_put(new_obj);
    return 1;
  }

  json_object_object_add(obj, (const char*)param->name, new_obj);

  return 1;
}

/** 
 * Parses JSON object into event parameters.
 * 
 * @param event Pointer to td_event
 * @param object Pointer to json_object
 */
static void get_event_params(td_event *event, json_object *object)
{
  td_event_param *param = NULL;
  char *key;
  struct json_object *val;
  struct lh_entry *entry;

  for(entry = json_object_get_object(object)->head;
      (entry ? (key = (char*)entry->k,
          val = (struct json_object*)entry->v, entry) : 0);
      entry = entry->next) {
    param = td_event_param_create();

    if (!val) {
      /* null object needs special processing */
      param->type = xmlStrdup(BAD_CAST "null");
      param->name = xmlStrdup(BAD_CAST key);
      param->value = xmlStrdup(BAD_CAST "null");
    } else {
      switch (json_object_get_type(val)) {
      case json_type_boolean:
        param->type = xmlStrdup(BAD_CAST "boolean");
        break;
      case json_type_double:
      case json_type_int:
        param->type = xmlStrdup(BAD_CAST "number");
        break;
      case json_type_object:
        param->type = xmlStrdup(BAD_CAST "object");
        break;
      case json_type_array:
        param->type = xmlStrdup(BAD_CAST "array");
        break;
      case json_type_string:
        param->type = xmlStrdup(BAD_CAST "string");
        break;
      default:
        break;
      }

      param->name = xmlStrdup(BAD_CAST key);
      param->value = xmlStrdup(BAD_CAST 
             json_object_get_string(val));
    }

    xmlListAppend (event->params, param);
  }
}

/** 
 * @return Pointer to a messenger , 0 in failure
 */
static pn_messenger_t *broker_session()
{
  
  while (!messenger) {
    messenger = pn_messenger(NULL);
    pn_messenger_start(messenger);
    int errno = pn_messenger_errno(messenger);
    if(errno) {
      LOG_MSG (LOG_WARNING, "Creating AMQP connection failed");
      LOG_MSG (LOG_WARNING, "%s", pn_code(errno));
      break;
    }
    /*
    pn_messenger_subscribe(messenger, address);
    if(pn_messenger_errno(messenger)) {
      LOG_MSG (LOG_WARNING, "Cannot connect to AMQP broker");
      break;
    }
    */

    break;
  }

  if (!messenger) {
    cleanup_event_system();
  }

  return messenger;
}

/* ------------------------------------------------------------------------- */
/* ======================== FUNCTIONS ====================================== */
/* ------------------------------------------------------------------------- */

/** 
 * Initializes event system.
 * 
 * 
 * @return 1 in success, 0 in failure
 */
int init_event_system()
{
  pn_messenger_t *msgr  = NULL;
  int ret = 0;
  msgr = broker_session();
  if (!msgr) {
    messenger = msgr;
    ret = 1;
  }
  return ret;
}

/** 
 * Closes connection and session to broker.
 * 
 */
void cleanup_event_system()
{
  if (messenger) {
    pn_messenger_stop(messenger);
    pn_messenger_free(messenger);
  }
  messenger = NULL;
}

/** 
 * Waits until an event is received or timeout occurs. 
 * 
 * @param event Event description
 * 
 * @return 1 on success, 0 in failure
 */
int wait_for_event(td_event *event)
{
  char *buf = NULL;
  pn_messenger_t *session = NULL;
// Receiver *receiver = NULL;
// Message *message = NULL;

  size_t len = 0;
  json_object *obj = NULL;
  int ret = 0; /* 0 */

  /*
  session = broker_session();
  if (!session) {
    ret = 0;
    goto out;
  }

  LOG_MSG (LOG_INFO, "Waiting for event: %s", event->resource);

  receiver = session_create_receiver_str(session,
                 (const char*)event->resource);
  if (!receiver) {
    ret = 0;
    goto out;
  }

  message = receiver_fetch(receiver, event->timeout * 1000UL);
  if (!message) {
    ret = 0;
    goto out;
  }

  LOG_MSG (LOG_INFO, "Received event");

  len = message_get_content_size(message);

  if (len > 0) {
    buf = (char *)malloc(len + 1);
    memcpy(buf, message_get_content_ptr(message), len);
    buf[len] = '\0';

    obj = json_tokener_parse(buf);
    if (!is_error(obj)) {
      if (json_object_get_type(obj) == json_type_object) {
        get_event_params(event, obj);
      }
      json_object_put(obj);
    } else {
      LOG_MSG (LOG_WARNING, "Cannot parse received "
         "event content");
    }

    free(buf);
  }

  session_acknowledge(session);

 out:
  if (message) {
    message_destroy(message);
  }

  if (receiver) {
    receiver_destroy(receiver);
  }
  */
  return ret;
}

/** 
 * Sends an event.
 * 
 * @param event Event description
 * 
 * @return 1 on success, 0 in failure
 */
int send_event(td_event *event)
{
  pn_messenger_t *session = NULL;
  pn_message_t *message = NULL;
  pn_data_t *body = NULL;
  json_object *obj = NULL;
  char *addr= "amqp://0.0.0.0";
  const char* token = NULL;
  int ret = 1;

  session = broker_session();
  if (!session) {
    ret = 0;
    goto out;
  }

  obj = json_object_new_object();

  /* add event params to json object */
  xmlListWalk(event->params, add_to_json_object, obj);

  token = json_object_to_json_string(obj);

  message = pn_message();
  if (!message) {
    ret = 0;
    goto out;
  }
  pn_message_set_address(message, addr);
  body = pn_message_body(message);
  pn_data_put_string(body, pn_bytes(strlen(token), token));
  pn_messenger_put(messenger, message);
  pn_messenger_send(messenger, message);

  LOG_MSG (LOG_INFO, "Sending event: %s", event->resource);

 out:
  if (message) {
    pn_message_free(message);
  }
  if (obj) {
    json_object_put(obj);
  }

  return ret;
}

/* ================= OTHER EXPORTED FUNCTIONS ============================== */
/* None */

/* ------------------------------------------------------------------------- */
/* End of file */
