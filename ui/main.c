#include <Elementary.h>
#include "ui/ui.h"

int ui_log_dom = -1;

static Eina_Bool
con(void *d __UNUSED__, int type __UNUSED__, Shotgun_Auth *auth)
{
   contact_list_new();
   shotgun_iq_roster_get(auth);
   shotgun_presence_set(auth, SHOTGUN_USER_STATUS_CHAT, "testing SHOTGUN!");
   return ECORE_CALLBACK_RENEW;
}

int
main(int argc, char *argv[])
{
   char *pass;
   Shotgun_Auth *auth;
   char *getpass_x(const char *prompt);

   if (argc != 3)
     {
        fprintf(stderr, "Usage: %s [username] [domain]\n", argv[0]);
        return 1;
     }

   eina_init();
   shotgun_init();
   elm_init(argc, argv);
   
   ui_log_dom = eina_log_domain_register("shotgun_ui", EINA_COLOR_LIGHTRED);
   eina_log_domain_level_set("shotgun_ui", EINA_LOG_LEVEL_DBG);
   eina_log_domain_level_set("shotgun", EINA_LOG_LEVEL_DBG);
   //eina_log_domain_level_set("ecore_con", EINA_LOG_LEVEL_DBG);
   ecore_event_handler_add(SHOTGUN_EVENT_CONNECT, (Ecore_Event_Handler_Cb)con, NULL);

   auth = shotgun_new(argv[1], argv[2]);
   pass = getpass_x("Password: ");
   if (!pass)
     {
        ERR("No password entered!");
        return 1;
     }
   shotgun_password_set(auth, pass);
   shotgun_gchat_connect(auth);
   ecore_main_loop_begin();

   elm_shutdown();

   return 0;
}
