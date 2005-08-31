/*********************************************************
 *
 * This source code is part of the Carnegie Mellon Robot
 * Navigation Toolkit (CARMEN)
 *
 * CARMEN Copyright (c) 2002 Michael Montemerlo, Nicholas
 * Roy, and Sebastian Thrun
 *
 * CARMEN is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public 
 * License as published by the Free Software Foundation; 
 * either version 2 of the License, or (at your option)
 * any later version.
 *
 * CARMEN is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied 
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more 
 * details.
 *
 * You should have received a copy of the GNU General 
 * Public License along with CARMEN; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, 
 * Suite 330, Boston, MA  02111-1307 USA
 *
 ********************************************************/

#include <carmen/carmen_graphics.h>
#include <carmen/param_interface.h>

#define MAX_VARIABLE_LENGTH 2048

typedef struct {
  int m;
  int p;
} carmen_param_id;

static const int DEFAULT_WINDOW_HEIGHT = 500;
static const int TABLE_COLUMN_SPACINGS = 5;

static char *robot_name;

static char **modules;
static int num_modules;
static GtkWidget *statusbar;
static int status_message_max_size = 60;

/* per module */
static char ***variables, ***values;
static int *num_params;
static GtkWidget ***labels, ***entries, ****radio_buttons;
static int **update_param_mask;
static int **num_param_mask;

static char ini_filename[1024];
static GtkWidget *file_window = NULL;
static GtkWidget *file_dialog = NULL;
static GtkWidget *file_dialog_label = NULL;

static void check_status_message(char *checked_message, const char *message) {

  strncpy(checked_message + 1, message, status_message_max_size);
  checked_message[0] = ' ';
  checked_message[status_message_max_size + 1] = '\0';

  for  (; *checked_message != '\0'; checked_message++)
    if (!isprint(*checked_message))
      *checked_message = ' ';
}

guint status_print(const char *message, const char *context) {

  char checked_message[1024];
  guint message_id, context_id;

  if (context == NULL)
    context = "all";

  check_status_message(checked_message, message);

  context_id = gtk_statusbar_get_context_id(GTK_STATUSBAR(statusbar),
					    context);
  message_id = gtk_statusbar_push(GTK_STATUSBAR(statusbar),
				  context_id, checked_message);
  return message_id;
}


static void params_shutdown() {

  int m, p;

  for (m = 0; m < num_modules; m++) {
    for (p = 0; p < num_params[m]; p++) {
      free(variables[m][p]);
      free(values[m][p]);
    }
    free(modules[m]);
    free(variables[m]);
    free(values[m]);
    free(update_param_mask[m]);
    free(num_param_mask[m]);
  }
  free(robot_name);
  free(modules);
  free(variables);
  free(values);
  free(num_params);
  free(update_param_mask);
  free(num_param_mask);
}

static void param_change_handler(char *module, char *variable, char *value) {

  char buf[1024];
  int m, p;

  for (m = 0; m < num_modules; m++) {
    if (!strcmp(modules[m], module)) {
      for (p = 0; p < num_params[m]; p++) {
	if (!strcmp(variables[m][p], variable)) {
	  update_param_mask[m][p] = -1;
	  if (entries[m][p])
	    gtk_entry_set_text(GTK_ENTRY(entries[m][p]), values[m][p]);
	  else if (!carmen_strncasecmp(values[m][p], "on", 256))
	    gtk_toggle_button_set_active
	      (GTK_TOGGLE_BUTTON(radio_buttons[m][p][0]), TRUE);
	  else
	    gtk_toggle_button_set_active
	      (GTK_TOGGLE_BUTTON(radio_buttons[m][p][1]), TRUE);
	  break;
	}
      }
      break;
    }
  }

  sprintf(buf, "Parameter changed: %s_%s = %s", module, variable, value);
  status_print(buf, "param_edit");
}

static int strisnum(char *param) {

  char *end;

  if (strtod(param, &end) == 0.0 && end == param)
    return 0;
  for (; *end != '\0'; end++)
    if (!isspace(*end))
      return 0;

  return 1;
}

static void params_init() {

  int m, p;

  robot_name = carmen_param_get_robot();
  carmen_param_get_modules(&modules, &num_modules);
  variables = (char ***) calloc(num_modules, sizeof(void *));
  carmen_test_alloc(variables);
  values = (char ***) calloc(num_modules, sizeof(void *));
  carmen_test_alloc(values);
  num_params = (int *) calloc(num_modules, sizeof(int));
  carmen_test_alloc(num_params);
  update_param_mask = (int **) calloc(num_modules, sizeof(int *));
  carmen_test_alloc(update_param_mask);
  num_param_mask = (int **) calloc(num_modules, sizeof(int *));
  carmen_test_alloc(num_param_mask);

  for (m = 0; m < num_modules; m++) {
    carmen_param_get_all(modules[m], variables + m, values + m,
			 num_params + m);
    update_param_mask[m] = (int *) calloc(num_params[m], sizeof(int));
    carmen_test_alloc(update_param_mask[m]);
    num_param_mask[m] = (int *) calloc(num_params[m], sizeof(int));
    carmen_test_alloc(num_param_mask[m]);
    for (p = 0; p < num_params[m]; p++) {
      carmen_param_subscribe_string(modules[m], variables[m][p],
				    &values[m][p], param_change_handler);
      if (strisnum(values[m][p]))
	num_param_mask[m][p] = 1;
    }
  }
}

static gint params_save_delayed(gpointer ptr __attribute__ ((unused))) {

  char *return_value;
  int m, p, status = 0;

  status_print("Saving parameters...", "param_edit");

  for (m = 0; m < num_modules; m++) {
    carmen_param_set_module(modules[m]);
    for (p = 0; p < num_params[m]; p++) {
      if (update_param_mask[m][p] > 0) {
	if (num_param_mask[m][p] && !strisnum(values[m][p])) {
	  status = -1;
	  gtk_editable_select_region(GTK_EDITABLE(entries[m][p]), 0, -1);
	}
	else if (carmen_param_set_variable(variables[m][p], values[m][p],
				      &return_value) < 0)
	  status = -1;
	else {
	  status = 1;
	  update_param_mask[m][p] = 0;
	}
      }
    }
  }
  
  if (status == 1)
    status_print("Saving parameters...done", "param_edit");
  else if (status == 0)
    status_print("Saving parameters...nothing to save", "param_edit");
  else
    status_print("Saving parameters...failed", "param_edit");
  
  return FALSE;
}

static void params_save(GtkWidget *w __attribute__ ((unused)),
			GdkEvent *event,
			gpointer pntr __attribute__ ((unused))) {

  if ((event->key.keyval == gdk_keyval_from_name("Enter")) ||
      (event->key.keyval == gdk_keyval_from_name("Return")))
    gtk_idle_add(params_save_delayed, NULL);
}

static int params_save_as_ini(char *filename) {

  FILE *fin, *fout;
  char *line, line_out[MAX_VARIABLE_LENGTH];
  char *mark, *token;
  int token_num;
  char lvalue[255], rvalue[MAX_VARIABLE_LENGTH], comment[MAX_VARIABLE_LENGTH];
  char module[255], variable[255];
  int found_desired_robot = 0;
  int line_length;
  int count;
  int m, p;

  fin = fout = NULL;

  fin = fopen("carmen.ini", "r");
  if (fin == NULL) {
    fin = fopen("../carmen.ini", "r");
    if (fin == NULL)
      fin = fopen("../src/carmen.ini", "r");
  }
  fout = fopen(filename, "w");
  if(fin == NULL || fout == NULL)
    return -1;

  line = (char *) calloc(MAX_VARIABLE_LENGTH, sizeof(char));
  carmen_test_alloc(line);

  count = 0;
  while (!feof(fin)) {
    fgets(line, MAX_VARIABLE_LENGTH, fin);
    strncpy(line_out, line, MAX_VARIABLE_LENGTH - 1);
    count++;
    if (feof(fin))
      break;
    mark = strchr(line, '#');
    if (mark != NULL) {
      strcpy(comment, mark);
      mark[0] = '\0';
    }
    else
      comment[0] = '\0';
    mark = strchr(line, '\n');
    if (mark != NULL)
      mark[0] = '\0';
      
    line_length = strlen(line) - 1;
    while (line_length >= 0 && 
	   (line[line_length] == ' ' || line[line_length] == '\t' )) {
      line[line_length--] = '\0';
    }
    line_length++;
      
    if (line_length == 0) {
      fprintf(fout, "%s", line_out);
      continue;
    }
      
    /* Skip over initial blank space */
      
    mark = line + strspn(line, " \t");
    if (strlen(mark) == 0) {
      fprintf(fout, "%s", line_out);
      continue;
    }
      
    strcpy(lvalue, "");
    strcpy(rvalue, "");
    token_num = 0;
      
    /* tokenize line */
    token = mark;
    mark = strpbrk(mark, " \t");

    if (token != NULL && strlen(token) > 0) {
      if (mark) {
	mark[0] = '\0';
	mark++;
	mark += strspn(mark, " \t");
      }
      strncpy(lvalue, token, 255);
      token_num++;
      if (mark != NULL && strlen(mark) > 0) {
	  strncpy(rvalue, mark, MAX_VARIABLE_LENGTH);
	  token_num++;
      }
    } /* End of if (token != NULL && strlen(token)) */

    if (token_num > 0) {
      if (lvalue[0] == '[') {
	if (carmen_strncasecmp(lvalue+1, robot_name, strlen(robot_name)) == 0)
	  found_desired_robot = 1;
	else if (lvalue[1] == '*')
	  found_desired_robot = 1;
	else
	  found_desired_robot = 0;
	fprintf(fout, "%s", line_out);
      }
      else if(token_num == 2 && found_desired_robot == 1) {
	fprintf(fout, "%s\t\t", lvalue);
	sscanf(lvalue, "%[^_]_%s", module, variable);
	for (m = 0; m < num_modules; m++) {
	  if (!strcmp(modules[m], module)) {
	    for (p = 0; p < num_params[m]; p++) {
	      if (!strcmp(variables[m][p], variable)) {
		fprintf(fout, "%s", values[m][p]);
		break;
	      }
	    }
	    if (p == num_params[m])
	      fputs(rvalue, fout);
	    break;
	  }
	}
	if (m == num_modules)
	  fputs(rvalue, fout);
	if (strlen(comment) > 0)
	  fprintf(fout, "\t%s\n", comment);
	else
	  fputs("\n", fout);
      }
      else
	fprintf(fout, "%s", line_out);
    } /* End of if (token_num > 0) */
    else
      fprintf(fout, "%s", line_out);
  } /* End of while (!feof(fin)) */
  
  fclose(fin);
  fclose(fout);

  return 0;
}

static void file_dialog_destroy(GtkWidget *w __attribute__ ((unused)),
				gpointer p __attribute__ ((unused))) {
  file_dialog = NULL;
}

static void file_dialog_ok() {

  char buf[1024];

  if (params_save_as_ini(ini_filename) < 0)
    sprintf(buf, "Saving %s...failed", ini_filename);
  else
    sprintf(buf, "Saving %s...done", ini_filename);
  status_print(buf, NULL);

  if (file_dialog)
    gtk_widget_hide(file_dialog);
  if (file_window)
    gtk_widget_hide(file_window);
}

static void file_dialog_cancel() {

  gtk_widget_hide(file_dialog);
}

static void file_dialog_init() {

  GtkWidget *ok_button, *cancel_button;

  file_dialog = gtk_dialog_new();
  gtk_window_set_modal(GTK_WINDOW(file_dialog), TRUE);
  file_dialog_label = gtk_label_new("");
  ok_button = gtk_button_new_with_label(" Ok ");
  cancel_button = gtk_button_new_with_label(" Cancel ");

  gtk_signal_connect(GTK_OBJECT(ok_button), "clicked",
		     GTK_SIGNAL_FUNC(file_dialog_ok), NULL);
  gtk_signal_connect(GTK_OBJECT(cancel_button), "clicked",
		     GTK_SIGNAL_FUNC(file_dialog_cancel), NULL);
  gtk_signal_connect(GTK_OBJECT(file_dialog), "destroy",
		     GTK_SIGNAL_FUNC(file_dialog_destroy), NULL);

  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(file_dialog)->vbox), file_dialog_label,
		     TRUE, TRUE, 10);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(file_dialog)->action_area), ok_button,
		     FALSE, FALSE, 10);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(file_dialog)->action_area),
		     cancel_button, FALSE, FALSE, 10);
}

static void file_dialog_popup() {

  char buf[255];

  if (file_dialog == NULL)
    file_dialog_init();
  sprintf(buf, "  File %s already exists. Overwrite?  ", ini_filename);
  gtk_label_set_text(GTK_LABEL(file_dialog_label), buf);
  gtk_widget_show_all(file_dialog);
}

static void file_window_destroy(GtkWidget *w __attribute__ ((unused)),
				gpointer p __attribute__ ((unused))) {
  file_window = NULL;
}

static void file_ok() {

  strncpy(ini_filename, gtk_file_selection_get_filename
	  (GTK_FILE_SELECTION(file_window)), 1023);
  if (carmen_file_exists(ini_filename))
    file_dialog_popup();
  else
    file_dialog_ok();
}

static void file_cancel() {

  gtk_widget_hide(file_window);
}

static void file_window_init() {

  file_window = gtk_file_selection_new("Save ini");
  gtk_window_set_modal(GTK_WINDOW(file_window), TRUE);
  gtk_file_selection_complete(GTK_FILE_SELECTION(file_window), "*.ini");
  gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(file_window)->ok_button),
		     "clicked", GTK_SIGNAL_FUNC(file_ok), NULL);
  gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(file_window)
				->cancel_button), "clicked",
		     GTK_SIGNAL_FUNC(file_cancel), NULL);
  gtk_signal_connect(GTK_OBJECT(file_window), "destroy",
		     GTK_SIGNAL_FUNC(file_window_destroy), NULL);
}

static void file_window_popup() {

  if (!file_window)
    file_window_init();
  gtk_widget_show_all(file_window);
}

static void gui_shutdown() {

  int m, p;

  for (m = 0; m < num_modules; m++) {
    free(labels[m]);
    free(entries[m]);
    for (p = 0; p < num_params[m]; p++)
      if (radio_buttons[m][p])
	free(radio_buttons[m][p]);
    free(radio_buttons[m]);
  }
  free(labels);
  free(entries);
  free(radio_buttons);
}

static void window_destroy(GtkWidget *w __attribute__ ((unused)),
			   gpointer p __attribute__ ((unused))) {
  gui_shutdown();
  params_shutdown();
  gtk_main_quit();
}

static GtkWidget *menubar_init(GtkWidget *window) {

  GtkItemFactory *item_factory;
  GtkAccelGroup *accel_group;
  GtkWidget *menubar;
  gint nmenu_items;
  GtkItemFactoryEntry menu_items[] = {
    {"/_File", NULL, NULL, 0, "<Branch>"},
    {"/File/_Save ini", "<control>S", file_window_popup, 0, NULL},
    {"/File/", NULL, NULL, 0, "<Separator>"},
    {"/File/_Quit", "<control>Q", window_destroy, 0, NULL},
  };

  nmenu_items = sizeof(menu_items) / sizeof(menu_items[0]);

  accel_group = gtk_accel_group_new();
  item_factory = gtk_item_factory_new(GTK_TYPE_MENU_BAR, "<main>", 
				      accel_group);
  gtk_item_factory_create_items(item_factory, nmenu_items, menu_items, NULL);
  gtk_window_add_accel_group(GTK_WINDOW(window), accel_group);

  menubar = gtk_item_factory_get_widget(item_factory, "<main>");

  return menubar;
}

static void entry_changed(GtkWidget *w, gpointer data) 
{
  int m, p;
  char *value;

  carmen_param_id *id_ptr = (carmen_param_id *)data;

  m = id_ptr->m;
  p = id_ptr->p;

  value = gtk_editable_get_chars(GTK_EDITABLE(w), 0, -1);
  free(values[m][p]);
  values[m][p] = calloc(strlen(value) + 1, sizeof(char));
  carmen_test_alloc(values[m][p]);
  strcpy(values[m][p], value);
  g_free(value);
  update_param_mask[m][p]++;
  if (update_param_mask[m][p] > 0)
    gtk_label_set_pattern(GTK_LABEL(labels[m][p]),
			  "___________________________________________");
  else
    gtk_label_set_pattern(GTK_LABEL(labels[m][p]), "");
}

static void radio_button_toggled(GtkWidget *radio_button, gpointer data) 
{
  int m, p;
  carmen_param_id *id_ptr = (carmen_param_id *)data;

  m = id_ptr->m;
  p = id_ptr->p;

  if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radio_button)))
    return;

  if (update_param_mask[m][p] < 0) {
    update_param_mask[m][p] = 0;
    return;
  }

  update_param_mask[m][p] = 1;

  values[m][p] = realloc(values[m][p], (strlen("off") + 1) * sizeof(char));
  carmen_test_alloc(values[m][p]);
  if ((radio_button == radio_buttons[m][p][0]) &&
      carmen_strncasecmp(values[m][p], "on", 2))
    strcpy(values[m][p], "on");
  else if ((radio_button == radio_buttons[m][p][1]) &&
	   carmen_strncasecmp(values[m][p], "off", 3))
    strcpy(values[m][p], "off");
  else
    update_param_mask[m][p] = 0;

  gtk_idle_add(params_save_delayed, NULL);  
}

static GtkWidget *notebook_init() {

  GtkWidget *notebook, *scrolled_window, *vbox, *hbox, *hbox2, *table, *tab;
  int m, p;
  carmen_param_id *param_id;

  notebook = gtk_notebook_new();
  gtk_notebook_set_scrollable(GTK_NOTEBOOK(notebook), TRUE);
  gtk_notebook_set_tab_pos(GTK_NOTEBOOK(notebook), GTK_POS_LEFT);
  labels = (GtkWidget ***) calloc(num_modules, sizeof(void *));
  carmen_test_alloc(labels);
  entries = (GtkWidget ***) calloc(num_modules, sizeof(void *));
  carmen_test_alloc(entries);
  radio_buttons = (GtkWidget ****) calloc(num_modules, sizeof(void *));
  carmen_test_alloc(radio_buttons);

  for (m = 0; m < num_modules; m++) {
    scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
				   GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
    vbox = gtk_vbox_new(FALSE, 0);
    hbox = gtk_hbox_new(FALSE, 0);
    table = gtk_table_new(num_params[m], 2, FALSE);
    gtk_table_set_col_spacings(GTK_TABLE(table), TABLE_COLUMN_SPACINGS);
    tab = gtk_label_new(modules[m]);
    labels[m] = (GtkWidget **) calloc(num_params[m], sizeof(void *));
    carmen_test_alloc(labels[m]);
    entries[m] = (GtkWidget **) calloc(num_params[m], sizeof(void *));
    carmen_test_alloc(entries[m]);
    radio_buttons[m] = (GtkWidget ***) calloc(num_params[m], sizeof(void *));
    carmen_test_alloc(radio_buttons[m]);
    for (p = 0; p < num_params[m]; p++) {
      labels[m][p] = gtk_label_new(variables[m][p]);
      gtk_table_attach_defaults(GTK_TABLE(table), labels[m][p],
				0, 1, p, p + 1);
      if (!carmen_strncasecmp(values[m][p], "on", strlen(values[m][p])) ||
	  !carmen_strncasecmp(values[m][p], "off", strlen(values[m][p]))) {
	radio_buttons[m][p] = (GtkWidget **) calloc(2, sizeof(void *));
	carmen_test_alloc(radio_buttons[m][p]);
	radio_buttons[m][p][0] = gtk_radio_button_new_with_label(NULL, "on");
	radio_buttons[m][p][1] =
	  gtk_radio_button_new_with_label(
	    gtk_radio_button_group(GTK_RADIO_BUTTON(radio_buttons[m][p][0])),
	    "off");
	if (!carmen_strncasecmp(values[m][p], "on", strlen(values[m][p])))
	  gtk_toggle_button_set_active(
            GTK_TOGGLE_BUTTON(radio_buttons[m][p][0]), TRUE);
	else
	  gtk_toggle_button_set_active(
            GTK_TOGGLE_BUTTON(radio_buttons[m][p][1]), TRUE);
	hbox2 = gtk_hbox_new(TRUE, 0);
	gtk_box_pack_start(GTK_BOX(hbox2), radio_buttons[m][p][0],
			   FALSE, FALSE, 10);
	gtk_box_pack_start(GTK_BOX(hbox2), radio_buttons[m][p][1],
			   FALSE, FALSE, 10);
	gtk_table_attach_defaults(GTK_TABLE(table), hbox2, 1, 2, p, p + 1);
	param_id = (carmen_param_id *)calloc(1, sizeof(carmen_param_id));
	carmen_test_alloc(param_id);
	param_id->m = m;
	param_id->p = p;
	gtk_signal_connect(GTK_OBJECT(radio_buttons[m][p][0]), "clicked",
			   GTK_SIGNAL_FUNC(radio_button_toggled),
			   (gpointer) param_id);
	gtk_signal_connect(GTK_OBJECT(radio_buttons[m][p][1]), "clicked",
			   GTK_SIGNAL_FUNC(radio_button_toggled),
			   (gpointer) param_id);
	gtk_signal_connect(GTK_OBJECT(radio_buttons[m][p][0]),
			   "key_press_event", GTK_SIGNAL_FUNC(params_save),
			   NULL);
	gtk_signal_connect(GTK_OBJECT(radio_buttons[m][p][1]),
			   "key_press_event", GTK_SIGNAL_FUNC(params_save),
			   NULL);
      }
      else {
	entries[m][p] = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(entries[m][p]), values[m][p]);
	gtk_table_attach_defaults(GTK_TABLE(table), entries[m][p],
				  1, 2, p, p + 1);
	param_id = (carmen_param_id *)calloc(1, sizeof(carmen_param_id));
	carmen_test_alloc(param_id);
	param_id->m = m;
	param_id->p = p;
	gtk_signal_connect(GTK_OBJECT(entries[m][p]), "changed",
			   GTK_SIGNAL_FUNC(entry_changed),
			   (gpointer) param_id);
	gtk_signal_connect(GTK_OBJECT(entries[m][p]), "key_press_event",
			   GTK_SIGNAL_FUNC(params_save), NULL);
      }
    }
    gtk_box_pack_start(GTK_BOX(hbox), table, TRUE, TRUE, 20);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 20);
    gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolled_window),
					  vbox);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), scrolled_window, tab);
  }

  return notebook;
}

static GtkWidget *statusbar_init() {

  GtkWidget *hbox;

  hbox = gtk_hbox_new(FALSE, 0);
  statusbar = gtk_statusbar_new();
  gtk_box_pack_start(GTK_BOX(hbox), statusbar, TRUE, TRUE, 3);

  return hbox;
}

static void gui_init() {

  GtkWidget *window, *vbox;
  char title[255];

  window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  sprintf(title, "Param Editor:  [%s]", robot_name);
  gtk_window_set_title(GTK_WINDOW(window), title);
  gtk_window_set_default_size(GTK_WINDOW(window), 0, DEFAULT_WINDOW_HEIGHT);
  gtk_signal_connect(GTK_OBJECT(window), "destroy",
		     GTK_SIGNAL_FUNC(window_destroy), NULL);

  vbox = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), menubar_init(window), FALSE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), notebook_init(), TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), statusbar_init(), FALSE, TRUE, 3);

  gtk_container_add(GTK_CONTAINER(window), vbox);

  gtk_widget_show_all(window);
}

static gint updateIPC(gpointer *data __attribute__ ((unused))) {

  sleep_ipc(0.01);
  carmen_graphics_update_ipc_callbacks((GdkInputFunction) updateIPC);

  return 1;
}

int main(int argc, char *argv[]) {

  carmen_initialize_ipc(argv[0]);
  carmen_param_check_version(argv[0]);

  gtk_init(&argc, &argv);
  params_init();
  gui_init();

  carmen_graphics_update_ipc_callbacks((GdkInputFunction) updateIPC);

  gtk_main();

  return 0;
}