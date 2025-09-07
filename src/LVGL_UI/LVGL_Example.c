#include "LVGL_Example.h"
#include "secrets.h"
#include "Wireless.h"


/**********************
 *      TYPEDEFS
 **********************/
typedef enum {
    DISP_SMALL,
    DISP_MEDIUM,
    DISP_LARGE,
} disp_size_t;

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void Onboard_create(lv_obj_t * parent);

static void ta_event_cb(lv_event_t * e);
void example1_increase_lvgl_tick(lv_timer_t * t);
/**********************
 *  STATIC VARIABLES
 **********************/
static disp_size_t disp_size;

static lv_obj_t * tv;
lv_style_t style_text_muted;
lv_style_t style_title;
static lv_style_t style_icon;
static lv_style_t style_bullet;

static const lv_font_t * font_large;
static const lv_font_t * font_normal;

static lv_timer_t * auto_step_timer;
// static lv_color_t original_screen_bg_color;

static lv_timer_t * meter2_timer;

lv_obj_t * SD_Size;
lv_obj_t * FlashSize;
lv_obj_t * BAT_Volts;
lv_obj_t * Board_angle;
lv_obj_t * RTC_Time;
lv_obj_t * Wireless_Scan;
lv_obj_t * Backlight_slider;

// Status label at the top and state variables
static lv_obj_t * g_status_label = NULL;
static bool g_heater_on = false;
static bool g_steam_on = false;
static bool g_shot_on = false;
// Dashboard widgets
static lv_obj_t * g_meter_temp = NULL;
static lv_meter_scale_t * g_temp_scale = NULL;
static lv_meter_indicator_t * g_temp_needle = NULL;
static lv_meter_indicator_t * g_temp_set_band = NULL; // +/-5C band

static lv_obj_t * g_meter_press = NULL;
static lv_meter_scale_t * g_press_scale = NULL;
static lv_meter_indicator_t * g_press_needle = NULL;
static lv_meter_indicator_t * g_press_crit = NULL; // 9-10 bar band

static lv_obj_t * g_label_shot = NULL;
static lv_obj_t * g_switch_heater = NULL;
static TickType_t g_shot_start = 0;

static void heater_switch_event_cb(lv_event_t * e);

static void ui_apply_mode(void *unused)
{
  LV_UNUSED(unused);
  const char *text = "STANDBY";
  lv_color_t color = lv_color_hex(0x1E88E5); // blue

  if (g_heater_on) {
    if (g_shot_on) {
      text = "SHOT";
      color = lv_color_hex(0xFDD835); // yellow
    } else if (g_steam_on) {
      text = "STEAM";
      color = lv_color_hex(0xE53935); // red
    } else {
      text = "BREW";
      color = lv_color_hex(0x43A047); // green
    }
  }

  if (g_status_label) {
    lv_label_set_text(g_status_label, text);
  }
  lv_obj_set_style_bg_color(lv_scr_act(), color, 0);
}

void UI_SetStates(bool heater_on, bool steam_on, bool shot_on)
{
  g_heater_on = heater_on;
  g_steam_on = steam_on;
  // handle shot timer start/stop
  if (!g_shot_on && shot_on) {
    g_shot_start = xTaskGetTickCount();
  }
  g_shot_on = shot_on;
  lv_async_call(ui_apply_mode, NULL);
}

void UI_SetHeaterSwitch(bool on)
{
  if (g_switch_heater) {
    if (on) lv_obj_add_state(g_switch_heater, LV_STATE_CHECKED);
    else    lv_obj_clear_state(g_switch_heater, LV_STATE_CHECKED);
  }
}

void UI_UpdatePressure(float bar)
{
  if (g_press_needle && g_press_scale) {
    if (bar < 0) bar = 0;
    if (bar > 12) bar = 12;
    lv_meter_set_indicator_value(g_meter_press, g_press_needle, bar);
  }
}

void UI_UpdateTemp(float current_c, float set_c, bool steam_mode)
{
  // Adjust scale based on mode
  if (g_temp_scale) {
    if (steam_mode) lv_meter_set_scale_range(g_meter_temp, g_temp_scale, 110, 160, 270, 135);
    else            lv_meter_set_scale_range(g_meter_temp, g_temp_scale, 60, 110, 270, 135);
  }
  if (g_temp_needle) {
    lv_meter_set_indicator_value(g_meter_temp, g_temp_needle, current_c);
  }
  if (g_temp_set_band) {
    float start = set_c - 5.0f;
    float end = set_c + 5.0f;
    lv_meter_set_indicator_start_value(g_meter_temp, g_temp_set_band, start);
    lv_meter_set_indicator_end_value(g_meter_temp, g_temp_set_band, end);
  }
}


void Lvgl_Example1(void){

  if(LV_HOR_RES <= 320) disp_size = DISP_SMALL;             
  else if(LV_HOR_RES < 720) disp_size = DISP_MEDIUM;       
  else disp_size = DISP_LARGE;    
  font_large = LV_FONT_DEFAULT;                             
  font_normal = LV_FONT_DEFAULT;                         
  
  lv_coord_t tab_h;
  if(disp_size == DISP_LARGE) {
    tab_h = 70;
    #if LV_FONT_MONTSERRAT_24
      font_large     = &lv_font_montserrat_24;
    #else
      LV_LOG_WARN("LV_FONT_MONTSERRAT_24 is not enabled for the widgets demo. Using LV_FONT_DEFAULT instead.");
    #endif
    #if LV_FONT_MONTSERRAT_16
      font_normal    = &lv_font_montserrat_16;
    #else
      LV_LOG_WARN("LV_FONT_MONTSERRAT_16 is not enabled for the widgets demo. Using LV_FONT_DEFAULT instead.");
    #endif
  }
  else if(disp_size == DISP_MEDIUM) {
    tab_h = 45;
    #if LV_FONT_MONTSERRAT_20
      font_large     = &lv_font_montserrat_20;
    #else
        LV_LOG_WARN("LV_FONT_MONTSERRAT_20 is not enabled for the widgets demo. Using LV_FONT_DEFAULT instead.");
    #endif
    #if LV_FONT_MONTSERRAT_14
      font_normal    = &lv_font_montserrat_14;
    #else
      LV_LOG_WARN("LV_FONT_MONTSERRAT_14 is not enabled for the widgets demo. Using LV_FONT_DEFAULT instead.");
    #endif
  }
  else {   /* disp_size == DISP_SMALL */
    tab_h = 45;
    #if LV_FONT_MONTSERRAT_18
      font_large     = &lv_font_montserrat_18;
    #else
      LV_LOG_WARN("LV_FONT_MONTSERRAT_18 is not enabled for the widgets demo. Using LV_FONT_DEFAULT instead.");
    #endif
    #if LV_FONT_MONTSERRAT_12
      font_normal    = &lv_font_montserrat_12;
    #else
      LV_LOG_WARN("LV_FONT_MONTSERRAT_12 is not enabled for the widgets demo. Using LV_FONT_DEFAULT instead.");
    #endif
  }         // 设置字体
  
  lv_style_init(&style_text_muted);
  lv_style_set_text_opa(&style_text_muted, LV_OPA_90);

  lv_style_init(&style_title);
  lv_style_set_text_font(&style_title, font_large);

  lv_style_init(&style_icon);
  lv_style_set_text_color(&style_icon, lv_theme_get_color_primary(NULL));
  lv_style_set_text_font(&style_icon, font_large);

  lv_style_init(&style_bullet);
  lv_style_set_border_width(&style_bullet, 0);
  lv_style_set_radius(&style_bullet, LV_RADIUS_CIRCLE);

  tv = lv_tabview_create(lv_scr_act(), LV_DIR_TOP, tab_h);

  lv_obj_set_style_text_font(lv_scr_act(), font_normal, 0);

  if(disp_size == DISP_LARGE) {
    lv_obj_t * tab_btns = lv_tabview_get_tab_btns(tv);
    lv_obj_set_style_pad_left(tab_btns, LV_HOR_RES / 2, 0);
    #if LV_USE_DEMO_WIDGETS
    lv_obj_t * logo = lv_img_create(tab_btns);
    LV_IMG_DECLARE(img_lvgl_logo);
    lv_img_set_src(logo, &img_lvgl_logo);
    lv_obj_align(logo, LV_ALIGN_LEFT_MID, -LV_HOR_RES / 2 + 25, 0);
    #endif

    lv_obj_t * label = lv_label_create(tab_btns);
    lv_obj_add_style(label, &style_title, 0);
    lv_label_set_text(label, "LVGL v8");
    #if LV_USE_DEMO_WIDGETS
    lv_obj_align_to(label, logo, LV_ALIGN_OUT_RIGHT_TOP, 10, 0);
    #else
    lv_obj_align(label, LV_ALIGN_LEFT_MID, -LV_HOR_RES / 2 + 60, -12);
    #endif

    label = lv_label_create(tab_btns);
    #if LV_USE_DEMO_WIDGETS
    lv_label_set_text(label, "Widgets demo");
    #else
    lv_label_set_text(label, "LVGL");
    #endif
    lv_obj_add_style(label, &style_text_muted, 0);
    #if LV_USE_DEMO_WIDGETS
    lv_obj_align_to(label, logo, LV_ALIGN_OUT_RIGHT_BOTTOM, 10, 0);
    #else
    lv_obj_align(label, LV_ALIGN_LEFT_MID, -LV_HOR_RES / 2 + 60, 12);
    #endif
  }

  // Create a top status label
  g_status_label = lv_label_create(lv_scr_act());
  static lv_style_t style_big_title; lv_style_init(&style_big_title);
#if LV_FONT_MONTSERRAT_32
  lv_style_set_text_font(&style_big_title, &lv_font_montserrat_32);
#elif LV_FONT_MONTSERRAT_24
  lv_style_set_text_font(&style_big_title, &lv_font_montserrat_24);
#else
  lv_style_set_text_font(&style_big_title, LV_FONT_DEFAULT);
#endif
  lv_style_set_text_opa(&style_big_title, LV_OPA_COVER);
  lv_obj_add_style(g_status_label, &style_big_title, 0);
  lv_obj_align(g_status_label, LV_ALIGN_TOP_MID, 0, 6);
  lv_label_set_text(g_status_label, "STANDBY");

  lv_obj_t * t1 = lv_tabview_add_tab(tv, "");
  // lv_obj_t * t2 = lv_tabview_add_tab(tv, "Buzzer");
  // lv_obj_t * t3 = lv_tabview_add_tab(tv, "Shop");
  
  // lv_coord_t screen_width = lv_obj_get_width(lv_scr_act());
  // lv_obj_set_width(t1, screen_width);
  Onboard_create(t1);
  // Buzzer_create(t2);
  // shop_create(t3);

  // color_changer_create(tv);
}

static void led_event_cb(lv_event_t *e) {
    lv_obj_t *led = (lv_obj_t *)lv_event_get_user_data(e);
    lv_obj_t *sw = lv_event_get_target(e); 
    if (lv_obj_get_state(sw) & LV_STATE_CHECKED) {
      lv_led_on(led);
      Buzzer_On();
    } 
    else {
      lv_led_off(led);
      Buzzer_Off();
    }
}
// static void Buzzer_create(lv_obj_t * parent)
// {
//   lv_obj_t *label = lv_label_create(parent);
//   lv_label_set_text(label, "The buzzer tes");
//   lv_obj_set_size(label, LV_PCT(30), LV_PCT(5));
//   lv_obj_align(label, LV_ALIGN_CENTER, 0, -60);
//   lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0); 


//   lv_obj_t *led = lv_led_create(parent);
//   lv_obj_set_size(led, 50, 50);
//   lv_obj_align(led, LV_ALIGN_CENTER, -60, 0);
//   lv_led_off(led);

//   lv_obj_t *sw = lv_switch_create(parent);
//   lv_obj_align(sw, LV_ALIGN_CENTER, 60, 0);
//   lv_obj_add_event_cb(sw, led_event_cb, LV_EVENT_VALUE_CHANGED, led);
// }
void Lvgl_Example1_close(void)
{
  /*Delete all animation*/
  lv_anim_del(NULL, NULL);

  lv_timer_del(meter2_timer);
  meter2_timer = NULL;

  lv_obj_clean(lv_scr_act());

  lv_style_reset(&style_text_muted);
  lv_style_reset(&style_title);
  lv_style_reset(&style_icon);
  lv_style_reset(&style_bullet);
}


/**********************
*   STATIC FUNCTIONS
**********************/

static void Onboard_create(lv_obj_t * parent)
{
  // declare dummies to satisfy old unreachable code
  lv_obj_t * panel1 = NULL;
  // Replace demo UI with dashboard
  lv_obj_clean(parent);
  // Temperature meter
  g_meter_temp = lv_meter_create(parent);
  lv_obj_set_size(g_meter_temp, 220, 220);
  lv_obj_align(g_meter_temp, LV_ALIGN_LEFT_MID, 40, -10);
  g_temp_scale = lv_meter_add_scale(g_meter_temp);
  lv_meter_set_scale_ticks(g_meter_temp, g_temp_scale, 41, 2, 10, lv_palette_main(LV_PALETTE_GREY));
  lv_meter_set_scale_major_ticks(g_meter_temp, g_temp_scale, 5, 4, 15, lv_color_black(), 10);
  lv_meter_set_scale_range(g_meter_temp, g_temp_scale, 60, 110, 270, 135);
  g_temp_needle = lv_meter_add_needle_line(g_meter_temp, g_temp_scale, 4, lv_palette_main(LV_PALETTE_BLUE), -10);
  g_temp_set_band = lv_meter_add_arc(g_meter_temp, g_temp_scale, 10, lv_palette_main(LV_PALETTE_RED), 0);

  // Pressure meter
  g_meter_press = lv_meter_create(parent);
  lv_obj_set_size(g_meter_press, 220, 220);
  lv_obj_align(g_meter_press, LV_ALIGN_RIGHT_MID, -40, -10);
  g_press_scale = lv_meter_add_scale(g_meter_press);
  lv_meter_set_scale_ticks(g_meter_press, g_press_scale, 25, 2, 10, lv_palette_main(LV_PALETTE_GREY));
  lv_meter_set_scale_major_ticks(g_meter_press, g_press_scale, 5, 4, 15, lv_color_black(), 10);
  lv_meter_set_scale_range(g_meter_press, g_press_scale, 0, 12, 270, 135);
  g_press_needle = lv_meter_add_needle_line(g_meter_press, g_press_scale, 4, lv_palette_main(LV_PALETTE_BLUE), -10);
  g_press_crit = lv_meter_add_arc(g_meter_press, g_press_scale, 10, lv_palette_main(LV_PALETTE_RED), 0);
  lv_meter_set_indicator_start_value(g_meter_press, g_press_crit, 9);
  lv_meter_set_indicator_end_value(g_meter_press, g_press_crit, 10);

  // Shot time label
  g_label_shot = lv_label_create(parent);
  lv_obj_align(g_label_shot, LV_ALIGN_BOTTOM_MID, 0, -10);
  lv_label_set_text(g_label_shot, "Shot: 00.0s");

  // Heater switch
  g_switch_heater = lv_switch_create(parent);
  lv_obj_align(g_switch_heater, LV_ALIGN_BOTTOM_LEFT, 20, -10);
  lv_obj_add_event_cb(g_switch_heater, heater_switch_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
  lv_obj_t *lbl = lv_label_create(parent);
  lv_label_set_text(lbl, "Heater");
  lv_obj_align_to(lbl, g_switch_heater, LV_ALIGN_OUT_RIGHT_MID, 10, 0);

  auto_step_timer = lv_timer_create(example1_increase_lvgl_tick, 100, NULL);
  return;

  lv_obj_t * panel1_title = lv_label_create(panel1);
  lv_label_set_text(panel1_title, "Onboard parameter");
  lv_obj_add_style(panel1_title, &style_title, 0);

  lv_obj_t * SD_label = lv_label_create(panel1);
  lv_label_set_text(SD_label, "SD Card");
  lv_obj_add_style(SD_label, &style_text_muted, 0);

  SD_Size = lv_textarea_create(panel1);
  lv_textarea_set_one_line(SD_Size, true);
  lv_textarea_set_placeholder_text(SD_Size, "SD Size");
  lv_obj_add_event_cb(SD_Size, ta_event_cb, LV_EVENT_ALL, NULL);

  lv_obj_t * Flash_label = lv_label_create(panel1);
  lv_label_set_text(Flash_label, "Flash Size");
  lv_obj_add_style(Flash_label, &style_text_muted, 0);

  FlashSize = lv_textarea_create(panel1);
  lv_textarea_set_one_line(FlashSize, true);
  lv_textarea_set_placeholder_text(FlashSize, "Flash Size");
  lv_obj_add_event_cb(FlashSize, ta_event_cb, LV_EVENT_ALL, NULL);

  lv_obj_t * BAT_label = lv_label_create(panel1);
  lv_label_set_text(BAT_label, "Battery Voltage");
  lv_obj_add_style(BAT_label, &style_text_muted, 0);

  BAT_Volts = lv_textarea_create(panel1);
  lv_textarea_set_one_line(BAT_Volts, true);
  lv_textarea_set_placeholder_text(BAT_Volts, "BAT Volts");
  lv_obj_add_event_cb(BAT_Volts, ta_event_cb, LV_EVENT_ALL, NULL);

  lv_obj_t * angle_label = lv_label_create(panel1);
  lv_label_set_text(angle_label, "Angular deflection");
  lv_obj_add_style(angle_label, &style_text_muted, 0);

  Board_angle = lv_textarea_create(panel1);
  lv_textarea_set_one_line(Board_angle, true);
  lv_textarea_set_placeholder_text(Board_angle, "Board angle");
  lv_obj_add_event_cb(Board_angle, ta_event_cb, LV_EVENT_ALL, NULL);

  lv_obj_t * Time_label = lv_label_create(panel1);
  lv_label_set_text(Time_label, "RTC Time");
  lv_obj_add_style(Time_label, &style_text_muted, 0);

  RTC_Time = lv_textarea_create(panel1);
  lv_textarea_set_one_line(RTC_Time, true);
  lv_textarea_set_placeholder_text(RTC_Time, "Display time");
  lv_obj_add_event_cb(RTC_Time, ta_event_cb, LV_EVENT_ALL, NULL);

  lv_obj_t * Wireless_label = lv_label_create(panel1);
  lv_label_set_text(Wireless_label, "Wireless scan");
  lv_obj_add_style(Wireless_label, &style_text_muted, 0);

  Wireless_Scan = lv_textarea_create(panel1);
  lv_textarea_set_one_line(Wireless_Scan, true);
  lv_textarea_set_placeholder_text(Wireless_Scan, "Wireless number");
  lv_obj_add_event_cb(Wireless_Scan, ta_event_cb, LV_EVENT_ALL, NULL);

  lv_obj_t * Backlight_label = lv_label_create(panel1);
  lv_label_set_text(Backlight_label, "Backlight brightness");
  lv_obj_add_style(Backlight_label, &style_text_muted, 0);

  Backlight_slider = lv_slider_create(panel1);                                 
  lv_obj_add_flag(Backlight_slider, LV_OBJ_FLAG_CLICKABLE);    
  lv_obj_set_size(Backlight_slider, 200, 35);              
  lv_obj_set_style_radius(Backlight_slider, 3, LV_PART_KNOB);               // Adjust the value for more or less rounding                                            
  lv_obj_set_style_bg_opa(Backlight_slider, LV_OPA_TRANSP, LV_PART_KNOB);                               
  // lv_obj_set_style_pad_all(Backlight_slider, 0, LV_PART_KNOB);                                            
  lv_obj_set_style_bg_color(Backlight_slider, lv_color_hex(0xAAAAAA), LV_PART_KNOB);               
  lv_obj_set_style_bg_color(Backlight_slider, lv_color_hex(0xFFFFFF), LV_PART_INDICATOR);             
  lv_obj_set_style_outline_width(Backlight_slider, 2, LV_PART_INDICATOR);  
  lv_obj_set_style_outline_color(Backlight_slider, lv_color_hex(0xD3D3D3), LV_PART_INDICATOR);      
  lv_slider_set_range(Backlight_slider, 5, Backlight_MAX);              
  lv_slider_set_value(Backlight_slider, LCD_Backlight, LV_ANIM_ON);  
  lv_obj_add_event_cb(Backlight_slider, Backlight_adjustment_event_cb, LV_EVENT_VALUE_CHANGED, NULL);


  lv_obj_t * panel2 = lv_obj_create(parent);
  lv_obj_set_height(panel2, LV_SIZE_CONTENT);

  lv_obj_t * panel2_title = lv_label_create(panel2);
  lv_label_set_text(panel2_title, "The buzzer tes");
  lv_obj_add_style(panel2_title, &style_title, 0);

  lv_obj_t *led = lv_led_create(panel2);
  lv_obj_set_size(led, 50, 50);
  lv_obj_align(led, LV_ALIGN_CENTER, -60, 0);
  lv_led_off(led);

  lv_obj_t *sw = lv_switch_create(panel2);
  lv_obj_set_size(sw, 65, 40);
  lv_obj_align(sw, LV_ALIGN_CENTER, 60, 0);
  lv_obj_add_event_cb(sw, led_event_cb, LV_EVENT_VALUE_CHANGED, led);

/////////////////////////////////////////////////////////
  static lv_coord_t grid_main_col_dsc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
  static lv_coord_t grid_main_row_dsc[] = {LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};


  /*Create the top panel*/
  static lv_coord_t grid_1_col_dsc[] = {LV_GRID_FR(4),  LV_GRID_FR(1),  LV_GRID_FR(1),  LV_GRID_FR(1), LV_GRID_FR(4), LV_GRID_TEMPLATE_LAST};
  static lv_coord_t grid_1_row_dsc[] = {
    LV_GRID_CONTENT, /*Name*/
    LV_GRID_CONTENT, /*Description*/
    LV_GRID_CONTENT, /*Email*/
    LV_GRID_CONTENT, /*Email*/
    LV_GRID_TEMPLATE_LAST
  };

  static lv_coord_t grid_2_col_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(5), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
  static lv_coord_t grid_2_row_dsc[] = {
    LV_GRID_CONTENT,  /*Title*/
    5,                /*Separator*/
    LV_GRID_CONTENT,  /*Box title*/
    40,               /*Box*/
    LV_GRID_CONTENT,  /*Box title*/
    40,               /*Box*/
    LV_GRID_CONTENT,  /*Box title*/
    40,               /*Box*/
    LV_GRID_CONTENT,  /*Box title*/
    40,               /*Box*/
    LV_GRID_CONTENT,  /*Box title*/
    40,               /*Box*/
    LV_GRID_CONTENT,  /*Box title*/
    40,               /*Box*/
    LV_GRID_CONTENT,  /*Box title*/
    40,               /*Box*/
    LV_GRID_CONTENT,  /*Box title*/
    40,               /*Box*/
    LV_GRID_TEMPLATE_LAST
  };


  lv_obj_set_grid_dsc_array(parent, grid_main_col_dsc, grid_main_row_dsc);

  // old grid layout removed
}

void example1_increase_lvgl_tick(lv_timer_t * t)
{
  LV_UNUSED(t);
  // Update shot time if active
  if (g_shot_on && g_label_shot) {
    TickType_t now = xTaskGetTickCount();
    uint32_t ms = (now - g_shot_start) * portTICK_PERIOD_MS;
    char buf[32];
    snprintf(buf, sizeof(buf), "Shot: %lu.%lus", (unsigned long)(ms/1000), (unsigned long)((ms%1000)/100));
    lv_label_set_text(g_label_shot, buf);
  }
}





static void ta_event_cb(lv_event_t * e)
{
}

static void heater_switch_event_cb(lv_event_t * e)
{
  if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;
  lv_obj_t *sw = lv_event_get_target(e);
  bool on = lv_obj_has_state(sw, LV_STATE_CHECKED);
  // Publish to MQTT
  extern int MQTT_Publish(const char*, const char*, int, bool);
  char topic[96];
  snprintf(topic, sizeof(topic), "gaggia_classic/%s/heater/state", GAGGIA_ID);
  MQTT_Publish(topic, on ? "ON":"OFF", 1, true);
}



void Backlight_adjustment_event_cb(lv_event_t * e) {
  uint8_t Backlight = lv_slider_get_value(lv_event_get_target(e));  
  if (Backlight <= Backlight_MAX)  {
    lv_slider_set_value(Backlight_slider, Backlight, LV_ANIM_ON); 
    LCD_Backlight = Backlight;
    LVGL_Backlight_adjustment(Backlight);
  }
  else
    printf("Volume out of range: %d\n", Backlight);

}

void LVGL_Backlight_adjustment(uint8_t Backlight) {
  Set_Backlight(Backlight);                                 
}






