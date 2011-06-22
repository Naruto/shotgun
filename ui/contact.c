#include <Elementary.h>
#include "../shotgun_private.h"

#include "ui.h"

typedef struct
{
   Evas_Object *win;
   Evas_Object *list;

   struct {
        Ecore_Event_Handler *iq;
        Ecore_Event_Handler *presence;
   } event_handlers;
} Contact_List;

static char *
_it_label_get(void *data, Evas_Object *obj __UNUSED__, const char *part __UNUSED__)
{
   Shotgun_User *user = data;
   if (user->name) return strdup(user->name);
   return strdup(user->jid);
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
_it_del(void *data __UNUSED__, Evas_Object *obj __UNUSED__)
{
}

static void
_do_something_with_user(Contact_List *cl, Shotgun_User *user)
{
   Elm_Genlist_Item *i;
   static Elm_Genlist_Item_Class it = {
        .item_style = "default",
        .func = {
             .label_get = _it_label_get,
             .icon_get = _it_icon_get,
             .state_get = _it_state_get,
             .del = _it_del
        }
   };

   i = elm_genlist_item_append(cl->list, &it, user, NULL, ELM_GENLIST_ITEM_NONE,
                               NULL, NULL);
   if (!i)
     INF("Failed ");
   else
     INF("Successfully ");
   INF("added %s(%s) to list\n", user->name, user->jid);
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
           Eina_List *l;
           Shotgun_User *user;
           EINA_LIST_FOREACH((Eina_List *)ev->ev, l, user)
              _do_something_with_user(cl, user);
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
   Shotgun_Event_Presence *ev = event;

   return EINA_TRUE;
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

   evas_object_smart_callback_add(btn, "clicked", _close_btn_cb, win);
   evas_object_event_callback_add(win, EVAS_CALLBACK_FREE,
                                  _contact_list_free_cb, cldata);

   cldata->win = win;
   cldata->list = list;
   cldata->event_handlers.iq = ecore_event_handler_add(SHOTGUN_EVENT_IQ,
                                                       _event_iq_cb, cldata);
   cldata->event_handlers.presence =
      ecore_event_handler_add(SHOTGUN_EVENT_PRESENCE, _event_presence_cb,
                              cldata);

   evas_object_data_set(win, "contact-list", cldata);

   evas_object_resize(win, 300, 700);
   evas_object_show(win);
}
