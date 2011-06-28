#include <Elementary.h>
#include "../shotgun_private.h"

#include "ui.h"

typedef struct
{
   Evas_Object *win;
   Evas_Object *list;

   Eina_Hash *users;

   struct {
        Ecore_Event_Handler *iq;
        Ecore_Event_Handler *presence;
        Ecore_Event_Handler *message;
   } event_handlers;
} Contact_List;

typedef struct
{
   Shotgun_User base;
   Shotgun_User_Status status;
   char *description;
   Elm_Genlist_Item *list_item;
   Evas_Object *chat_window;
   Evas_Object *chat_buffer;
   Evas_Object *status_line;
} Contact;

static void
_contact_free(Contact *c)
{
   eina_stringshare_del(c->base.jid);
   eina_stringshare_del(c->base.name);
   free(c->description);
   if (c->list_item)
     elm_genlist_item_del(c->list_item);
   if (c->chat_window)
     evas_object_del(c->chat_window);
   free(c);
}

static char *
_it_label_get(void *data, Evas_Object *obj __UNUSED__, const char *part)
{
   Contact *user = data;

   if (!strcmp(part, "elm.text"))
     {
        const char *ret = NULL;
        if (user->base.name)
          ret = user->base.name;
        else
          ret = user->base.jid;
        return strdup(ret);
     }
   else if (!strcmp(part, "elm.text.sub"))
     {
        char *buf;
        const char *status;
        int len = 0;

        switch(user->status)
          {
           case SHOTGUN_USER_STATUS_NORMAL:
              status = "Normal";
              break;
           case SHOTGUN_USER_STATUS_AWAY:
              status = "Away";
              break;
           case SHOTGUN_USER_STATUS_CHAT:
              status = "Chat";
              break;
           case SHOTGUN_USER_STATUS_DND:
              status = "Busy";
           case SHOTGUN_USER_STATUS_XA:
              status = "Very Away";
              break;
           case SHOTGUN_USER_STATUS_NONE:
              status = "Offline?";
              break;
           default:
              status = "What the fuck aren't we handling?";
          }

        if (!user->description)
          return strdup(status);
        len = strlen(status) + strlen(user->description) + 2;
        buf = malloc(len);
        snprintf(buf, len, "%s: %s", status, user->description);
        return buf;
     }

   return NULL;
}

static Evas_Object *
_it_icon_get(void *data __UNUSED__, Evas_Object *obj __UNUSED__, const char *part __UNUSED__)
{
   return NULL;
}

static Eina_Bool
_it_state_get(void *data __UNUSED__, Evas_Object *obj __UNUSED__, const char *part __UNUSED__)
{
   return EINA_FALSE;
}

static void
_it_del(void *data, Evas_Object *obj __UNUSED__)
{
   Contact *c = data;
   c->list_item = NULL;
}

static void
_do_something_with_user(Contact_List *cl, Shotgun_User *user)
{
   Contact *c;
   static Elm_Genlist_Item_Class it = {
        .item_style = "double_label",
        .func = {
             .label_get = _it_label_get,
             .icon_get = _it_icon_get,
             .state_get = _it_state_get,
             .del = _it_del
        }
   };

   if (eina_hash_find(cl->users, user->jid))
     return;

   c = calloc(1, sizeof(Contact));
   c->base.jid = user->jid;
   c->base.name = user->name;
   c->base.subscription = user->subscription;
   c->base.account = user->account;
   if (c->base.subscription > SHOTGUN_USER_SUBSCRIPTION_NONE)
     c->list_item = elm_genlist_item_append(cl->list, &it, c, NULL,
                                            ELM_GENLIST_ITEM_NONE, NULL, NULL);

   eina_hash_direct_add(cl->users, c->base.jid, c);
}

static void
_chat_message_insert(Contact *c, const char *from, const char *msg)
{
   int len;
   char timebuf[11];
   char *buf, *s;
   Evas_Object *e = c->chat_buffer;

   strftime(timebuf, sizeof(timebuf), "[%H:%M:%S]",
            localtime((time_t[]){ time(NULL) }));

   s = elm_entry_utf8_to_markup(msg);
   len = strlen(timebuf) + strlen(from) + strlen(s) + 20;
   buf = alloca(len);
   snprintf(buf, len, "%s <b>%s:</b> %s<br>", timebuf, from, s);
   free(s);

   elm_entry_entry_append(e, buf);
}

static void
_chat_window_send_cb(void *data, Evas_Object *obj, void *ev __UNUSED__)
{
   Contact *c = data;
   char *s;

   s = elm_entry_markup_to_utf8(elm_entry_entry_get(obj));

   shotgun_message_send(c->base.account, c->base.jid, s);
   _chat_message_insert(c, "me", s);
   elm_entry_entry_set(obj, "");

   free(s);
}

static void
_chat_window_free_cb(void *data, Evas *e __UNUSED__, Evas_Object *obj __UNUSED__, void *ev __UNUSED__)
{
   Contact *c = data;
   c->chat_window = NULL;
   c->chat_buffer = NULL;
   c->status_line = NULL;
}

static void
_chat_window_close_cb(void *data, Evas_Object *obj __UNUSED__, void *ev __UNUSED__)
{
   evas_object_del((Evas_Object *)data);
}

static void
_chat_window_open(Contact *c)
{
   Evas_Object *parent_win, *win, *bg, *box, *convo, *entry;
   Evas_Object *topbox, *frame, *status, *close, *icon;

   parent_win = elm_object_top_widget_get(
      elm_genlist_item_genlist_get(c->list_item));

   win = elm_win_add(parent_win, "chat-window", ELM_WIN_BASIC);
   elm_win_autodel_set(win, 1);
   evas_object_resize(win, 300, 320);
   evas_object_show(win);

   bg = elm_bg_add(win);
   evas_object_size_hint_weight_set(bg, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   elm_win_resize_object_add(win, bg);
   evas_object_show(bg);

   box = elm_box_add(win);
   evas_object_size_hint_weight_set(box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   elm_win_resize_object_add(win, box);
   evas_object_show(box);

   frame = elm_frame_add(win);
   elm_frame_label_set(frame, c->base.name ? : c->base.jid);
   evas_object_size_hint_weight_set(frame, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(frame, EVAS_HINT_FILL, 0);
   elm_box_pack_end(box, frame);
   evas_object_show(frame);

   topbox = elm_box_add(win);
   elm_box_horizontal_set(topbox, 1);
   evas_object_size_hint_weight_set(topbox, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   elm_frame_content_set(frame, topbox);
   evas_object_show(topbox);

   status = elm_entry_add(win);
   elm_entry_single_line_set(status, 1);
   elm_entry_scrollable_set(status, 1);
   evas_object_size_hint_weight_set(status, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(status, EVAS_HINT_FILL, 0);
   elm_box_pack_end(topbox, status);
   evas_object_show(status);

   close = elm_button_add(win);
   elm_box_pack_end(topbox, close);
   evas_object_show(close);
   icon = elm_icon_add(win);
   elm_icon_standard_set(icon, "close");
   elm_button_icon_set(close, icon);

   convo = elm_entry_add(win);
   elm_entry_editable_set(convo, 0);
   elm_entry_single_line_set(convo, 0);
   elm_entry_scrollable_set(convo, 1);
   elm_entry_line_wrap_set(convo, ELM_WRAP_MIXED);
   evas_object_size_hint_weight_set(convo, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(convo, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(box, convo);
   evas_object_show(convo);

   entry = elm_entry_add(win);
   elm_entry_single_line_set(entry, 1);
   elm_entry_scrollable_set(entry, 1);
   evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, 0);
   elm_box_pack_end(box, entry);
   evas_object_show(entry);

   evas_object_smart_callback_add(entry, "activated", _chat_window_send_cb, c);
   evas_object_event_callback_add(win, EVAS_CALLBACK_FREE, _chat_window_free_cb,
                                  c);
   evas_object_smart_callback_add(close, "clicked", _chat_window_close_cb, win);

   c->chat_window = win;
   c->chat_buffer = convo;
   c->status_line = status;
}

static Eina_Bool
_event_iq_cb(void *data, int type __UNUSED__, void *event)
{
   Contact_List *cl = data;
   Shotgun_Event_Iq *ev = event;

   INF("EVENT_IQ %d: %p", ev->type, ev->ev);
   switch(ev->type)
     {
      case SHOTGUN_IQ_EVENT_TYPE_ROSTER:
        {
           Shotgun_User *user;
           Eina_List *l;
           l = (Eina_List *)ev->ev;
           ev->ev = NULL;
           EINA_LIST_FREE(l, user)
             {
                _do_something_with_user(cl, user);
                free(user);
             }
           break;
        }
      default:
        INF("WTF!\n");
     }
   return EINA_TRUE;
}

static Eina_Bool
_event_presence_cb(void *data, int type __UNUSED__, void *event)
{
   Contact_List *cl = data;
   Contact *c;
   char *jid, *p;
   Shotgun_Event_Presence *ev = event;

   jid = strdup(ev->jid);
   p = strchr(jid, '/');
   if(p) *p = 0;
   c = eina_hash_find(cl->users, jid);
   if (!c) return EINA_TRUE;
   free(jid);

   c->status = ev->status;

   free(c->description);
   c->description = ev->description;
   ev->description = NULL;

   if (c->status_line)
     elm_entry_entry_set(c->status_line, c->description);

   return EINA_TRUE;
}

static Eina_Bool
_event_message_cb(void *data, int type __UNUSED__, void *event)
{
   Shotgun_Event_Message *msg = event;
   Contact_List *cl = data;
   Contact *c;
   char *jid, *p;
   const char *from;

   jid = strdup(msg->jid);
   p = strchr(jid, '/');
   *p = 0;
   c = eina_hash_find(cl->users, jid);
   if (!c) return EINA_TRUE;
   free(jid);

   if (!c->chat_window)
     _chat_window_open(c);

   from = c->base.name ? : c->base.jid;
   _chat_message_insert(c, from, msg->msg);

   return EINA_TRUE;
}

static void
_contact_dbl_click_cb(void *data __UNUSED__, Evas_Object *obj __UNUSED__, void *ev)
{
   Elm_Genlist_Item *it = ev;
   Contact *c;

   c = elm_genlist_item_data_get(it);
   if (c->chat_window)
     {
        elm_win_raise(c->chat_window);
        return;
     }

   _chat_window_open(c);
}

static void
_close_btn_cb(void *data, Evas_Object *obj __UNUSED__, void *ev __UNUSED__)
{
   evas_object_del((Evas_Object *)data);
}

static void
_contact_list_free_cb(void *data, Evas *e __UNUSED__, Evas_Object *obj __UNUSED__, void *ev __UNUSED__)
{
   Contact_List *cl = data;

   ecore_event_handler_del(cl->event_handlers.iq);
   ecore_event_handler_del(cl->event_handlers.presence);
   ecore_event_handler_del(cl->event_handlers.message);

   eina_hash_free(cl->users);

   free(cl);
}

void
contact_list_new(int argc, char **argv)
{
   Evas_Object *win, *bg, *box, *list, *btn;
   Contact_List *cldata;

   elm_init(argc, argv);

   elm_policy_set(ELM_POLICY_QUIT, ELM_POLICY_QUIT_LAST_WINDOW_CLOSED);

   cldata = calloc(1, sizeof(Contact_List));

   win = elm_win_add(NULL, "Shotgun - Contacts", ELM_WIN_BASIC);
   elm_win_autodel_set(win, 1);

   bg = elm_bg_add(win);
   evas_object_size_hint_weight_set(bg, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   elm_win_resize_object_add(win, bg);
   evas_object_show(bg);

   box = elm_box_add(win);
   evas_object_size_hint_weight_set(box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   elm_win_resize_object_add(win, box);
   evas_object_show(box);

   list = elm_genlist_add(win);
   evas_object_size_hint_weight_set(list, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(list, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(box, list);
   evas_object_show(list);

   btn = elm_button_add(win);
   elm_button_label_set(btn, "My wm has no close button");
   elm_box_pack_end(box, btn);
   evas_object_show(btn);

   evas_object_smart_callback_add(list, "clicked,double",
                                  _contact_dbl_click_cb, cldata);
   evas_object_smart_callback_add(btn, "clicked", _close_btn_cb, win);
   evas_object_event_callback_add(win, EVAS_CALLBACK_FREE,
                                  _contact_list_free_cb, cldata);

   cldata->win = win;
   cldata->list = list;

   cldata->users = eina_hash_string_superfast_new((Eina_Free_Cb)_contact_free);

   cldata->event_handlers.iq = ecore_event_handler_add(SHOTGUN_EVENT_IQ,
                                                       _event_iq_cb, cldata);
   cldata->event_handlers.presence =
      ecore_event_handler_add(SHOTGUN_EVENT_PRESENCE, _event_presence_cb,
                              cldata);
   cldata->event_handlers.message =
      ecore_event_handler_add(SHOTGUN_EVENT_MESSAGE, _event_message_cb,
                              cldata);

   evas_object_data_set(win, "contact-list", cldata);

   evas_object_resize(win, 300, 700);
   evas_object_show(win);
}
